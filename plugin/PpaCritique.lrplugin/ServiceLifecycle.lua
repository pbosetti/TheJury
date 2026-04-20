local LrFileUtils = import 'LrFileUtils'
local LrLogger = import 'LrLogger'
local LrPathUtils = import 'LrPathUtils'
local LrTasks = import 'LrTasks'

local Json = require 'Json'
local PluginVersion = require 'PluginVersion'
local ServiceClient = require 'ServiceClient'

local logger = LrLogger('TheJury')
logger:enable('logfile')

local ServiceLifecycle = {}

local HeartbeatSeconds = 5
local LeaseTtlSeconds = 15
local StatusPollSeconds = 5

local state = {
    instance_id = nil,
    lifecycle_state = 'stopped',
    last_error = '',
    command_busy = false,
    heartbeat_generation = 0,
    heartbeat_running = false,
    poll_generation = 0,
    poll_running = false,
}

local function trim(value)
    if type(value) ~= 'string' then
        return value
    end
    return value:match('^%s*(.-)%s*$')
end

local function generate_instance_id()
    math.randomseed(os.time())
    return string.format('thejury-%d-%06d', os.time(), math.random(0, 999999))
end

local function ensure_instance_id()
    if state.instance_id == nil then
        state.instance_id = generate_instance_id()
    end
    return state.instance_id
end

local function is_windows_path(path)
    return type(path) == 'string' and path:match('%.exe$') ~= nil
end

