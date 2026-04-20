local LrBinding = import 'LrBinding'
local LrDialogs = import 'LrDialogs'
local LrFileUtils = import 'LrFileUtils'
local LrFunctionContext = import 'LrFunctionContext'
local LrTasks = import 'LrTasks'
local LrView = import 'LrView'

local Json = require 'Json'
local Utils = require 'Utils'

local bind = LrView.bind

local function isArray(value)
    if type(value) ~= 'table' then
        return false
    end

    local expectedIndex = 1
    for key, _ in pairs(value) do
        if type(key) ~= 'number' or key ~= expectedIndex then
            return false
        end
        expectedIndex = expectedIndex + 1
    end

    return true
end

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

local function encodePrettyJson(value, indentLevel)
    indentLevel = indentLevel or 0
    local indent = string.rep('  ', indentLevel)
    local nextIndent = string.rep('  ', indentLevel + 1)
    local kind = type(value)

    if kind == 'nil' then
        return 'null'
    end
    if kind == 'boolean' or kind == 'number' then
        return tostring(value)
    end
    if kind == 'string' then
        return encodeString(value)
    end
    if kind ~= 'table' then
        return encodeString(tostring(value))
    end

    if isArray(value) then
        if #value == 0 then
            return '[]'
        end

        local items = {}
        for index = 1, #value do
            items[#items + 1] = nextIndent .. encodePrettyJson(value[index], indentLevel + 1)
        end
        return '[\n' .. table.concat(items, ',\n') .. '\n' .. indent .. ']'
    end

    local keys = {}
    for key, _ in pairs(value) do
        keys[#keys + 1] = key
    end
    table.sort(keys, function(left, right)
        return tostring(left) < tostring(right)
    end)

    if #keys == 0 then
        return '{}'
    end

    local fields = {}
    for _, key in ipairs(keys) do
        fields[#fields + 1] = nextIndent .. encodeString(tostring(key)) .. ': ' ..
            encodePrettyJson(value[key], indentLevel + 1)
    end
    return '{\n' .. table.concat(fields, ',\n') .. '\n' .. indent .. '}'
end

local function selectedPhoto()
    local photos = Utils.getSelectedPhotos()
    if not photos or #photos == 0 then
        return nil
    end
    return photos[1]
end

local function sidecarPathForPhoto(photo)
    local metadata = Utils.collectRequestMetadata(photo)
    return Utils.critiqueSidecarPath(metadata.original_path)
end

local function readSidecar(path)
    local contents = LrFileUtils.readFile(path)
    if contents == nil or contents == '' then
        return nil, 'The critique sidecar file is empty.'
    end

    local ok, payload = pcall(Json.decode, contents)
    if not ok then
        return nil, 'The critique sidecar file does not contain valid JSON.'
    end

    return payload, nil
end

LrTasks.startAsyncTask(function()
    LrFunctionContext.callWithContext('Show Critique Details', function(context)
        if LrDialogs.attachErrorDialogToFunctionContext then
            LrDialogs.attachErrorDialogToFunctionContext(context)
        end

        local photo = selectedPhoto()
        if photo == nil then
            return
        end

        local sidecarPath, pathErr = sidecarPathForPhoto(photo)
        if not sidecarPath then
            LrDialogs.message('Show Critique Details', tostring(pathErr), 'warning')
            return
        end

        if not LrFileUtils.exists(sidecarPath) then
            LrDialogs.message('Show Critique Details', 'No critique sidecar file was found for the selected photo.', 'warning')
            return
        end

        local payload, readErr = readSidecar(sidecarPath)
        if not payload then
            LrDialogs.message('Show Critique Details', tostring(readErr), 'warning')
            return
        end

        local properties = LrBinding.makePropertyTable(context)
        properties.sidecarPath = sidecarPath
        properties.detailsText = encodePrettyJson(payload)

        local factory = LrView.osFactory()
        LrDialogs.presentModalDialog({
            title = 'Critique Details',
            actionVerb = 'Close',
            contents = factory:column({
                bind_to_object = properties,
                spacing = factory:control_spacing(),
                factory:static_text({
                    title = bind('sidecarPath'),
                    width_in_chars = 90,
                    height_in_lines = 2,
                }),
                factory:separator({ fill_horizontal = 1 }),
                factory:scrolled_view({
                    width = 900,
                    height = 560,
                    horizontal_scroller = true,
                    vertical_scroller = true,
                    factory:edit_field({
                        value = bind('detailsText'),
                        width_in_chars = 110,
                        height_in_lines = 36,
                    }),
                }),
            }),
        })
    end)
end)
