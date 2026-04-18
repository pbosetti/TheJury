local LrApplication = import 'LrApplication'
local LrDialogs = import 'LrDialogs'
local LrExportSession = import 'LrExportSession'
local LrPathUtils = import 'LrPathUtils'

local Utils = {}

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
    local fileName = photo:getFormattedMetadata('fileName') or 'photo.jpg'
    local tempDir = _PLUGIN.tmpDir or LrPathUtils.getStandardFilePath('temp')
    return LrPathUtils.child(tempDir, fileName)
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

function Utils.currentTimestamp()
    return os.date('!%Y-%m-%dT%H:%M:%SZ')
end

return Utils
