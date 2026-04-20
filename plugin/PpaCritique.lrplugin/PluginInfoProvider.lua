local LrDialogs = import 'LrDialogs'
local LrPrefs = import 'LrPrefs'
local LrTasks = import 'LrTasks'
local LrView = import 'LrView'

local PluginVersion = require 'PluginVersion'
local ServiceClient = require 'ServiceClient'
local ServiceLifecycle = require 'ServiceLifecycle'

local bind = LrView.bind

local PluginInfoProvider = {}

local DefaultServiceHost = ServiceClient.defaultHost or 'http://127.0.0.1:6464'
local DefaultServiceTimeoutSeconds = tostring(ServiceClient.defaultTimeoutSeconds or 300)

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

local SemanticModeOptions = {
    { title = 'Ask Every Time', value = 'ask' },
    { title = 'Local Only', value = 'local' },
    { title = 'Run Semantic', value = 'semantic' },
}

local ProviderOptions = {
    { title = 'Disabled', value = 'disabled' },
    { title = 'Ollama', value = 'ollama' },
}

local function trim(value)
    if type(value) ~= 'string' then
        return value
    end
    return value:match('^%s*(.-)%s*$')
end

local function valueOrDefault(value, defaultValue)
    value = trim(value)
    if value == nil or value == '' then
        return defaultValue
    end
    return value
end

local function containsOption(options, value)
    for _, option in ipairs(options) do
        if option.value == value then
            return true
        end
    end
    return false
end

