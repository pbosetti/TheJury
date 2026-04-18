local LrDialogs = import 'LrDialogs'

local ResultDialog = {}

function ResultDialog.show(result)
    local aggregate = result.aggregate or {}
    local runtime = result.runtime or {}
    local message = table.concat({
        'Classification: ' .. (aggregate.classification or 'n/a'),
        'Summary: ' .. (aggregate.summary or 'n/a'),
        'Provider: ' .. (runtime.semantic_provider or 'disabled'),
        'Model: ' .. (runtime.model or ''),
    }, '\n')

    LrDialogs.message('PPA Critique Result', message, 'info')
end

return ResultDialog
