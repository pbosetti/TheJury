local LrApplication = import 'LrApplication'
local LrBinding = import 'LrBinding'
local LrDialogs = import 'LrDialogs'
local LrFunctionContext = import 'LrFunctionContext'
local LrPrefs = import 'LrPrefs'
local LrProgressScope = import 'LrProgressScope'
local LrView = import 'LrView'

local ServiceClient = require 'ServiceClient'
local Utils = require 'Utils'

local bind = LrView.bind

local MetadataFields = {
    { id = 'ppaCritiqueStatus', title = 'PPA Critique Status', dataType = 'string' },
    { id = 'ppaCritiqueCategory', title = 'PPA Critique Category', dataType = 'string' },
    { id = 'ppaCritiqueClassification', title = 'PPA Critique Classification', dataType = 'string' },
    { id = 'ppaCritiqueMeritScore', title = 'PPA Critique Merit Score', dataType = 'string' },
    { id = 'ppaCritiqueMeritProbability', title = 'PPA Critique Merit Probability', dataType = 'string' },
    { id = 'ppaCritiqueConfidence', title = 'PPA Critique Confidence', dataType = 'string' },
    { id = 'ppaCritiqueSemanticSummary', title = 'PPA Critique Semantic Summary', dataType = 'string' },
    { id = 'ppaCritiqueSemanticVote', title = 'PPA Critique Semantic Vote', dataType = 'string' },
    { id = 'ppaCritiqueSemanticVoteConfidence', title = 'PPA Critique Semantic Vote Confidence', dataType = 'string' },
    { id = 'ppaCritiqueSemanticRationale', title = 'PPA Critique Semantic Rationale', dataType = 'string' },
    { id = 'ppaCritiqueSemanticStrengths', title = 'PPA Critique Semantic Strengths', dataType = 'string' },
    { id = 'ppaCritiqueSemanticImprovements', title = 'PPA Critique Semantic Improvements', dataType = 'string' },
    { id = 'ppaCritiqueLastAnalyzedAt', title = 'PPA Critique Last Analyzed At', dataType = 'string' },
    { id = 'ppaCritiqueSemanticProvider', title = 'PPA Critique Semantic Provider', dataType = 'string' },
    { id = 'ppaCritiqueModel', title = 'PPA Critique Model', dataType = 'string' },
}

local CategoryOptions = {
    { title = 'Portrait', value = 'Portrait' },
    { title = 'Illustrative', value = 'Illustrative' },
    { title = 'Reportage', value = 'Reportage' },
    { title = 'Albums', value = 'Albums' },
    { title = 'Wedding Open', value = 'Wedding Open' },
    { title = 'Single Maker Albums', value = 'Single Maker Albums' },
    { title = 'Multi Maker Albums', value = 'Multi Maker Albums' },
    { title = 'Artist Open', value = 'Artist Open' },
}

local DefaultCategory = CategoryOptions[2]

local function pluginPrefs()
    return LrPrefs.prefsForPlugin()
end

local function joinList(values)
    if values == nil or #values == 0 then
        return ''
    end

    return table.concat(values, ', ')
end

local function buildMetadataValues(request, response)
    local semantic = response.semantic or {}
    local firstVote = semantic.votes and semantic.votes[1] or {}

    return {
        ppaCritiqueStatus = response.preflight and response.preflight.status or 'unknown',
        ppaCritiqueCategory = request.category,
        ppaCritiqueClassification = response.aggregate and response.aggregate.classification or '',
        ppaCritiqueMeritScore = tostring(response.aggregate and response.aggregate.merit_score or ''),
        ppaCritiqueMeritProbability = tostring(response.aggregate and response.aggregate.merit_probability or ''),
        ppaCritiqueConfidence = tostring(response.aggregate and response.aggregate.confidence or ''),
        ppaCritiqueSemanticSummary = semantic.summary or '',
        ppaCritiqueSemanticVote = firstVote.vote or '',
        ppaCritiqueSemanticVoteConfidence = tostring(firstVote.confidence or ''),
        ppaCritiqueSemanticRationale = firstVote.rationale or '',
        ppaCritiqueSemanticStrengths = joinList(semantic.strengths),
        ppaCritiqueSemanticImprovements = joinList(semantic.improvements),
        ppaCritiqueLastAnalyzedAt = Utils.currentTimestamp(),
        ppaCritiqueSemanticProvider = response.runtime and response.runtime.semantic_provider or 'disabled',
        ppaCritiqueModel = response.runtime and response.runtime.model or '',
    }
