#include "ppa/runtime/RuntimeSupport.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ppa::runtime {
namespace {

std::filesystem::path environment_path(const char* name) {
    const auto* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return {};
    }
    return std::filesystem::path(value);
}

}  // namespace

std::filesystem::path application_support_directory() {
#if defined(_WIN32)
    auto base = environment_path("LOCALAPPDATA");
    if (base.empty()) {
        base = environment_path("APPDATA");
    }
#elif defined(__APPLE__)
    auto base = environment_path("HOME");
    if (!base.empty()) {
        base /= "Library/Application Support";
    }
#else
    auto base = environment_path("XDG_STATE_HOME");
    if (base.empty()) {
        base = environment_path("HOME");
        if (!base.empty()) {
            base /= ".local/state";
        }
    }
#endif

    auto preferred = base.empty() ? std::filesystem::path{} : (base / "TheJury");
    if (!preferred.empty()) {
        std::error_code error;
        std::filesystem::create_directories(preferred, error);
        if (!error) {
            return preferred;
        }
    }

    auto fallback = std::filesystem::temp_directory_path() / "TheJury";
    std::error_code error;
    std::filesystem::create_directories(fallback, error);
    if (error) {
        throw std::runtime_error("failed to create writable runtime state directory");
    }
    return fallback;
}

RuntimePaths default_runtime_paths() {
    auto state_dir = application_support_directory() / "runtime";
    std::error_code error;
    std::filesystem::create_directories(state_dir, error);
    if (error) {
        state_dir = std::filesystem::temp_directory_path() / "TheJury" / "runtime";
        error.clear();
        std::filesystem::create_directories(state_dir, error);
        if (error) {
            throw std::runtime_error("failed to create writable runtime directory");
        }
    }

    return RuntimePaths{
        .state_dir = state_dir,
        .lock_dir = state_dir / "start.lock",
        .pid_file = state_dir / "ppa_service.pid",
        .launch_info_file = state_dir / "launch.json",
        .log_file = state_dir / "ppa_service.log",
    };
}

std::uint32_t current_process_id() {
#if defined(_WIN32)
    return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

std::string current_timestamp_utc() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif

    auto stream = std::ostringstream{};
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

}  // namespace ppa::runtime
