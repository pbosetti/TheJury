local LrHttp = import 'LrHttp'
local LrPrefs = import 'LrPrefs'
local LrStringUtils = import 'LrStringUtils'

local Json = require 'Json'

local ServiceClient = {
    defaultHost = 'http://127.0.0.1:6464',
}

local function encodeJson(payload)
    local body = Json.encode(payload)
    if LrStringUtils.unicodeToUtf8 then
        return LrStringUtils.unicodeToUtf8(body)
    end
    return body
end

local function decodeJson(payload)
    if payload == nil or payload == '' then
        return nil, 'empty response from local service'
    end

    local ok, decoded = pcall(Json.decode, payload)
    if not ok then
        return nil, decoded
    end

    return decoded
end

local function describeHeaders(headers)
    if type(headers) ~= 'table' then
        return nil
    end

    local parts = {}

    if headers.status ~= nil then
        parts[#parts + 1] = 'status=' .. tostring(headers.status)
    end

    if type(headers.error) == 'table' then
        if headers.error.errorCode then
            parts[#parts + 1] = 'errorCode=' .. tostring(headers.error.errorCode)
        end
        if headers.error.name then
            parts[#parts + 1] = 'error=' .. tostring(headers.error.name)
        end
        if headers.error.nativeCode ~= nil then
            parts[#parts + 1] = 'nativeCode=' .. tostring(headers.error.nativeCode)
        end
    end

    if headers.partial_data ~= nil and headers.partial_data ~= '' then
        parts[#parts + 1] = 'partial_data=' .. tostring(headers.partial_data)
    end

    if #parts == 0 then
        return nil
    end

    return table.concat(parts, ', ')
end

local function getHost()
    local prefs = LrPrefs.prefsForPlugin()
    return prefs.serviceHost or ServiceClient.defaultHost
end

function ServiceClient.getCapabilities()
    local body, headers = LrHttp.get(getHost() .. '/v1/capabilities')
    if body == nil then
        return nil, describeHeaders(headers) or 'local service request failed', headers
    end
    local decoded, err = decodeJson(body)
    if decoded and decoded.error then
        return nil, decoded.message or decoded.error, headers
    end
    return decoded, err, headers
end

function ServiceClient.getConfig()
    local body, headers = LrHttp.get(getHost() .. '/v1/config')
    if body == nil then
        return nil, describeHeaders(headers) or 'local service request failed', headers
    end
    local decoded, err = decodeJson(body)
    if decoded and decoded.error then
        return nil, decoded.message or decoded.error, headers
    end
    return decoded, err, headers
end

function ServiceClient.updateConfig(payload)
    local body, headers = LrHttp.post(getHost() .. '/v1/config', encodeJson(payload), {
        { field = 'Content-Type', value = 'application/json' },
    }, 'PUT')
    if body == nil then
        return nil, describeHeaders(headers) or 'local service request failed', headers
    end
    local decoded, err = decodeJson(body)
    if decoded and decoded.error then
        return nil, decoded.message or decoded.error, headers
    end
    return decoded, err, headers
end

function ServiceClient.submitCritique(payload)
    local body, headers = LrHttp.post(getHost() .. '/v1/critique', encodeJson(payload), {
        { field = 'Content-Type', value = 'application/json' },
    })
    if body == nil then
        return nil, describeHeaders(headers) or 'local service request failed', headers
    end
    local decoded, err = decodeJson(body)
    if decoded and decoded.error then
        return nil, decoded.message or decoded.error, headers
    end
    return decoded, err, headers
end

return ServiceClient