end

local function updateMetadata(photo, metadataValues)
    local catalog = LrApplication.activeCatalog()
    catalog:withWriteAccessDo('Write PPA critique metadata', function()
        for _, field in ipairs(MetadataFields) do
            photo:setPropertyForPlugin(_PLUGIN, field.id, metadataValues[field.id] or '')
        end
    end)
end

local function updateCategorySelection(photo, category)
    local catalog = LrApplication.activeCatalog()
    catalog:withWriteAccessDo('Set PPA critique category', function()
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueCategory', category or DefaultCategory.value)
    end)
end

local function normalizeCategory(value)
    if type(value) ~= 'string' then
        return DefaultCategory.value
    end

    local trimmed = value:match('^%s*(.-)%s*$')
    local lowered = string.lower(trimmed or '')
    for _, category in ipairs(CategoryOptions) do
        if lowered == string.lower(category.value) then
            return category.value
        end
    end

    return DefaultCategory.value
end

local function chooseCategory(context, photo)
    local properties = LrBinding.makePropertyTable(context)
    properties.category = normalizeCategory(
        photo:getPropertyForPlugin(_PLUGIN, 'ppaCritiqueCategory') or pluginPrefs().defaultCategory
    )

    local factory = LrView.osFactory()
    local result = LrDialogs.presentModalDialog({
        title = 'PPA Critique Category',
        actionVerb = 'Continue',
        cancelVerb = 'Cancel',
        contents = factory:column({
            bind_to_object = properties,
            spacing = factory:control_spacing(),
            factory:static_text({
                title = 'Select the PPA Merit Image Review category for the selected photo(s).',
            }),
            factory:popup_menu({
                value = bind('category'),
                items = CategoryOptions,
            }),
            factory:static_text({
                title = 'The selected value will be sent to the service and written to the PPA Critique Category metadata field.',
                width_in_chars = 60,
                height_in_lines = 2,
            }),
        }),
    })

    if result ~= 'ok' then
        return nil
    end

    return normalizeCategory(properties.category)
end

local function chooseSemanticMode()
    local defaultSemanticMode = pluginPrefs().defaultSemanticMode
    if defaultSemanticMode == 'local' then
        return false
    end
    if defaultSemanticMode == 'semantic' then
        return true
    end

    local choice = LrDialogs.confirm(
        'PPA Critique',
        'Choose whether to run the local Ollama semantic stage for this critique.',
        'Run Semantic',
        'Cancel',
        'Local Only'
    )

    if choice == 'cancel' then
        return nil
    end

    return choice == 'ok'
end

local function buildRequest(photo, exportPath, runSemantic, category)
    local fileName = photo:getFormattedMetadata('fileName') or 'photo.jpg'
    local requestMetadata = Utils.collectRequestMetadata(photo)

    return {
        image = {
            path = exportPath,
        },
        photo = {
            id = requestMetadata.original_path ~= '' and requestMetadata.original_path or fileName,
            file_name = fileName,
        },
        category = category or DefaultCategory.value,
        mode = 'mir12',
        options = {
            run_preflight = true,
            run_semantic = runSemantic,
            semantic_provider = runSemantic and 'ollama' or 'disabled',
        },
        metadata = requestMetadata,
    }
end

local function updateProgress(progressScope, step, totalSteps, caption)
    if not progressScope then
        return
    end

    if caption ~= nil and caption ~= '' then
        progressScope:setCaption(caption)
    end
    progressScope:setPortionComplete(step, totalSteps)
end

local function photoFileName(photo)
    return photo:getFormattedMetadata('fileName') or 'photo.jpg'
end

