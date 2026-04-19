local LrApplication = import 'LrApplication'
local LrDialogs = import 'LrDialogs'
local LrExportSession = import 'LrExportSession'
local LrLogger = import 'LrLogger'
local LrPathUtils = import 'LrPathUtils'

local Utils = {}
local logger = LrLogger('TheJury')

local function exportFileNameForPhoto(photo)
    local fileName = photo:getFormattedMetadata('fileName') or 'photo.jpg'
    local stem = fileName:gsub('%.[^%.]+$', '')
    if stem == '' then
        stem = 'photo'
    end
    return stem .. '.jpg'
end

local function safeGetRawMetadata(photo, key)
    return photo:getRawMetadata(key)
end

local function safeGetFormattedMetadata(photo, key)
    return photo:getFormattedMetadata(key)
end

local function trim(value)
    if type(value) ~= 'string' then
        return value
    end

    return value:match('^%s*(.-)%s*$')
end

local function firstNonEmptyValue(...)
    for index = 1, select('#', ...) do
        local candidate = select(index, ...)
        if candidate ~= nil then
            if type(candidate) == 'string' then
                candidate = trim(candidate)
            end

            if candidate ~= '' then
                return candidate
            end
        end
    end

    return nil
end

local function toInteger(value)
    if type(value) == 'number' then
        return math.floor(value)
    end

    if type(value) == 'string' then
        local number = tonumber(value)
        if number ~= nil then
            return math.floor(number)
        end
    end

    return 0
end

local function parseDimensionPair(value)
    if type(value) == 'table' then
        local width = toInteger(value.width or value[1])
        local height = toInteger(value.height or value[2])
        if width > 0 and height > 0 then
            return width, height
        end
    end

    if type(value) == 'string' then
        local width, height = value:match('(%d+)%s*[xX]%s*(%d+)')
        if width and height then
            return toInteger(width), toInteger(height)
        end
    end

    return 0, 0
end

local function stringify(value)
    if value == nil then
        return nil
    end
    if type(value) == 'string' then
        return trim(value)
    end
    if type(value) == 'number' or type(value) == 'boolean' then
        return tostring(value)
    end
    return nil
end

local function firstRawMetadataValue(rawMetadata, keys)
    for _, key in ipairs(keys) do
        local value = rawMetadata[key]
        if value ~= nil then
            return value
        end
    end
    return nil
end

local function firstRawString(rawMetadata, keys)
    return firstNonEmptyValue(firstRawMetadataValue(rawMetadata, keys))
end

