local LrDialogs = import 'LrDialogs'

local ResultDialog = {}

local function joinList(values)
    if values == nil or #values == 0 then
        return 'n/a'
    end

    return table.concat(values, ', ')
end

local function appendSection(lines, title)
    if #lines > 0 then
        lines[#lines + 1] = ''
    end
    lines[#lines + 1] = title
end

local function appendLine(lines, label, value)
    lines[#lines + 1] = label .. ': ' .. ((value ~= nil and value ~= '') and tostring(value) or 'n/a')
end

local function appendChecks(lines, checks)
    if checks == nil or #checks == 0 then
        lines[#lines + 1] = 'Checks: n/a'
        return
    end

    for index, check in ipairs(checks) do
        lines[#lines + 1] = string.format('Check %d: %s [%s] %s', index, check.id or 'unknown', check.result or 'n/a', check.message or '')
    end
end

local function appendVotes(lines, votes)
    if votes == nil or #votes == 0 then
        lines[#lines + 1] = 'Judge votes: n/a'
        return
    end

    for index, vote in ipairs(votes) do
        lines[#lines + 1] = string.format(
            'Vote %d: %s -> %s (confidence %s)',
            index,
            vote.judge_id or 'judge',
            vote.vote or 'n/a',
            tostring(vote.confidence or 'n/a')
        )
        if vote.rationale ~= nil and vote.rationale ~= '' then
            lines[#lines + 1] = '  Overall rationale: ' .. vote.rationale
        end
        local elementReviews = vote.element_reviews or {}
        if #elementReviews == 0 then
            lines[#lines + 1] = '  Element reviews: n/a'
        else
            for _, review in ipairs(elementReviews) do
                lines[#lines + 1] = string.format(
                    '  %s: %s',
                    review.element or 'Element',
                    review.comment or 'n/a'
                )
            end
        end
    end
end

function ResultDialog.show(result, request, metadataValues, metadataFields)
    local aggregate = result.aggregate or {}
    local runtime = result.runtime or {}
    local preflight = result.preflight or {}
    local semantic = result.semantic or nil
    local requestMetadata = request.metadata or {}
    local lines = {}

    appendSection(lines, 'Outcome')
    appendLine(lines, 'Request ID', result.request_id)
    appendLine(lines, 'Classification', aggregate.classification)
    appendLine(lines, 'Merit score', aggregate.merit_score)
    appendLine(lines, 'Summary', aggregate.summary)
    appendLine(lines, 'Merit probability', aggregate.merit_probability)
    appendLine(lines, 'Confidence', aggregate.confidence)
    appendLine(lines, 'Semantic provider', runtime.semantic_provider or 'disabled')
    appendLine(lines, 'Model', runtime.model)

    appendSection(lines, 'Photo request')
    appendLine(lines, 'File', request.photo and request.photo.file_name)
    appendLine(lines, 'Category', request.category)
    appendLine(lines, 'Original path', requestMetadata.original_path)
    appendLine(lines, 'Exported JPEG', request.image and request.image.path)
    appendLine(lines, 'Dimensions', string.format('%sx%s', tostring(requestMetadata.width or 0), tostring(requestMetadata.height or 0)))
    appendLine(lines, 'ICC profile', requestMetadata.icc_profile)
    appendLine(lines, 'File format', requestMetadata.file_format)
    appendLine(lines, 'Capture time', requestMetadata.capture_time)
    appendLine(lines, 'Color label', requestMetadata.color_label)
    appendLine(lines, 'Rating', requestMetadata.rating)
    appendLine(lines, 'Keywords', joinList(requestMetadata.keywords or {}))

    appendSection(lines, 'Preflight')
    appendLine(lines, 'Status', preflight.status)
    appendChecks(lines, preflight.checks)

    appendSection(lines, 'Semantic')
    if semantic then
        appendLine(lines, 'Summary', semantic.summary)
        appendLine(lines, 'Strengths', joinList(semantic.strengths or {}))
        appendLine(lines, 'Improvements', joinList(semantic.improvements or {}))
        appendVotes(lines, semantic.votes)
    else
        lines[#lines + 1] = 'Semantic stage was not run for this request.'
    end

    appendSection(lines, 'Metadata written to photo')
    lines[#lines + 1] = 'These plugin fields are now attached to the selected photo:'
    for _, field in ipairs(metadataFields or {}) do
        lines[#lines + 1] = string.format(
            '%s (%s): %s',
            field.title or field.id,
            field.id,
            metadataValues and metadataValues[field.id] or 'n/a'
        )
    end

    LrDialogs.message('PPA Critique Result', table.concat(lines, '\n'), 'info')
end

return ResultDialog
