local P = {}

local GuiActionBridge = require('gui_action_bridge')

P.ProtocolVersion = 2
P.Channels = {}
P.Native = nil

local function GetNative()
	if type(P.Native) == "table" then
		return P.Native
	end

	local native = rawget(_G, 'ScriptedGuiNative')
	if type(native) == "table" then
		return native
	end

	return nil
end

local function IsChannelName(value)
	return type(value) == "string" and value ~= ""
end

local function CopyTable(source)
	local result = {}

	if type(source) == "table" then
		for key, value in pairs(source) do
			result[key] = value
		end
	end

	return result
end

local function GetChannel(channelName)
	local channel = P.Channels[channelName]
	if not channel then
		channel = {
			revision = 0
		}
		P.Channels[channelName] = channel
	end

	return channel
end

local function NormalizeUpdate(channelName, update)
	if not IsChannelName(channelName)
		or type(update) ~= "table" then
		return nil
	end

	local channel = GetChannel(channelName)
	local normalized = CopyTable(update)
	normalized.version = tonumber(normalized.version)
		or P.ProtocolVersion
	normalized.revision = math.floor(
		tonumber(normalized.revision)
		or (channel.revision + 1)
	)
	normalized.fullSnapshot = normalized.fullSnapshot == true
	normalized.baseRevision = math.floor(
		tonumber(normalized.baseRevision)
		or (normalized.fullSnapshot and 0 or channel.revision)
	)
	normalized.values = CopyTable(normalized.values)
	normalized.lists = CopyTable(normalized.lists)
	normalized.removedValues = normalized.removedValues or {}
	normalized.removedLists = normalized.removedLists or {}

	if normalized.revision <= 0 then
		return nil
	end
	if normalized.fullSnapshot then
		if normalized.baseRevision ~= 0 then
			return nil
		end
	elseif channel.revision == 0
		or normalized.baseRevision ~= channel.revision
		or normalized.revision <= normalized.baseRevision then
		return nil
	end

	return normalized
end

function P.SetNative(native)
	if native ~= nil and type(native) ~= "table" then
		return false
	end

	P.Native = native
	return true
end

function P.Reset(channelName)
	if channelName == nil then
		P.Channels = {}
		return true
	end
	if not IsChannelName(channelName) then
		return false
	end

	P.Channels[channelName] = nil
	return true
end

function P.TryAcquireChannel(channelName, priority)
	local native = GetNative()
	if not IsChannelName(channelName)
		or not native
		or type(native.TryAcquireChannel) ~= "function" then
		return false, "native_acquire_unavailable", 0
	end

	priority = math.max(
		0,
		math.floor(tonumber(priority) or 0)
	)
	local success, acquired, status, stateOrdinal = pcall(
		native.TryAcquireChannel,
		channelName,
		priority
	)
	if not success or acquired ~= true then
		return false, tostring(status or "rejected"),
			tonumber(stateOrdinal) or 0
	end

	status = tostring(status or "owned")
	if status == "claimed" or status == "taken_over" then
		GetChannel(channelName).revision = 0
	end
	return true, status, tonumber(stateOrdinal) or 0
end

function P.ReleaseChannel(channelName)
	local native = GetNative()
	if not IsChannelName(channelName)
		or not native
		or type(native.ReleaseChannel) ~= "function" then
		return false
	end

	local success, released = pcall(
		native.ReleaseChannel,
		channelName
	)
	if success and released == true then
		P.Reset(channelName)
		return true
	end
	return false
end

function P.PublishUpdate(channelName, update)
	local normalized = NormalizeUpdate(channelName, update)
	local native = GetNative()
	if not normalized
		or not native
		or type(native.PublishUpdate) ~= "function" then
		return false
	end

	local success, accepted = pcall(
		native.PublishUpdate,
		channelName,
		normalized
	)
	if not success or accepted ~= true then
		return false
	end

	GetChannel(channelName).revision = normalized.revision
	return true
end

function P.PublishSnapshot(channelName, snapshot)
	local update = CopyTable(snapshot)
	update.fullSnapshot = true
	update.baseRevision = 0
	return P.PublishUpdate(channelName, update)
end

function P.PublishDelta(channelName, delta)
	local update = CopyTable(delta)
	local channel = GetChannel(channelName)
	update.fullSnapshot = false
	update.baseRevision = channel.revision
	update.revision = tonumber(update.revision)
		or (channel.revision + 1)
	return P.PublishUpdate(channelName, update)
end

function P.TryPopAction(channelName)
	local native = GetNative()
	if not IsChannelName(channelName)
		or not native
		or type(native.TryPopAction) ~= "function" then
		return nil
	end

	local success, action = pcall(
		native.TryPopAction,
		channelName
	)
	if not success or type(action) ~= "table" then
		return nil
	end

	return action
end

function P.DispatchActions(channelName, maximumActions)
	maximumActions = math.max(
		1,
		math.floor(tonumber(maximumActions) or 64)
	)

	local dispatched = 0
	for _ = 1, maximumActions do
		local action = P.TryPopAction(channelName)
		if not action then
			break
		end

		local functionName = action.functionName
		if type(functionName) ~= "string"
			or functionName == "" then
			functionName = action.action
		end

		if GuiActionBridge.Dispatch(functionName, action) then
			dispatched = dispatched + 1
		end
	end

	return dispatched
end

function P.Tick(channelName, producer, maximumActions)
	local published = false
	if type(producer) == "function" then
		local success, update = pcall(producer)
		if success and type(update) == "table" then
			published = update.fullSnapshot == false
				and P.PublishDelta(channelName, update)
				or P.PublishSnapshot(channelName, update)
		end
	end

	return published, P.DispatchActions(
		channelName,
		maximumActions
	)
end

_G.GuiDataBridge = P

return P
