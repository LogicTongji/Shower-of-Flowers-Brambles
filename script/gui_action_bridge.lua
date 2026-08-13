local P = {}

P.Handlers = {}

function P.Register(actionName, handler)
	if type(actionName) ~= "string"
		or actionName == ""
		or type(handler) ~= "function" then
		return false
	end

	P.Handlers[actionName] = handler
	return true
end

function P.Unregister(actionName)
	if type(actionName) ~= "string" then
		return false
	end

	if P.Handlers[actionName] == nil then
		return false
	end

	P.Handlers[actionName] = nil
	return true
end

function P.Dispatch(actionName, payload)
	local handler = P.Handlers[actionName]
	if type(handler) ~= "function" then
		return false
	end

	payload = payload or {}
	payload.action = payload.action or actionName
	local success, result = pcall(handler, payload)
	if not success then
		return false
	end

	return result ~= false
end

_G.GuiActionBridge = P

return P
