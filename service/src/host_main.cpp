#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ppa/config/ServiceConfig.hpp"
#include "ppa/model.hpp"
#include "ppa/runtime/RuntimeSupport.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
extern char** environ;
#endif

namespace {
using json = nlohmann::json;
using namespace std::chrono_literals;

constexpr auto* kHost = "127.0.0.1";
constexpr auto kPort = 6464;
constexpr auto kStartTimeout = 10s;
constexpr auto kStopTimeout = 10s;
constexpr auto kProbeInterval = 250ms;

struct LockDirectory {
    explicit LockDirectory(std::filesystem::path path) : _path(std::move(path)) {
        std::filesystem::create_directories(_path.parent_path());
        std::error_code error;
        if (!std::filesystem::create_directory(_path, error)) {
            throw std::runtime_error("another start operation is already in progress");
        }
    }

    ~LockDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    std::filesystem::path _path;
};

httplib::Client make_client() {
    auto client = httplib::Client(kHost, kPort);
    client.set_connection_timeout(1, 0);
    client.set_read_timeout(2, 0);
    client.set_write_timeout(2, 0);
    return client;
}

bool health_ok() {
    auto client = make_client();
    const auto response = client.Get("/health");
    return response && response->status == 200;
}

std::optional<ppa::RuntimeStatus> fetch_runtime_status() {
    auto client = make_client();
    const auto response = client.Get("/v1/runtime/status");
    if (!response || response->status != 200) {
        return std::nullopt;
    }

    return json::parse(response->body).get<ppa::RuntimeStatus>();
}

void write_json_file(const std::filesystem::path& path, const json& payload) {
    std::filesystem::create_directories(path.parent_path());
    auto output = std::ofstream(path);
    if (!output) {
        throw std::runtime_error("failed to open runtime state file: " + path.string());
    }
    output << payload.dump(2) << '\n';
}

json stopped_status_json(const std::string& last_error = {}) {
    auto status = ppa::RuntimeStatus{};
    status.state = "stopped";
    status.reachable = false;
    status.service = ppa::runtime::kServiceName;
    status.version = ppa::runtime::kServiceVersion;
    status.last_error = last_error;
    return json(status);
}

std::filesystem::path service_binary_path() {
    const auto helper_path = ppa::current_executable_path();
#if defined(_WIN32)
    return helper_path.parent_path() / "ppa_service.exe";
#else
    return helper_path.parent_path() / "ppa_service";
#endif
}

void wait_for_health(const std::chrono::steady_clock::duration timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (health_ok()) {
            return;
        }
        std::this_thread::sleep_for(kProbeInterval);
    }
    throw std::runtime_error("service did not become healthy before timeout");
}

void cleanup_stale_lock(const ppa::runtime::RuntimePaths& paths) {
    std::error_code error;
    if (!std::filesystem::exists(paths.lock_dir, error)) {
        return;
    }

    if (health_ok()) {
        return;
    }

    const auto now = std::filesystem::file_time_type::clock::now();
    const auto modified = std::filesystem::last_write_time(paths.lock_dir, error);
    if (error) {
        return;
    }

    if (now - modified > 30s) {
        std::filesystem::remove_all(paths.lock_dir, error);
    }
}

#if defined(_WIN32)
std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("failed to convert UTF-8 path to wide string");
    }

    auto output = std::wstring(static_cast<std::size_t>(size - 1), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, output.data(), size) <= 0) {
        throw std::runtime_error("failed to convert UTF-8 path to wide string");
    }
    return output;
}

