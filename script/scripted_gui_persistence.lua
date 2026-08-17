local P = {}

P.Version = 1
P.Prefix = "sgui_"
P.LastWritten = {}
P.LastReadError = ""
P.LastWriteError = ""

local function NormalizeName(value)
	value = string.lower(tostring(value or ""))
	value = value:gsub("[^%w_]", "_")
	value = value:gsub("_+", "_")
	return value
end

local function FullName(namespace, key)
	local normalizedNamespace = NormalizeName(namespace)
	local normalizedKey = NormalizeName(key)
	if normalizedNamespace == "" or normalizedKey == "" then
		return nil
	end
	return P.Prefix .. normalizedNamespace .. "_" .. normalizedKey
end

local function FixedPointValue(value)
	if type(value) == "number" then
		return value
	end
	local success, result = pcall(function()
		return value:Get()
	end)
	if success then
		return tonumber(result) or 0
	end
	return tonumber(value) or 0
end

local function ResolveContext(context)
	if type(context) ~= "table" or not context.minister then
		return nil
	end
	local success, resolved = pcall(function()
		local tag = context.persistenceTagObject
			or context.ministerTagObject
			or context.minister:GetCountryTag()
		local country = context.persistenceCountry
			or tag:GetCountry()
		local ai = context.persistenceAI
			or context.ministerAI
			or context.minister:GetOwnerAI()
		return {
			tag = tag,
			tagName = tostring(tag),
			country = country,
			ai = ai
		}
	end)
	return success and resolved or nil
end

local function VariableKey(name)
	local success, result = pcall(function()
		return CString(name)
	end)
	if success then
		return result
	end
	return name
end

local function FixedPointArgument(value)
	local success, result = pcall(function()
		return CFixedPoint(value)
	end)
	if success then
		return result
	end
	return value
end

function P.ReadNumber(context, namespace, key, defaultValue)
	local name = FullName(namespace, key)
	local resolved = ResolveContext(context)
	if not name or not resolved then
		P.LastReadError = "persistence_read_context_unavailable"
		return tonumber(defaultValue) or 0, false
	end

	local success, value = pcall(function()
		local variables = resolved.country:GetVariables()
		local variableKey = VariableKey(name)
		local readSuccess, result = pcall(function()
			return variables:GetVariable(variableKey)
		end)
		if not readSuccess then
			result = variables:GetVariable(name)
		end
		return FixedPointValue(result)
	end)
	if not success then
		P.LastReadError = tostring(value)
		return tonumber(defaultValue) or 0, false
	end
	P.LastReadError = ""
	return tonumber(value) or tonumber(defaultValue) or 0, true
end

function P.WriteNumber(context, namespace, key, value)
	local name = FullName(namespace, key)
	local resolved = ResolveContext(context)
	value = tonumber(value)
	if not name or not resolved or not value then
		P.LastWriteError = "persistence_write_context_unavailable"
		return false
	end

	local cacheKey = resolved.tagName .. ":" .. name
	if P.LastWritten[cacheKey] == value then
		return true
	end
	local success, errorMessage = pcall(function()
		local command = CSetVariableCommand(
			resolved.tag,
			VariableKey(name),
			FixedPointArgument(value)
		)
		resolved.ai:Post(command)
	end)
	if success then
		P.LastWritten[cacheKey] = value
		P.LastWriteError = ""
	else
		P.LastWriteError = tostring(errorMessage)
	end
	return success
end

function P.ReadBoolean(context, namespace, key, defaultValue)
	local value, available = P.ReadNumber(
		context,
		namespace,
		key,
		defaultValue and 1 or 0
	)
	return value >= 0.5, available
end

function P.WriteBoolean(context, namespace, key, value)
	return P.WriteNumber(
		context,
		namespace,
		key,
		value and 1 or 0
	)
end

function P.ReadProfileKey(context, namespace)
	local token, available = P.ReadNumber(
		context,
		namespace,
		"profile",
		0
	)
	token = math.floor(tonumber(token) or 0)
	if not available or token <= 0 then
		return "", available, token
	end
	return tostring(context.playerTag or "")
		.. ":" .. tostring(token), true, token
end

function P.EnsureProfileKey(context, namespace)
	local profileKey, _, token = P.ReadProfileKey(context, namespace)
	local created = false
	local written = true
	if token <= 0 then
		created = true
		local randomValue = 0
		pcall(function()
			randomValue = tonumber(CCurrentGameState.GetAIRand()) or 0
		end)
		local day = math.floor(tonumber(context.currentDay) or 0)
		local tagHash = 0
		for index = 1, #(context.playerTag or "") do
			tagHash = (tagHash * 33
				+ string.byte(context.playerTag, index)) % 1000000
		end
		token = (math.abs(randomValue) + day * 97 + tagHash)
			% 999999 + 1
		written = P.WriteNumber(
			context,
			namespace,
			"profile",
			token
		)
		profileKey = tostring(context.playerTag or "")
			.. ":" .. tostring(token)
	end
	return profileKey, token, created, written
end

function P.ResetWriteCache()
	P.LastWritten = {}
	P.LastReadError = ""
	P.LastWriteError = ""
end

function P.GetLastError()
	if P.LastWriteError ~= "" then
		return P.LastWriteError
	end
	return P.LastReadError
end

_G.ScriptedGuiPersistence = P

return P