local function uniqueModelItems(availableModels, currentModel, fallbackModel)
    local items = {}
    local seen = {}

    local function appendModel(model)
        model = trim(model)
        if model ~= nil and model ~= '' and not seen[model] then
            seen[model] = true
            items[#items + 1] = { title = model, value = model }
        end
    end

    if type(availableModels) == 'table' then
        for _, model in ipairs(availableModels) do
            appendModel(model)
        end
    end
    appendModel(currentModel)
    appendModel(fallbackModel)

    if #items == 0 then
        appendModel('qwen2.5vl:7b')
        appendModel('qwen2.5vl:3b')
    end

    table.sort(items, function(left, right)
        return left.title < right.title
    end)
    return items
end

local function parseJurorSubset(value)
    value = trim(value) or ''
    if value == '' then
        return {}
    end

    local subset = {}
    local seen = {}
    for entry in string.gmatch(value, '[^,]+') do
        local trimmed = trim(entry)
        if trimmed == nil or trimmed == '' or not trimmed:match('^%d+$') then
            return nil
        end

        local index = tonumber(trimmed)
        if index == nil or index <= 0 then
            return nil
        end

        if not seen[index] then
            seen[index] = true
            subset[#subset + 1] = index
        end
    end

    return subset
end

local function applyPrefs(propertyTable)
    local prefs = LrPrefs.prefsForPlugin()
    propertyTable.serviceHost = valueOrDefault(prefs.serviceHost, DefaultServiceHost)
    propertyTable.serviceTimeoutSeconds = valueOrDefault(prefs.serviceTimeoutSeconds, DefaultServiceTimeoutSeconds)
    propertyTable.defaultSemanticMode = valueOrDefault(prefs.defaultSemanticMode, 'ask')
    propertyTable.defaultCategory = valueOrDefault(prefs.defaultCategory, 'Illustrative')
    propertyTable.defaultJurorSubset = valueOrDefault(prefs.defaultJurorSubset, '')

    if not containsOption(SemanticModeOptions, propertyTable.defaultSemanticMode) then
        propertyTable.defaultSemanticMode = 'ask'
    end
    if not containsOption(CategoryOptions, propertyTable.defaultCategory) then
        propertyTable.defaultCategory = 'Illustrative'
    end
end

local function persistPrefs(propertyTable)
    local prefs = LrPrefs.prefsForPlugin()
    prefs.serviceHost = valueOrDefault(propertyTable.serviceHost, DefaultServiceHost)
    prefs.serviceTimeoutSeconds = valueOrDefault(propertyTable.serviceTimeoutSeconds, DefaultServiceTimeoutSeconds)
    prefs.defaultSemanticMode = valueOrDefault(propertyTable.defaultSemanticMode, 'ask')
    prefs.defaultCategory = valueOrDefault(propertyTable.defaultCategory, 'Illustrative')
    prefs.defaultJurorSubset = valueOrDefault(propertyTable.defaultJurorSubset, '')
end

local function applyServiceConfig(propertyTable, payload)
    local ollama = payload.ollama or {}
    local semantic = payload.semantic or {}
    local availableModels = payload.available_models or {}
    local jurors = payload.jurors or {}

    propertyTable.ollamaBaseUrl = valueOrDefault(ollama.base_url, 'http://127.0.0.1:11434')
    propertyTable.ollamaTimeoutMs = tostring(ollama.timeout_ms or 120000)
    propertyTable.defaultProvider = valueOrDefault(semantic.default_provider, 'ollama')
    propertyTable.availableModelItems = uniqueModelItems(availableModels, ollama.model, ollama.fallback_model)
    propertyTable.ollamaModel = valueOrDefault(ollama.model, propertyTable.availableModelItems[1].value)
    propertyTable.ollamaFallbackModel = valueOrDefault(ollama.fallback_model, propertyTable.ollamaModel)
    propertyTable.availableModelsText = #availableModels > 0 and table.concat(availableModels, ', ') or 'No models reported by service'
    propertyTable.serviceConfigPath = payload.path or ''
    propertyTable.serviceConfigOrigin = payload.from_file and 'Loaded from TOML file' or 'Using defaults in memory'
    propertyTable.jurorSummary = #jurors > 0 and (tostring(#jurors) .. ' jurors configured in service TOML') or 'No jurors configured'
end

local function refreshFromService(propertyTable)
    propertyTable.statusText = 'Refreshing settings from local service...'
    local payload, err = ServiceClient.getConfig()
    if not payload then
        propertyTable.statusText = 'Could not load service settings: ' .. tostring(err)
        return
    end

    applyServiceConfig(propertyTable, payload)
    propertyTable.statusText = 'Service settings loaded successfully.'
end

local function saveToService(propertyTable)
    local timeoutMs = tonumber(propertyTable.ollamaTimeoutMs)
    local serviceTimeoutSeconds = tonumber(propertyTable.serviceTimeoutSeconds)
    local jurorSubset = parseJurorSubset(propertyTable.defaultJurorSubset)
    if timeoutMs == nil or timeoutMs <= 0 then
        propertyTable.statusText = 'Timeout must be a positive integer.'
        return
    end
    if serviceTimeoutSeconds == nil or serviceTimeoutSeconds <= 0 then
        propertyTable.statusText = 'HTTP timeout must be a positive integer number of seconds.'
        return
    end
    if jurorSubset == nil then
        propertyTable.statusText = 'Juror subset must be empty or a comma-separated list like 1,3, 2.'
        return
    end

    persistPrefs(propertyTable)
    propertyTable.statusText = 'Saving settings to local service...'

    local payload = {
        ollama = {
            base_url = valueOrDefault(propertyTable.ollamaBaseUrl, 'http://127.0.0.1:11434'),
            model = valueOrDefault(propertyTable.ollamaModel, 'qwen2.5vl:7b'),
            fallback_model = valueOrDefault(propertyTable.ollamaFallbackModel, valueOrDefault(propertyTable.ollamaModel, 'qwen2.5vl:7b')),
            timeout_ms = math.floor(timeoutMs),
        },
        semantic = {
            default_provider = valueOrDefault(propertyTable.defaultProvider, 'ollama'),
        },
    }

    local response, err = ServiceClient.updateConfig(payload)
    if not response then
        propertyTable.statusText = 'Could not save service settings: ' .. tostring(err)
        return
    end

    applyServiceConfig(propertyTable, response)
    propertyTable.statusText = 'Plugin and service settings saved.'
end

function PluginInfoProvider.startDialog(propertyTable)
    applyPrefs(propertyTable)
    propertyTable.pluginVersion = PluginVersion.version or 'v0.1.0'
    propertyTable.ollamaBaseUrl = 'http://127.0.0.1:11434'
    propertyTable.ollamaTimeoutMs = '120000'
    propertyTable.defaultProvider = 'ollama'
    propertyTable.availableModelItems = uniqueModelItems(nil, 'qwen2.5vl:7b', 'qwen2.5vl:3b')
    propertyTable.ollamaModel = 'qwen2.5vl:7b'
    propertyTable.ollamaFallbackModel = 'qwen2.5vl:3b'
    propertyTable.availableModelsText = 'Refresh to query the local service'
    propertyTable.serviceConfigPath = ''
    propertyTable.serviceConfigOrigin = ''
    propertyTable.jurorSummary = ''
    propertyTable.runtimeState = 'Starting'
    propertyTable.runtimeReachability = 'Unknown'
    propertyTable.runtimePid = ''
    propertyTable.runtimeUptimeSeconds = '0'
    propertyTable.runtimeJobsInFlight = '0'
    propertyTable.runtimeLeaseCount = '0'
    propertyTable.runtimeProviderModel = 'unknown'
    propertyTable.runtimeLastError = 'None'
    propertyTable.runtimeStatusText = 'Waiting for managed service status...'
    propertyTable.statusText = 'Ready.'

    LrTasks.startAsyncTask(function()
        refreshFromService(propertyTable)
    end)
    ServiceLifecycle.start_status_polling(propertyTable)
    ServiceLifecycle.start_managed_service_async(propertyTable)
end

function PluginInfoProvider.endDialog(propertyTable)
    ServiceLifecycle.stop_status_polling()
end

function PluginInfoProvider.sectionsForTopOfDialog(f, propertyTable)
    return {
        {
            title = 'Managed Service Runtime',
            synopsis = bind('runtimeStatusText'),
            bind_to_object = propertyTable,

            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Lifecycle state', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeState'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Reachability', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeReachability'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'PID', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimePid'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Uptime (s)', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeUptimeSeconds'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Active jobs', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeJobsInFlight'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Active leases', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeLeaseCount'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Provider / model', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeProviderModel'), width_in_chars = 45 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Last lifecycle error', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('runtimeLastError'), width_in_chars = 45, height_in_lines = 2 }),
            }),
            f:row({
                spacing = f:control_spacing(),
                f:push_button({
                    title = 'Start',
                    action = function()
                        ServiceLifecycle.start_managed_service_async(propertyTable)
                    end,
                }),
                f:push_button({
                    title = 'Stop',
                    action = function()
                        ServiceLifecycle.stop_managed_service_async(propertyTable)
                    end,
                }),
                f:push_button({
                    title = 'Restart',
                    action = function()
                        ServiceLifecycle.stop_managed_service_async(propertyTable)
                        LrTasks.startAsyncTask(function()
                            LrTasks.sleep(1)
                            ServiceLifecycle.start_managed_service_async(propertyTable)
                        end)
                    end,
                }),
                f:push_button({
                    title = 'Refresh',
                    action = function()
                        ServiceLifecycle.refresh_status_async(propertyTable)
                    end,
                }),
            }),
        },
        {
            title = 'Plugin Defaults',
            synopsis = bind('statusText'),
            bind_to_object = propertyTable,

            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Plugin version', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('pluginVersion'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Service URL', width = 120, alignment = 'right' }),
                f:edit_field({ value = bind('serviceHost'), width_in_chars = 32 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'HTTP timeout (s)', width = 120, alignment = 'right' }),
                f:edit_field({ value = bind('serviceTimeoutSeconds'), width_in_chars = 10 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Default semantic mode', width = 120, alignment = 'right' }),
                f:popup_menu({ value = bind('defaultSemanticMode'), items = SemanticModeOptions }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Default category', width = 120, alignment = 'right' }),
                f:popup_menu({ value = bind('defaultCategory'), items = CategoryOptions }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Juror subset', width = 120, alignment = 'right' }),
                f:edit_field({ value = bind('defaultJurorSubset'), width_in_chars = 20 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = '', width = 120 }),
                f:static_text({ title = 'Blank uses all jurors. Example: 1,3, 2', width_in_chars = 45 }),
            }),
            f:row({
                spacing = f:control_spacing(),
                f:push_button({
                    title = 'Refresh From Service',
                    action = function()
                        persistPrefs(propertyTable)
                        LrTasks.startAsyncTask(function()
                            refreshFromService(propertyTable)
                        end)
                    end,
                }),
            }),
        },
        {
            title = 'Local Service Configuration',
            bind_to_object = propertyTable,

            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Ollama URL', width = 120, alignment = 'right' }),
                f:edit_field({ value = bind('ollamaBaseUrl'), width_in_chars = 32 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Primary model', width = 120, alignment = 'right' }),
                f:popup_menu({ value = bind('ollamaModel'), items = bind('availableModelItems') }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Fallback model', width = 120, alignment = 'right' }),
                f:popup_menu({ value = bind('ollamaFallbackModel'), items = bind('availableModelItems') }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Timeout (ms)', width = 120, alignment = 'right' }),
                f:edit_field({ value = bind('ollamaTimeoutMs'), width_in_chars = 10 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Default provider', width = 120, alignment = 'right' }),
                f:popup_menu({ value = bind('defaultProvider'), items = ProviderOptions }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Available models', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('availableModelsText'), width_in_chars = 50 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Config path', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('serviceConfigPath'), width_in_chars = 50 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Config source', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('serviceConfigOrigin'), width_in_chars = 50 }),
            }),
            f:row({
                spacing = f:label_spacing(),
                f:static_text({ title = 'Juror panel', width = 120, alignment = 'right' }),
                f:static_text({ title = bind('jurorSummary'), width_in_chars = 50 }),
            }),
            f:row({
                spacing = f:control_spacing(),
                f:push_button({
                    title = 'Save Settings',
                    action = function()
                        LrTasks.startAsyncTask(function()
                            saveToService(propertyTable)
                        end)
                    end,
                }),
                f:push_button({
                    title = 'Reload Service Settings',
                    action = function()
                        LrTasks.startAsyncTask(function()
                            refreshFromService(propertyTable)
                        end)
                    end,
                }),
            }),
        },
        {
            title = 'Status',
            bind_to_object = propertyTable,

            f:row({
                f:static_text({
                    title = bind('statusText'),
                    width_in_chars = 70,
                    height_in_lines = 3,
                }),
            }),
        },
    }
end

return PluginInfoProvider
