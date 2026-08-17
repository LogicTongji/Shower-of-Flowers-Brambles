local P = {}

local GuiDataBridge = require('gui_data_bridge')

P.Version = 1
P.RegistryModule = "scripted_gui_plugins"
P.Plugins = {}
P.PluginOrder = {}
P.Diagnostics = {}
P.TickSerial = 0
P.Initialized = false
P.InitializationError = nil

local function NormalizeName(value)
	if type(value) ~= "string" then
		return ""
	end
	return string.lower(value)
end

local function Log(message)
	message = "ScriptedGuiRuntime: " .. tostring(message)
	if type(Utils) == "table"
		and type(Utils.LUA_DEBUGOUT) == "function" then
		pcall(Utils.LUA_DEBUGOUT, message)
	end
end

local function SafeTag(value)
	local success, result = pcall(tostring, value)
	return success and result or ""
end

local function BuildContext(minister)
	local context = {
		minister = minister,
		ministerTagObject = nil,
		ministerAI = nil,
		ministerTag = "",
		playerTagObject = nil,
		playerTag = "",
		currentDay = 0,
		tick = P.TickSerial
	}

	pcall(function()
		context.ministerTagObject = minister:GetCountryTag()
		context.ministerTag = SafeTag(context.ministerTagObject)
	end)
	pcall(function()
		context.ministerAI = minister:GetOwnerAI()
	end)
	pcall(function()
		context.playerTagObject = CCurrentGameState.GetPlayer()
		context.playerTag = SafeTag(context.playerTagObject)
	end)
	pcall(function()
		context.currentDay =
			CCurrentGameState.GetCurrentDate():GetTotalDays()
	end)
	context.isPlayerContext = context.playerTag ~= ""
		and context.ministerTag == context.playerTag
	return context
end

local function ValidateDefinition(definition)
	if type(definition) ~= "table" then
		return nil, "plugin_definition_not_table"
	end

	local normalized = {}
	normalized.id = NormalizeName(definition.id)
	normalized.channel = NormalizeName(
		definition.channel or definition.id
	)
	normalized.moduleName = definition.module
	normalized.scope = NormalizeName(
		definition.scope or "player_preferred"
	)
	normalized.playerPriority = math.max(
		0,
		math.floor(tonumber(definition.playerPriority) or 100)
	)
	normalized.fallbackPriority = math.max(
		0,
		math.floor(tonumber(definition.fallbackPriority) or 0)
	)
	normalized.maxConsecutiveErrors = math.max(
		1,
		math.floor(tonumber(definition.maxConsecutiveErrors) or 3)
	)
	normalized.errorCooldownTicks = math.max(
		1,
		math.floor(tonumber(definition.errorCooldownTicks) or 16)
	)
	normalized.actionBudget = math.max(
		1,
		math.floor(tonumber(definition.actionBudget) or 64)
	)
	normalized.refreshMode = NormalizeName(
		definition.refreshMode or "always"
	)
	normalized.refreshIntervalTicks = math.max(
		1,
		math.floor(tonumber(definition.refreshIntervalTicks) or 1)
	)
	normalized.enabled = definition.enabled ~= false
	normalized.priority = tonumber(definition.priority) or 0

	if normalized.id == "" then
		return nil, "plugin_id_missing"
	end
	if normalized.channel == "" then
		return nil, "plugin_channel_missing:" .. normalized.id
	end
	if type(normalized.moduleName) ~= "string"
		or normalized.moduleName == "" then
		return nil, "plugin_module_missing:" .. normalized.id
	end
	if normalized.scope ~= "all"
		and normalized.scope ~= "player_only"
		and normalized.scope ~= "player_preferred" then
		return nil, "plugin_scope_invalid:" .. normalized.id
	end
	if normalized.refreshMode ~= "always"
		and normalized.refreshMode ~= "daily"
		and normalized.refreshMode ~= "interval"
		and normalized.refreshMode ~= "manual" then
		return nil, "plugin_refresh_mode_invalid:" .. normalized.id
	end

	return normalized
end

local function RecordError(plugin, phase, message)
	plugin.consecutiveErrors = plugin.consecutiveErrors + 1
	plugin.totalErrors = plugin.totalErrors + 1
	plugin.lastError = tostring(message)
	plugin.lastErrorPhase = phase
	P.Diagnostics[plugin.id] = {
		phase = phase,
		message = plugin.lastError,
		consecutiveErrors = plugin.consecutiveErrors,
		totalErrors = plugin.totalErrors,
		tick = P.TickSerial
	}

	if plugin.lastLoggedError ~= plugin.lastError
		or plugin.consecutiveErrors == 1 then
		plugin.lastLoggedError = plugin.lastError
		Log(plugin.id .. " [" .. phase .. "]: " .. plugin.lastError)
	end

	if plugin.consecutiveErrors >= plugin.maxConsecutiveErrors then
		plugin.retryAfterTick = P.TickSerial
			+ plugin.errorCooldownTicks
		plugin.consecutiveErrors = 0
		plugin.ownsChannel = false
		GuiDataBridge.ReleaseChannel(plugin.channel)
	end