local function appendFailure(failures, fileName, message)
    failures[#failures + 1] = string.format('%s: %s', fileName or 'photo', message or 'Unknown error')
end

local function showBatchSummary(successCount, failures, canceled, totalPhotos)
    if not canceled and #failures == 0 then
        return
    end

    local lines = {}
    if canceled then
        lines[#lines + 1] = string.format('Critique canceled after processing %d of %d photo(s).', successCount + #failures, totalPhotos)
    else
        lines[#lines + 1] = string.format('Processed %d of %d photo(s) successfully.', successCount, totalPhotos)
    end

    if #failures > 0 then
        lines[#lines + 1] = ''
        lines[#lines + 1] = 'Failures:'
        for _, failure in ipairs(failures) do
            lines[#lines + 1] = failure
        end
    end

    LrDialogs.message('PPA Critique', table.concat(lines, '\n'), canceled and 'warning' or 'critical')
end

local function processPhoto(photo, category, runSemantic, progressScope, photoIndex, totalPhotos)
    local totalSteps = totalPhotos * 5
    local baseStep = (photoIndex - 1) * 5
    local fileName = photoFileName(photo)
    local prefix = string.format('Photo %d of %d: %s', photoIndex, totalPhotos, fileName)

    updateProgress(progressScope, baseStep, totalSteps, prefix .. ' - setting category...')
    updateCategorySelection(photo, category)

    if progressScope:isCanceled() then
        return nil, 'canceled', fileName
    end

    updateProgress(progressScope, baseStep + 1, totalSteps, prefix .. ' - exporting JPEG...')
    local exportPath = Utils.exportPhoto(photo)

    if progressScope:isCanceled() then
        return nil, 'canceled', fileName
    end

    updateProgress(progressScope, baseStep + 2, totalSteps, prefix .. ' - collecting Lightroom metadata...')
    local request = buildRequest(photo, exportPath, runSemantic, category)

    if progressScope:isCanceled() then
        return nil, 'canceled', fileName
    end

    updateProgress(progressScope, baseStep + 3, totalSteps, prefix .. ' - submitting to local service...')
    local response, err = ServiceClient.submitCritique(request)
    if not response then
        return nil, err or 'Request to local service failed.', fileName
    end

    if progressScope:isCanceled() then
        return nil, 'canceled', fileName
    end

    updateProgress(progressScope, baseStep + 4, totalSteps, prefix .. ' - writing metadata...')
    local metadataValues = buildMetadataValues(request, response)
    updateMetadata(photo, metadataValues)
    updateProgress(progressScope, baseStep + 5, totalSteps, prefix .. ' - complete.')

    return true, nil, fileName
end

LrFunctionContext.postAsyncTaskWithContext('PPA Critique', function(context)
    local photos = Utils.getSelectedPhotos()
    if not photos then
        return
    end

    local category = chooseCategory(context, photos[1])
    if category == nil then
        return
    end

    local runSemantic = chooseSemanticMode()
    if runSemantic == nil then
        return
    end

    local totalPhotos = #photos
    local totalSteps = totalPhotos * 5
    local progressScope = LrProgressScope({
        title = 'PPA Critique',
        caption = 'Preparing batch critique...',
        functionContext = context,
    })
    progressScope:setCancelable(true)
    updateProgress(progressScope, 0, totalSteps, string.format('Preparing critique for %d photo(s)...', totalPhotos))

    local successCount = 0
    local failures = {}
    local canceled = false

    for index, photo in ipairs(photos) do
        if progressScope:isCanceled() then
            canceled = true
            break
        end

        local ok, err, fileName = processPhoto(photo, category, runSemantic, progressScope, index, totalPhotos)
        if ok then
            successCount = successCount + 1
        elseif err == 'canceled' then
            canceled = true
            break
        else
            appendFailure(failures, fileName, err)
        end
    end

    if canceled then
        updateProgress(
            progressScope,
            successCount * 5,
            totalSteps,
            string.format('Critique canceled after %d of %d photo(s).', successCount + #failures, totalPhotos)
        )
    else
        updateProgress(
            progressScope,
            totalSteps,
            totalSteps,
            string.format('Processed %d photo(s).', successCount)
        )
    end

    progressScope:done()
    showBatchSummary(successCount, failures, canceled, totalPhotos)
end)