local function appendKeyword(keywords, seen, value)
    local text = stringify(value)
    if text == nil or text == '' then
        return
    end
    if not seen[text] then
        seen[text] = true
        keywords[#keywords + 1] = text
    end
end

local function keywordsFromValue(value, keywords, seen)
    if value == nil then
        return
    end

    if type(value) == 'string' then
        for entry in value:gmatch('[^,;]+') do
            appendKeyword(keywords, seen, entry)
        end
        return
    end

    if type(value) == 'table' then
        for _, entry in ipairs(value) do
            if type(entry) == 'table' then
                appendKeyword(keywords, seen, entry.keyword)
                appendKeyword(keywords, seen, entry.name)
                appendKeyword(keywords, seen, entry.title)
                appendKeyword(keywords, seen, entry.value)
            else
                appendKeyword(keywords, seen, entry)
            end
        end
    end
end

local function collectRawMetadataTable(photo)
    local value = photo:getRawMetadata()
    if type(value) == 'table' then
        return value
    end

    logger:warn('photo:getRawMetadata() returned type ' .. type(value))
    return {}
end

local function logInterestingRawMetadata(rawMetadata)
    local entries = {}

    for key, value in pairs(rawMetadata) do
        local lowerKey = string.lower(tostring(key))
        if lowerKey:find('width', 1, true) or
            lowerKey:find('height', 1, true) or
            lowerKey:find('dimension', 1, true) or
            lowerKey:find('size', 1, true) or
            lowerKey:find('path', 1, true) or
            lowerKey:find('date', 1, true) or
            lowerKey:find('time', 1, true) or
            lowerKey:find('format', 1, true) or
            lowerKey:find('type', 1, true) or
            lowerKey:find('profile', 1, true) or
            lowerKey:find('icc', 1, true) or
            lowerKey:find('keyword', 1, true) or
            lowerKey:find('label', 1, true) or
            lowerKey:find('rating', 1, true) then
            entries[#entries + 1] = tostring(key) .. '=' .. tostring(value)
        end
    end

    table.sort(entries)
    logger:info('Interesting raw metadata keys: ' .. table.concat(entries, '; '))
end

local function dimensionsFromRawMetadataTable(rawMetadata)
    local preferredWidthKeys = {
        'croppedWidth',
        'width',
        'fileWidth',
        'pixelWidth',
        'rawWidth',
    }
    local preferredHeightKeys = {
        'croppedHeight',
        'height',
        'fileHeight',
        'pixelHeight',
        'rawHeight',
    }

    for _, widthKey in ipairs(preferredWidthKeys) do
        for _, heightKey in ipairs(preferredHeightKeys) do
            local width = toInteger(rawMetadata[widthKey])
            local height = toInteger(rawMetadata[heightKey])
            if width > 0 and height > 0 then
                return width, height
            end
        end
    end

    for key, value in pairs(rawMetadata) do
        local lowerKey = string.lower(tostring(key))
        if lowerKey:find('dimension', 1, true) or lowerKey:find('size', 1, true) then
            local width, height = parseDimensionPair(value)
            if width > 0 and height > 0 then
                return width, height
            end
        end
    end

    local discoveredWidth = 0
    local discoveredHeight = 0
    for key, value in pairs(rawMetadata) do
        local lowerKey = string.lower(tostring(key))
        if discoveredWidth == 0 and lowerKey:find('width', 1, true) then
            discoveredWidth = toInteger(value)
        end
        if discoveredHeight == 0 and lowerKey:find('height', 1, true) then
            discoveredHeight = toInteger(value)
        end
    end

    if discoveredWidth > 0 and discoveredHeight > 0 then
        return discoveredWidth, discoveredHeight
    end

    return 0, 0
end

local function splitKeywords(value)
    local keywords = {}
    local seen = {}
    keywordsFromValue(value, keywords, seen)
    return keywords
end

function Utils.getSelectedPhoto()
    local catalog = LrApplication.activeCatalog()
    local targetPhotos = catalog:getTargetPhotos()
    if #targetPhotos ~= 1 then
        LrDialogs.message('PPA Critique', 'Select exactly one photo before running the critique.', 'warning')
        return nil
    end

    return targetPhotos[1]
end

function Utils.buildTemporaryExportPath(photo)
    local tempDir = _PLUGIN.tmpDir or LrPathUtils.getStandardFilePath('temp')
    return LrPathUtils.child(tempDir, exportFileNameForPhoto(photo))
end

function Utils.exportPhoto(photo)
    local exportPath = Utils.buildTemporaryExportPath(photo)
    local exportSession = LrExportSession {
        photosToExport = { photo },
        exportSettings = {
            LR_export_destinationType = 'specificFolder',
            LR_export_destinationPathPrefix = LrPathUtils.parent(exportPath),
            LR_export_useSubfolder = false,
            LR_collisionHandling = 'overwrite',
            LR_format = 'JPEG',
            LR_jpeg_quality = 0.8,
            LR_size_doConstrain = false,
            LR_export_colorSpace = 'sRGB',
        },
    }

    exportSession:doExportOnCurrentTask()
    return exportPath
end

function Utils.collectRequestMetadata(photo)
    local rawMetadata = collectRawMetadataTable(photo)
    logInterestingRawMetadata(rawMetadata)

    local width = toInteger(firstNonEmptyValue(
        rawMetadata.croppedWidth,
        rawMetadata.width,
        rawMetadata.fileWidth,
        rawMetadata.pixelWidth,
        rawMetadata.rawWidth
    ))
    local height = toInteger(firstNonEmptyValue(
        rawMetadata.croppedHeight,
        rawMetadata.height,
        rawMetadata.fileHeight,
        rawMetadata.pixelHeight,
        rawMetadata.rawHeight
    ))

    if width == 0 or height == 0 then
        local parsedWidth, parsedHeight = parseDimensionPair(firstNonEmptyValue(
            rawMetadata.dimensions,
            rawMetadata.croppedDimensions,
            rawMetadata.fileDimensions
        ))
        if width == 0 then
            width = parsedWidth
        end
        if height == 0 then
            height = parsedHeight
        end
    end

    if width == 0 or height == 0 then
        local discoveredWidth, discoveredHeight = dimensionsFromRawMetadataTable(rawMetadata)
        if width == 0 then
            width = discoveredWidth
        end
        if height == 0 then
            height = discoveredHeight
        end
    end

    if width == 0 or height == 0 then
        local candidateKeys = {}
        for key, value in pairs(rawMetadata) do
            local lowerKey = string.lower(tostring(key))
            if lowerKey:find('width', 1, true) or lowerKey:find('height', 1, true) or
                lowerKey:find('dimension', 1, true) or lowerKey:find('size', 1, true) then
                candidateKeys[#candidateKeys + 1] = tostring(key) .. '=' .. tostring(value)
            end
        end
        table.sort(candidateKeys)
        logger:warn('Could not determine Lightroom dimensions. Candidate metadata: ' ..
            table.concat(candidateKeys, '; '))
    end

    local metadata = {
        width = width,
        height = height,
        icc_profile = firstNonEmptyValue(
            firstRawString(rawMetadata, { 'colorProfile', 'profileName', 'embeddedProfile', 'iccProfile' }),
            'unknown'
        ),
        keywords = splitKeywords(firstNonEmptyValue(
            firstRawMetadataValue(rawMetadata, { 'keywordTags', 'keywords', 'keywordTagsForExport' }),
            ''
        )),
        original_path = firstNonEmptyValue(
            firstRawString(rawMetadata, { 'path', 'absolutePath', 'originalPath' }),
            ''
        ),
        capture_time = firstNonEmptyValue(
            firstRawString(rawMetadata, {
                'dateTimeOriginalISO8601',
                'dateTimeISO8601',
                'dateTimeDigitizedISO8601',
                'dateTimeOriginal',
                'captureTime',
                'dateTime',
                'dateTimeDigitized',
            }),
            ''
        ),
        file_format = firstNonEmptyValue(
            firstRawString(rawMetadata, { 'fileFormat', 'fileType', 'format' }),
            ''
        ),
        color_label = firstNonEmptyValue(
            firstRawString(rawMetadata, { 'colorNameForLabel', 'label', 'colorLabel' }),
            ''
        ),
        rating = toInteger(firstNonEmptyValue(
            firstRawMetadataValue(rawMetadata, { 'rating' })
        )),
    }

    logger:info('Request metadata width=' .. tostring(metadata.width) ..
        ' height=' .. tostring(metadata.height) ..
        ' icc_profile=' .. tostring(metadata.icc_profile) ..
        ' original_path=' .. tostring(metadata.original_path) ..
        ' capture_time=' .. tostring(metadata.capture_time) ..
        ' file_format=' .. tostring(metadata.file_format) ..
        ' color_label=' .. tostring(metadata.color_label) ..
        ' rating=' .. tostring(metadata.rating) ..
        ' keywords=' .. table.concat(metadata.keywords, ','))

    return metadata
end

function Utils.currentTimestamp()
    return os.date('!%Y-%m-%dT%H:%M:%SZ')
end

return Utils