std::uint32_t spawn_service_detached(const std::filesystem::path& executable,
                                     const ppa::runtime::RuntimePaths& paths) {
    std::filesystem::create_directories(paths.state_dir);
    auto command_line = L"\"" + widen(executable.string()) + L"\"";
    auto mutable_command_line = std::vector<wchar_t>(command_line.begin(), command_line.end());
    mutable_command_line.push_back(L'\0');

    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;

    const auto log_path = widen(paths.log_file.string());
    auto log_handle = CreateFileW(log_path.c_str(),
                                  FILE_APPEND_DATA,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &security_attributes,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
    if (log_handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("failed to open service log file");
    }

    auto null_handle = CreateFileW(L"NUL",
                                   GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security_attributes,
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
    if (null_handle == INVALID_HANDLE_VALUE) {
        CloseHandle(log_handle);
        throw std::runtime_error("failed to open null device");
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = null_handle;
    startup_info.hStdOutput = log_handle;
    startup_info.hStdError = log_handle;

    PROCESS_INFORMATION process_info{};
    const auto success = CreateProcessW(widen(executable.string()).c_str(),
                                        mutable_command_line.data(),
                                        nullptr,
                                        nullptr,
                                        TRUE,
                                        DETACHED_PROCESS | CREATE_NO_WINDOW,
                                        nullptr,
                                        widen(executable.parent_path().string()).c_str(),
                                        &startup_info,
                                        &process_info);

    CloseHandle(null_handle);
    CloseHandle(log_handle);

    if (!success) {
        throw std::runtime_error("failed to spawn ppa_service");
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return static_cast<std::uint32_t>(process_info.dwProcessId);
}
#else
std::uint32_t spawn_service_detached(const std::filesystem::path& executable,
                                     const ppa::runtime::RuntimePaths& paths) {
    std::filesystem::create_directories(paths.state_dir);
    const auto log_path = paths.log_file.string();
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
#if defined(POSIX_SPAWN_SETSID)
    short flags = POSIX_SPAWN_SETSID;
    posix_spawnattr_setflags(&attributes, flags);
#endif

    const auto executable_text = executable.string();
    auto argv = std::vector<char*>{const_cast<char*>(executable_text.c_str()), nullptr};
    pid_t pid = 0;
    const auto result = posix_spawn(&pid,
                                    executable_text.c_str(),
                                    &actions,
                                    &attributes,
                                    argv.data(),
                                    environ);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);

    if (result != 0) {
        throw std::runtime_error("failed to spawn ppa_service");
    }
    return static_cast<std::uint32_t>(pid);
}
#endif

json start_service() {
    if (const auto status = fetch_runtime_status()) {
        if (status->state != "draining") {
            return json(*status);
        }

        const auto deadline = std::chrono::steady_clock::now() + kStopTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (!health_ok()) {
                break;
            }
            std::this_thread::sleep_for(kProbeInterval);
        }
    }

    const auto runtime_paths = ppa::runtime::default_runtime_paths();
    cleanup_stale_lock(runtime_paths);

    if (health_ok()) {
        if (const auto status = fetch_runtime_status()) {
            return json(*status);
        }
        return json{
            {"state", "running"},
            {"reachable", true},
            {"service", ppa::runtime::kServiceName},
            {"version", ppa::runtime::kServiceVersion},
        };
    }

    auto lock = LockDirectory(runtime_paths.lock_dir);
    const auto service_path = service_binary_path();
    if (!std::filesystem::exists(service_path)) {
        throw std::runtime_error("service binary not found next to helper: " + service_path.string());
    }

    const auto pid = spawn_service_detached(service_path, runtime_paths);
    write_json_file(runtime_paths.launch_info_file,
                    json{
                        {"service_path", service_path.string()},
                        {"pid", pid},
                        {"log_path", runtime_paths.log_file.string()},
                        {"started_at", ppa::runtime::current_timestamp_utc()},
                    });
    wait_for_health(kStartTimeout);
    if (const auto status = fetch_runtime_status()) {
        return json(*status);
    }
    return json{
        {"state", "running"},
        {"reachable", true},
        {"service", ppa::runtime::kServiceName},
        {"version", ppa::runtime::kServiceVersion},
        {"pid", pid},
    };
}

json stop_service() {
    if (!health_ok()) {
        return stopped_status_json();
    }

    auto client = make_client();
    const auto response = client.Post("/v1/runtime/shutdown", "", "application/json");
    if (!response || response->status >= 400) {
        throw std::runtime_error("service shutdown request failed");
    }

    const auto deadline = std::chrono::steady_clock::now() + kStopTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!health_ok()) {
            return stopped_status_json();
        }
        std::this_thread::sleep_for(kProbeInterval);
    }

    if (const auto status = fetch_runtime_status()) {
        return json(*status);
    }
    auto fallback = stopped_status_json("service shutdown timed out");
    fallback["state"] = "draining";
    fallback["reachable"] = true;
    return fallback;
}

json status_service() {
    if (const auto status = fetch_runtime_status()) {
        return json(*status);
    }
    if (health_ok()) {
        return json{
            {"state", "running"},
            {"reachable", true},
            {"service", ppa::runtime::kServiceName},
            {"version", ppa::runtime::kServiceVersion},
        };
    }
    return stopped_status_json();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::runtime_error("usage: ppa_service_host <start|stop|restart|status>");
        }

        const auto command = std::string(argv[1]);
        if (command == "start") {
            std::cout << start_service().dump() << '\n';
            return 0;
        }
        if (command == "stop") {
            std::cout << stop_service().dump() << '\n';
            return 0;
        }
        if (command == "restart") {
            (void)stop_service();
            std::cout << start_service().dump() << '\n';
            return 0;
        }
        if (command == "status") {
            std::cout << status_service().dump() << '\n';
            return 0;
        }

        throw std::runtime_error("unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