end

local function CallPlugin(plugin, phase, callback, ...)
	if type(callback) ~= "function" then
		return true, nil
	end
	local success, result = pcall(callback, ...)
	if not success then
		RecordError(plugin, phase, result)
		return false, nil
	end
	plugin.consecutiveErrors = 0
	return true, result
end

local function ResolvePriority(plugin, context)
	if plugin.scope == "player_only" then
		if not context.isPlayerContext then
			return nil
		end
		return plugin.playerPriority
	end
	if plugin.scope == "player_preferred" then
		return context.isPlayerContext
			and plugin.playerPriority
			or plugin.fallbackPriority
	end
	return plugin.fallbackPriority
end

function P.Register(definition)
	local plugin, errorMessage = ValidateDefinition(definition)
	if not plugin then
		return false, errorMessage
	end
	if P.Plugins[plugin.id] then
		return false, "plugin_id_duplicate:" .. plugin.id
	end

	local success, module = pcall(require, plugin.moduleName)
	if not success or type(module) ~= "table" then
		return false, "plugin_module_load_failed:"
			.. plugin.id .. ":" .. tostring(module)
	end
	if type(module.Tick) ~= "function"
		and type(module.BuildUpdate) ~= "function" then
		return false, "plugin_refresh_callback_missing:" .. plugin.id
	end

	plugin.module = module
	plugin.tick = module.Tick
	plugin.pumpActions = module.PumpActions
	plugin.buildUpdate = module.BuildUpdate
	plugin.publishUpdate = module.PublishUpdate
	plugin.onUpdatePublished = module.OnUpdatePublished
	plugin.restoreState = module.RestoreState
	plugin.persistState = module.PersistState
	plugin.shouldTick = module.ShouldTick
	plugin.shouldRefresh = module.ShouldRefresh
	plugin.onPublisherAcquired = module.OnPublisherAcquired
	plugin.onPublisherLost = module.OnPublisherLost
	plugin.onShutdown = module.Shutdown
	plugin.ownsChannel = false
	plugin.stateOrdinal = 0
	plugin.consecutiveErrors = 0
	plugin.totalErrors = 0
	plugin.retryAfterTick = 0
	plugin.lastError = nil
	plugin.lastLoggedError = nil
	plugin.lastRefreshDay = nil
	plugin.lastRefreshTick = 0
	plugin.dirty = true
	plugin.lastContext = nil

	P.Plugins[plugin.id] = plugin
	table.insert(P.PluginOrder, plugin)
	table.sort(P.PluginOrder, function(first, second)
		if first.priority == second.priority then
			return first.id < second.id
		end
		return first.priority > second.priority
	end)
	return true
end

function P.Initialize(registryModule)
	if P.Initialized then
		return true
	end

	registryModule = registryModule or P.RegistryModule
	local success, registry = pcall(require, registryModule)
	if not success or type(registry) ~= "table"
		or type(registry.plugins) ~= "table" then
		P.InitializationError = "registry_load_failed:"
			.. tostring(registry)
		Log(P.InitializationError)
		return false
	end

	local registeredCount = 0
	for index, definition in ipairs(registry.plugins) do
		local registered, errorMessage = P.Register(definition)
		if not registered then
			P.Diagnostics["registry_" .. tostring(index)] = {
				phase = "register",
				message = errorMessage,
				tick = P.TickSerial
			}
			Log(errorMessage)
		else
			registeredCount = registeredCount + 1
		end
	end
	if registeredCount == 0 then
		P.InitializationError = "registry_contains_no_valid_plugins"
		Log(P.InitializationError)
		return false
	end

	P.Initialized = true
	P.InitializationError = nil
	return true
end

local function CopyContext(source)
	local result = {}
	for key, value in pairs(source) do
		result[key] = value
	end
	return result
end

local function IsRefreshDue(plugin, context)
	if plugin.dirty then
		return true
	end
	if type(plugin.shouldRefresh) == "function" then
		local valid, result = CallPlugin(
			plugin,
			"should_refresh",
			plugin.shouldRefresh,
			context
		)
		return valid and result ~= false
	end
	if plugin.refreshMode == "manual" then
		return false
	end
	if plugin.refreshMode == "daily" then
		return plugin.lastRefreshDay ~= context.currentDay
	end
	if plugin.refreshMode == "interval" then
		return P.TickSerial - plugin.lastRefreshTick
			>= plugin.refreshIntervalTicks
	end
	return true
end

