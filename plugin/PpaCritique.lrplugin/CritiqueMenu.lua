local LrApplication = import 'LrApplication'
local LrDialogs = import 'LrDialogs'
local LrFunctionContext = import 'LrFunctionContext'

local ResultDialog = require 'ResultDialog'
local ServiceClient = require 'ServiceClient'
local Utils = require 'Utils'

local MetadataFields = {
    { id = 'ppaCritiqueStatus', title = 'PPA Critique Status', dataType = 'string' },
    { id = 'ppaCritiqueCategory', title = 'PPA Critique Category', dataType = 'string' },
    { id = 'ppaCritiqueClassification', title = 'PPA Critique Classification', dataType = 'string' },
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

local function chooseSemanticMode()
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

local function buildRequest(photo, exportPath, runSemantic)
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
        category = 'illustrative',
        mode = 'mir12',
        options = {
            run_preflight = true,
            run_semantic = runSemantic,
            semantic_provider = runSemantic and 'ollama' or 'disabled',
        },
        metadata = requestMetadata,
    }
end

LrFunctionContext.postAsyncTaskWithContext('PPA Critique', function()
    local photo = Utils.getSelectedPhoto()
    if not photo then
        return
    end

    local runSemantic = chooseSemanticMode()
    if runSemantic == nil then
        return
    end

    local exportPath = Utils.exportPhoto(photo)
    local request = buildRequest(photo, exportPath, runSemantic)
    local response, err = ServiceClient.submitCritique(request)
    if not response then
        LrDialogs.message('PPA Critique', err or 'Request to local service failed.', 'critical')
        return
    end

    local metadataValues = buildMetadataValues(request, response)
    updateMetadata(photo, metadataValues)
    ResultDialog.show(response, request, metadataValues, MetadataFields)
end)
