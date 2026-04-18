local Json = {}

local function encodeString(value)
    local replacements = {
        ['\\'] = '\\\\',
        ['"'] = '\\"',
        ['\b'] = '\\b',
        ['\f'] = '\\f',
        ['\n'] = '\\n',
        ['\r'] = '\\r',
        ['\t'] = '\\t',
    }

    return '"' .. value:gsub('[\\"%z\1-\31]', function(character)
        return replacements[character] or string.format('\\u%04x', character:byte())
    end) .. '"'
end

local function isArray(value)
    local expectedIndex = 1
    for key, _ in pairs(value) do
        if type(key) ~= 'number' or key ~= expectedIndex then
            return false
        end
        expectedIndex = expectedIndex + 1
    end
    return true
end

local function encodeValue(value)
    local kind = type(value)
    if kind == 'nil' then
        return 'null'
    elseif kind == 'boolean' or kind == 'number' then
        return tostring(value)
    elseif kind == 'string' then
        return encodeString(value)
    elseif kind == 'table' then
        if isArray(value) then
            local items = {}
            for index = 1, #value do
                items[#items + 1] = encodeValue(value[index])
            end
            return '[' .. table.concat(items, ',') .. ']'
        end

        local fields = {}
        for key, fieldValue in pairs(value) do
            fields[#fields + 1] = encodeString(key) .. ':' .. encodeValue(fieldValue)
        end
        table.sort(fields)
        return '{' .. table.concat(fields, ',') .. '}'
    end

    error('unsupported JSON value type: ' .. kind)
end

function Json.encode(value)
    return encodeValue(value)
end

local function decodeError(position, message)
    error(string.format('JSON decode error at position %d: %s', position, message))
end

local function decode(text)
    local position = 1

    local function skipWhitespace()
        while true do
            local character = text:sub(position, position)
            if character == '' or not character:match('%s') then
                return
            end
            position = position + 1
        end
    end

    local parseValue

    local function parseString()
        if text:sub(position, position) ~= '"' then
            decodeError(position, 'expected string')
        end
        position = position + 1
        local result = {}
        while position <= #text do
            local character = text:sub(position, position)
            if character == '"' then
                position = position + 1
                return table.concat(result)
            end
            if character == '\\' then
                position = position + 1
                local escape = text:sub(position, position)
                local escaped = ({
                    ['"'] = '"',
                    ['\\'] = '\\',
                    ['/'] = '/',
                    ['b'] = '\b',
                    ['f'] = '\f',
                    ['n'] = '\n',
                    ['r'] = '\r',
                    ['t'] = '\t',
                })[escape]
                if escaped then
                    result[#result + 1] = escaped
                    position = position + 1
                elseif escape == 'u' then
                    local hex = text:sub(position + 1, position + 4)
                    if not hex:match('%x%x%x%x') then
                        decodeError(position, 'invalid unicode escape')
                    end
                    result[#result + 1] = utf8.char(tonumber(hex, 16))
                    position = position + 5
                else
                    decodeError(position, 'invalid escape sequence')
                end
            else
                result[#result + 1] = character
                position = position + 1
            end
        end
        decodeError(position, 'unterminated string')
    end

    local function parseNumber()
        local startPosition = position
        while text:sub(position, position):match('[%d%+%-%e%E%.]') do
            position = position + 1
        end
        local number = tonumber(text:sub(startPosition, position - 1))
        if number == nil then
            decodeError(startPosition, 'invalid number')
        end
        return number
    end

    local function parseLiteral(literal, value)
        if text:sub(position, position + #literal - 1) ~= literal then
            decodeError(position, 'expected ' .. literal)
        end
        position = position + #literal
        return value
    end

    local function parseArray()
        position = position + 1
        skipWhitespace()
        local result = {}
        if text:sub(position, position) == ']' then
            position = position + 1
            return result
        end

        while true do
            result[#result + 1] = parseValue()
            skipWhitespace()
            local character = text:sub(position, position)
            if character == ']' then
                position = position + 1
                return result
            end
            if character ~= ',' then
                decodeError(position, 'expected comma in array')
            end
            position = position + 1
            skipWhitespace()
        end
    end

    local function parseObject()
        position = position + 1
        skipWhitespace()
        local result = {}
        if text:sub(position, position) == '}' then
            position = position + 1
            return result
        end

        while true do
            local key = parseString()
            skipWhitespace()
            if text:sub(position, position) ~= ':' then
                decodeError(position, 'expected colon in object')
            end
            position = position + 1
            skipWhitespace()
            result[key] = parseValue()
            skipWhitespace()
            local character = text:sub(position, position)
            if character == '}' then
                position = position + 1
                return result
            end
            if character ~= ',' then
                decodeError(position, 'expected comma in object')
            end
            position = position + 1
            skipWhitespace()
        end
    end

    function parseValue()
        skipWhitespace()
        local character = text:sub(position, position)
        if character == '{' then
            return parseObject()
        elseif character == '[' then
            return parseArray()
        elseif character == '"' then
            return parseString()
        elseif character == '-' or character:match('%d') then
            return parseNumber()
        elseif character == 't' then
            return parseLiteral('true', true)
        elseif character == 'f' then
            return parseLiteral('false', false)
        elseif character == 'n' then
            return parseLiteral('null', nil)
        end
        decodeError(position, 'unexpected token')
    end

    local result = parseValue()
    skipWhitespace()
    if position <= #text then
        decodeError(position, 'trailing data')
    end
    return result
end

function Json.decode(text)
    return decode(text)
end

return Json
