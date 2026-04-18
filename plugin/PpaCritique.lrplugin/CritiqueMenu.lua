local LrApplication = import 'LrApplication'
local LrDialogs = import 'LrDialogs'
local LrTasks = import 'LrTasks'

local ResultDialog = require 'ResultDialog'
local ServiceClient = require 'ServiceClient'
local Utils = require 'Utils'

local function updateMetadata(photo, request, response)
    local catalog = LrApplication.activeCatalog()
    catalog:withWriteAccessDo('Write PPA critique metadata', function()
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueStatus', response.preflight and response.preflight.status or 'unknown')
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueCategory', request.category)
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueClassification', response.aggregate and response.aggregate.classification or '')
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueMeritProbability', tostring(response.aggregate and response.aggregate.merit_probability or ''))
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueConfidence', tostring(response.aggregate and response.aggregate.confidence or ''))
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueLastAnalyzedAt', Utils.currentTimestamp())
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueSemanticProvider', response.runtime and response.runtime.semantic_provider or 'disabled')
        photo:setPropertyForPlugin(_PLUGIN, 'ppaCritiqueModel', response.runtime and response.runtime.model or '')
    end)
end

local function buildRequest(photo, exportPath)
    local fileName = photo:getFormattedMetadata('fileName') or 'photo.jpg'

    return {
        image = {
            path = exportPath,
        },
        photo = {
            id = fileName,
            file_name = fileName,
        },
        category = 'illustrative',
        mode = 'mir12',
        options = {
            run_preflight = true,
            run_semantic = false,
            semantic_provider = 'disabled',
        },
        metadata = {
            width = 0,
            height = 0,
            icc_profile = 'sRGB',
            keywords = {},
        },
    }
end

LrTasks.startAsyncTask(function()
    local photo = Utils.getSelectedPhoto()
    if not photo then
        return
    end

    local exportPath = Utils.exportPhoto(photo)
    local request = buildRequest(photo, exportPath)
    local ok, response = pcall(ServiceClient.submitCritique, request)
    if not ok then
        LrDialogs.message('PPA Critique', response, 'critical')
        return
    end

    updateMetadata(photo, request, response)
    ResultDialog.show(response)
end)