local function posix_quote(value)
    return "'" .. tostring(value):gsub("'", [['"'"']]) .. "'"
end

local function windows_quote(value)
    return '"' .. tostring(value):gsub('"', '\\"') .. '"'
end

local function shell_quote(value, windows)
    if windows then
        return windows_quote(value)
    end
    return posix_quote(value)
end

local function helperCandidates()
    return {
        LrPathUtils.child(_PLUGIN.path, 'bin/macos-arm64/ppa_service_host'),
        LrPathUtils.child(_PLUGIN.path, 'bin/macos-x86_64/ppa_service_host'),
        LrPathUtils.child(_PLUGIN.path, 'bin/windows-x86_64/ppa_service_host.exe'),
        LrPathUtils.child(_PLUGIN.path, 'bin/linux-x86_64/ppa_service_host'),
        LrPathUtils.child(_PLUGIN.path, 'bin/linux-arm64/ppa_service_host'),
    }
end

local function helperPath()
    for _, candidate in ipairs(helperCandidates()) do
        if candidate and LrFileUtils.exists(candidate) then
            return candidate
        end
    end
    return nil
end

local function runtimeSupportDir()
    local localAppData = os.getenv('LOCALAPPDATA')
    if type(localAppData) == 'string' and localAppData ~= '' then
        return LrPathUtils.child(localAppData, 'TheJury')
    end

    local appData = os.getenv('APPDATA')
    if type(appData) == 'string' and appData ~= '' then
        return LrPathUtils.child(appData, 'TheJury')
    end

    local home = os.getenv('HOME')
    if type(home) == 'string' and home ~= '' then
        return home .. '/Library/Application Support/TheJury'
    end

    return nil
end

local function runtimeLogPath()
    local supportDir = runtimeSupportDir()
    if supportDir == nil then
        return nil
    end
    return LrPathUtils.child(LrPathUtils.child(supportDir, 'runtime'), 'ppa_service.log')
end

local function readFile(path)
    if not path or not LrFileUtils.exists(path) then
        return ''
    end

    local ok, contents = pcall(LrFileUtils.readFile, path)
    if not ok or contents == nil then
        return ''
    end
    return contents
end

local function removeFile(path)
    if path and LrFileUtils.exists(path) then
        pcall(LrFileUtils.delete, path)
        pcall(os.remove, path)
    end
end

local function decodeJson(payload)
    if payload == nil or payload == '' then
        return nil, 'empty response'
    end

    local ok, decoded = pcall(Json.decode, payload)
    if not ok then
        return nil, decoded
    end
    return decoded
end

local function waitForRuntimeStatus(command, attempts)
    attempts = attempts or (command == 'start' and 25 or 5)
    for _ = 1, attempts do
        local runtimeStatus = ServiceClient.getRuntimeStatus()
        if runtimeStatus then
            return runtimeStatus
        end
        LrTasks.sleep(0.2)
    end
    return nil
end

local function lastNonEmptyLines(text, limit)
    local lines = {}
    for line in tostring(text):gmatch('[^\r\n]+') do
        local trimmed = trim(line)
        if trimmed ~= nil and trimmed ~= '' then
            lines[#lines + 1] = trimmed
        end
    end

    local startIndex = math.max(1, #lines - (limit or 3) + 1)
    local tail = {}
    for index = startIndex, #lines do
        tail[#tail + 1] = lines[index]
    end
    return tail
end

local function enrichStartFailureMessage(defaultMessage, helperResult)
    local logPath = runtimeLogPath()
    local logText = readFile(logPath)
    if logText ~= '' then
        local lowered = string.lower(logText)
        if lowered:find('failed to bind http server', 1, true) then
            return 'managed local service could not bind 127.0.0.1:6464; another process is likely already using that port'
        end

        local tail = lastNonEmptyLines(logText, 3)
        if #tail > 0 then
            return defaultMessage .. ' (' .. table.concat(tail, ' | ') .. ')'
        end
    end

    if helperResult and helperResult.exit_code ~= 0 then
        return defaultMessage .. ' (helper exit code ' .. tostring(helperResult.exit_code) .. ')'
    end

    return defaultMessage
end

local function applyRuntimeStatus(propertyTable, payload)
    local reachable = payload and payload.reachable == true
    local runtimeState = payload and payload.state or 'stopped'
    local provider = payload and payload.provider or 'disabled'
    local model = payload and payload.model or ''
    local pid = payload and tostring(payload.pid or '') or ''
    local uptime = payload and tostring(payload.uptime_seconds or 0) or '0'
    local jobs = payload and tostring(payload.jobs_in_flight or 0) or '0'
    local leases = payload and tostring(payload.active_lease_count or 0) or '0'
    local lastError = trim(payload and payload.last_error or '') or ''

    state.lifecycle_state = runtimeState
    state.last_error = lastError

    if propertyTable then
        propertyTable.runtimeState = runtimeState
        propertyTable.runtimeReachability = reachable and 'Reachable' or 'Unreachable'
        propertyTable.runtimePid = pid
        propertyTable.runtimeUptimeSeconds = uptime
        propertyTable.runtimeJobsInFlight = jobs
        propertyTable.runtimeLeaseCount = leases
        propertyTable.runtimeProviderModel = model ~= '' and (provider .. ' / ' .. model) or provider
        propertyTable.runtimeLastError = lastError ~= '' and lastError or 'None'
        propertyTable.runtimeStatusText = string.format(
            '%s | %s | jobs=%s | leases=%s',
            runtimeState,
            reachable and 'reachable' or 'unreachable',
            jobs,
            leases
        )
    end
end

local function withCommandLock(fn)
    while state.command_busy do
        LrTasks.sleep(0.1)
    end

    state.command_busy = true
    local a, b = fn()
    state.command_busy = false
    return a, b
end

local function buildHelperShellCommand(command)
    local helper = helperPath()
    if not helper then
        return nil, 'managed service helper not found inside plugin bundle bin directory'
    end

    local isWindows = is_windows_path(helper)
    local quotedHelper = shell_quote(helper, isWindows)
    local quotedCommand = shell_quote(command, isWindows)
    return string.format('%s %s', quotedHelper, quotedCommand)
end

local function runHelperCommand(command)
    local shellCommand, err = buildHelperShellCommand(command)
    if not shellCommand then
        return nil, err
    end

    logger:info('Executing managed service helper: ' .. shellCommand)
    local exitCode = LrTasks.execute(shellCommand)
    return {
        exit_code = exitCode,
        command = command,
    }
end

local function executeHelperCommand(command)
    return withCommandLock(function()
        local result, err = runHelperCommand(command)
        if not result then
            return nil, err
        end
        if result.exit_code ~= 0 and command ~= 'start' and command ~= 'stop' then
            return nil, 'helper command failed with exit code ' .. tostring(result.exit_code)
        end
        return result
    end)
end

local function renewLease(propertyTable)
    local payload, err = ServiceClient.renewRuntimeLease({
        client = 'com.pbosetti.thejury',
        instance_id = ensure_instance_id(),
        ttl_seconds = LeaseTtlSeconds,
    })
    if not payload then
        state.last_error = tostring(err)
        if propertyTable then
            propertyTable.runtimeLastError = state.last_error
        end
        return nil, err
    end

    local runtimeStatus, runtimeErr = ServiceClient.getRuntimeStatus()
    if runtimeStatus then
        applyRuntimeStatus(propertyTable, runtimeStatus)
    elseif payload then
        applyRuntimeStatus(propertyTable, {
            state = payload.state or 'running',
            reachable = true,
            active_lease_count = payload.active_lease_count or 1,
            jobs_in_flight = 0,
            provider = 'ollama',
            model = '',
            last_error = '',
        })
    else
        state.last_error = tostring(runtimeErr)
    end

    return payload
end

local function ensureServiceRunning(propertyTable)
    local helperResult, err = executeHelperCommand('start')
    if not helperResult then
        state.lifecycle_state = 'stopped'
        state.last_error = tostring(err)
        applyRuntimeStatus(propertyTable, {
            state = 'stopped',
            reachable = false,
            last_error = state.last_error,
        })
        return nil, err
    end

    local payload = waitForRuntimeStatus('start')
    if not payload then
        local message = helperResult.exit_code ~= 0
            and ('helper command failed with exit code ' .. tostring(helperResult.exit_code))
            or 'could not reach managed local service after start'
        message = enrichStartFailureMessage(message, helperResult)
        state.lifecycle_state = 'stopped'
        state.last_error = message
        applyRuntimeStatus(propertyTable, {
            state = 'stopped',
            reachable = false,
            last_error = message,
        })
        return nil, message
    end

    applyRuntimeStatus(propertyTable, payload)
    local lease, leaseErr = renewLease(propertyTable)
    if not lease then
        return nil, leaseErr
    end
    return payload
end

local function releaseLease(propertyTable)
    local payload, err = ServiceClient.releaseRuntimeLease(ensure_instance_id())
    if payload then
        applyRuntimeStatus(propertyTable, payload)
        return payload
    end

    local statusPayload, statusErr = waitForRuntimeStatus('status', 1)
    if statusPayload then
        applyRuntimeStatus(propertyTable, statusPayload)
        return statusPayload
    end

    state.last_error = tostring(err or statusErr)
    applyRuntimeStatus(propertyTable, {
        state = 'stopped',
        reachable = false,
        last_error = state.last_error,
    })
    return nil, err or statusErr
end

local function refreshStatus(propertyTable)
    local payload, err = ServiceClient.getRuntimeStatus()
    if payload then
        applyRuntimeStatus(propertyTable, payload)
        return payload
    end

    state.last_error = tostring(err or 'local service unreachable')
    applyRuntimeStatus(propertyTable, {
        state = 'stopped',
        reachable = false,
        last_error = state.last_error,
    })
    return nil, err or state.last_error
end

function ServiceLifecycle.begin_heartbeat(propertyTable)
    if state.heartbeat_running then
        return
    end

    state.heartbeat_generation = state.heartbeat_generation + 1
    local generation = state.heartbeat_generation
    state.heartbeat_running = true
    LrTasks.startAsyncTask(function()
        while generation == state.heartbeat_generation do
            local _, err = ensureServiceRunning(propertyTable)
            if err then
                logger:error('Managed service start failed: ' .. tostring(err))
            end

            for _ = 1, HeartbeatSeconds do
                if generation ~= state.heartbeat_generation then
                    break
                end
                LrTasks.sleep(1)
            end
        end
        state.heartbeat_running = false
    end)
end

function ServiceLifecycle.end_heartbeat()
    state.heartbeat_generation = state.heartbeat_generation + 1
end

function ServiceLifecycle.stop_managed_service_now()
    ServiceLifecycle.end_heartbeat()
    ServiceLifecycle.stop_status_polling()

    local result, err = runHelperCommand('stop')
    if not result then
        logger:error('Could not synchronously stop managed service: ' .. tostring(err))
        return nil, err
    end

    if result.exit_code ~= 0 then
        local message = 'helper command failed with exit code ' .. tostring(result.exit_code)
        logger:warn('Managed service stop returned non-zero during shutdown: ' .. message)
        return nil, message
    end

    state.lifecycle_state = 'stopped'
    state.last_error = ''
    return result
end

function ServiceLifecycle.start_managed_service_async(propertyTable)
    LrTasks.startAsyncTask(function()
        local _, err = ensureServiceRunning(propertyTable)
        if err then
            logger:error('Could not start managed service: ' .. tostring(err))
        end
        ServiceLifecycle.begin_heartbeat(propertyTable)
    end)
end

function ServiceLifecycle.stop_managed_service_async(propertyTable)
    ServiceLifecycle.end_heartbeat()
    LrTasks.startAsyncTask(function()
        local helperResult, err = executeHelperCommand('stop')
        if helperResult then
            local payload = waitForRuntimeStatus('stop', 5) or {
                state = 'stopped',
                reachable = false,
                service = 'ppa-companion',
                version = PluginVersion.version or 'v0.1.0',
                last_error = helperResult.exit_code ~= 0 and ('helper command failed with exit code ' .. tostring(helperResult.exit_code)) or '',
            }
            applyRuntimeStatus(propertyTable, payload)
        else
            state.last_error = tostring(err)
            applyRuntimeStatus(propertyTable, {
                state = 'stopped',
                reachable = false,
                last_error = state.last_error,
            })
        end
    end)
end

function ServiceLifecycle.release_lease_async(propertyTable)
    ServiceLifecycle.end_heartbeat()
    LrTasks.startAsyncTask(function()
        local _, err = releaseLease(propertyTable)
        if err then
            logger:error('Could not release managed service lease: ' .. tostring(err))
        end
    end)
end

function ServiceLifecycle.refresh_status_async(propertyTable)
    LrTasks.startAsyncTask(function()
        local _, err = refreshStatus(propertyTable)
        if err then
            logger:error('Could not refresh managed service status: ' .. tostring(err))
        end
    end)
end

function ServiceLifecycle.start_status_polling(propertyTable)
    if state.poll_running then
        return
    end

    state.poll_generation = state.poll_generation + 1
    local generation = state.poll_generation
    state.poll_running = true
    LrTasks.startAsyncTask(function()
        while generation == state.poll_generation do
            local _, err = refreshStatus(propertyTable)
            if err then
                logger:warn('Managed service status refresh failed: ' .. tostring(err))
            end

            for _ = 1, StatusPollSeconds do
                if generation ~= state.poll_generation then
                    break
                end
                LrTasks.sleep(1)
            end
        end
        state.poll_running = false
    end)
end

function ServiceLifecycle.stop_status_polling()
    state.poll_generation = state.poll_generation + 1
end

function ServiceLifecycle.ensure_service_running(propertyTable)
    return ensureServiceRunning(propertyTable)
end

return ServiceLifecycle
