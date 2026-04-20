#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "ppa/runtime/Version.hpp"

namespace ppa::runtime {

constexpr int kDefaultLeaseTtlSeconds = 15;
constexpr int kDefaultShutdownGraceSeconds = 5;
constexpr int kDefaultMonitorIntervalMs = 500;

struct RuntimePaths {
    std::filesystem::path state_dir;
    std::filesystem::path lock_dir;
    std::filesystem::path pid_file;
    std::filesystem::path launch_info_file;
    std::filesystem::path log_file;
};

[[nodiscard]] std::filesystem::path application_support_directory();
[[nodiscard]] RuntimePaths default_runtime_paths();
[[nodiscard]] std::uint32_t current_process_id();
[[nodiscard]] std::string current_timestamp_utc();

}  // namespace ppa::runtime