local function PumpPluginActions(plugin, context)
	if type(plugin.pumpActions) ~= "function" then
		return true, false
	end
	local valid, result = CallPlugin(
		plugin,
		"pump_actions",
		plugin.pumpActions,
		context,
		plugin.actionBudget
	)
	local changed = valid and (result == true
		or (type(result) == "number" and result > 0))
	if changed then
		plugin.dirty = true
	end
	return valid, changed
end

local function PublishPluginUpdate(plugin, context, update)
	if type(update) ~= "table" then
		plugin.dirty = false
		return true
	end

	local published = false
	if type(plugin.publishUpdate) == "function" then
		local valid, result = CallPlugin(
			plugin,
			"publish_update",
			plugin.publishUpdate,
			update,
			context
		)
		if not valid then
			return false
		end
		published = result == true
	else
		published = update.fullSnapshot == false
			and GuiDataBridge.PublishDelta(plugin.channel, update)
			or GuiDataBridge.PublishSnapshot(plugin.channel, update)
	end
	if not published then
		RecordError(plugin, "publish_update", "native_publish_rejected")
		return false
	end

	plugin.dirty = false
	plugin.lastRefreshDay = context.currentDay
	plugin.lastRefreshTick = P.TickSerial
	CallPlugin(
		plugin,
		"update_published",
		plugin.onUpdatePublished,
		update,
		context
	)
	return true
end

local function RefreshPlugin(plugin, context)
	if not IsRefreshDue(plugin, context) then
		return true
	end
	if type(plugin.buildUpdate) == "function" then
		local valid, update = CallPlugin(
			plugin,
			"build_update",
			plugin.buildUpdate,
			context
		)
		if not valid then
			return false
		end
		return PublishPluginUpdate(plugin, context, update)
	end

	local valid = CallPlugin(
		plugin,
		"tick",
		plugin.tick,
		context
	)
	if valid then
		plugin.dirty = false
		plugin.lastRefreshDay = context.currentDay
		plugin.lastRefreshTick = P.TickSerial
	end
	return valid
end

function P.Tick(minister)
	if not P.Initialize() then
		return false
	end

	P.TickSerial = P.TickSerial + 1
	local baseContext = BuildContext(minister)
	for _, plugin in ipairs(P.PluginOrder) do
		if plugin.enabled and P.TickSerial >= plugin.retryAfterTick then
			local context = CopyContext(baseContext)
			local priority = ResolvePriority(plugin, context)
			if priority ~= nil then
				local shouldTick = true
				if type(plugin.shouldTick) == "function" then
					local valid, result = CallPlugin(
						plugin,
						"should_tick",
						plugin.shouldTick,
						context
					)
					shouldTick = valid and result ~= false
				end

				if shouldTick then
					local acquired, status, stateOrdinal =
						GuiDataBridge.TryAcquireChannel(
							plugin.channel,
							priority
						)
					if acquired then
						local becameOwner = not plugin.ownsChannel
							or status == "claimed"
							or status == "taken_over"
						plugin.ownsChannel = true
						plugin.stateOrdinal = stateOrdinal
						context.publisherStatus = status
						context.publisherAcquired = becameOwner
						context.publisherStateOrdinal = stateOrdinal
						context.actionBudget = plugin.actionBudget
						context.requestRefresh = function()
							plugin.dirty = true
						end
						plugin.lastContext = context
						if becameOwner then
							CallPlugin(
								plugin,
								"publisher_acquired",
								plugin.onPublisherAcquired,
								context
							)
							CallPlugin(
								plugin,
								"restore_state",
								plugin.restoreState,
								context
							)
						end
						local actionsValid, actionsChanged =
							PumpPluginActions(plugin, context)
						if actionsValid and actionsChanged then
							CallPlugin(
								plugin,
								"persist_state",
								plugin.persistState,
								context
							)
						end
						if actionsValid then
							RefreshPlugin(plugin, context)
						end
					elseif plugin.ownsChannel then
						CallPlugin(
							plugin,
							"persist_state",
							plugin.persistState,
							context
						)
						plugin.ownsChannel = false
						CallPlugin(
							plugin,
							"publisher_lost",
							plugin.onPublisherLost,
							context
						)
					end
				end
			end
		end
	end
	return true
end

function P.Shutdown()
	for _, plugin in ipairs(P.PluginOrder) do
		CallPlugin(
			plugin,
			"persist_state",
			plugin.persistState,
			plugin.lastContext
		)
		if plugin.ownsChannel then
			GuiDataBridge.ReleaseChannel(plugin.channel)
			plugin.ownsChannel = false
		end
		CallPlugin(plugin, "shutdown", plugin.onShutdown)
	end
	return true
end

function P.GetDiagnostics()
	return P.Diagnostics
end

_G.ScriptedGuiRuntime = P

return P
