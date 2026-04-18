local LrHttp = import 'LrHttp'
local LrStringUtils = import 'LrStringUtils'

local Json = require 'Json'

local ServiceClient = {
    host = 'http://127.0.0.1:6464',
}

local function encodeJson(payload)
    return LrStringUtils.toUTF8(Json.encode(payload))
end

local function decodeJson(payload)
    return Json.decode(payload)
end

function ServiceClient.getCapabilities()
    local body, headers = LrHttp.get(ServiceClient.host .. '/v1/capabilities')
    return decodeJson(body), headers
end

function ServiceClient.submitCritique(payload)
    local body, headers = LrHttp.post(ServiceClient.host .. '/v1/critique', encodeJson(payload), {
        { field = 'Content-Type', value = 'application/json' },
    })
    return decodeJson(body), headers
end

return ServiceClient
