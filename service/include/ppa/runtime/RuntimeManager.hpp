#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "ppa/model.hpp"
#include "ppa/runtime/RuntimeSupport.hpp"

namespace ppa {

class CritiqueService;

class RuntimeManager {
public:
    struct Options {
        int default_lease_ttl_seconds{runtime::kDefaultLeaseTtlSeconds};
        int shutdown_grace_seconds{runtime::kDefaultShutdownGraceSeconds};
        int monitor_interval_ms{runtime::kDefaultMonitorIntervalMs};
    };

    RuntimeManager();
    explicit RuntimeManager(Options options);
    ~RuntimeManager();

    RuntimeManager(const RuntimeManager&) = delete;
    RuntimeManager& operator=(const RuntimeManager&) = delete;

    void set_stop_callback(std::function<void()> callback);
    void set_last_error(std::string message);

    [[nodiscard]] RuntimeStatus status(const CritiqueService& service);
    [[nodiscard]] RuntimeLeaseResponse renew_lease(const RuntimeLeaseRequest& request);
    [[nodiscard]] RuntimeStatus release_lease(const CritiqueService& service, const std::string& instance_id);
    [[nodiscard]] RuntimeStatus request_shutdown(const CritiqueService& service);

    class JobGuard {
    public:
        JobGuard() = default;
        explicit JobGuard(RuntimeManager* owner) : _owner(owner) {}
        JobGuard(const JobGuard&) = delete;
        JobGuard& operator=(const JobGuard&) = delete;
        JobGuard(JobGuard&& other) noexcept : _owner(other._owner) { other._owner = nullptr; }
        JobGuard& operator=(JobGuard&& other) noexcept {
            if (this != &other) {
                release();
                _owner = other._owner;
                other._owner = nullptr;
            }
            return *this;
        }
        ~JobGuard() { release(); }

        explicit operator bool() const { return _owner != nullptr; }

    private:
        void release();
        RuntimeManager* _owner{nullptr};
    };

    [[nodiscard]] JobGuard try_begin_job();

private:
    using Clock = std::chrono::steady_clock;

    struct LeaseEntry {
        std::string client;
        Clock::time_point expires_at;
    };

    void monitor_loop();
    void finish_job();
    void prune_expired_locked(Clock::time_point now);
    [[nodiscard]] RuntimeStatus status_locked(const CritiqueService& service, Clock::time_point now) const;
    [[nodiscard]] std::string state_locked(Clock::time_point now) const;
    void maybe_request_stop_locked(Clock::time_point now);
    void notify_stop_callback(std::function<void()> callback);
    void write_pid_file() const;
    void remove_pid_file() const;

    Options _options;
    runtime::RuntimePaths _paths;
    Clock::time_point _started_at;
    mutable std::mutex _mutex;
    mutable std::unordered_map<std::string, LeaseEntry> _leases;
    mutable std::string _last_error;
    std::function<void()> _stop_callback;
    std::thread _monitor_thread;
    std::condition_variable _condition;
    bool _stop_monitor{false};
    bool _stop_issued{false};
    bool _explicit_shutdown{false};
    int _jobs_in_flight{0};
    std::optional<Clock::time_point> _lease_shutdown_deadline;
    std::optional<Clock::time_point> _explicit_stop_deadline;
};

}  // namespace ppa
