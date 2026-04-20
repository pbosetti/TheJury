#include "ppa/runtime/RuntimeManager.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

#include "ppa/api/ApiError.hpp"
#include "ppa/core/CritiqueService.hpp"

namespace ppa {
namespace {
constexpr auto kExplicitStopDelay = std::chrono::milliseconds(100);
}  // namespace

RuntimeManager::RuntimeManager()
    : RuntimeManager(Options{}) {}

RuntimeManager::RuntimeManager(Options options)
    : _options(options),
      _paths(runtime::default_runtime_paths()),
      _started_at(Clock::now()) {
    std::filesystem::create_directories(_paths.state_dir);
    write_pid_file();
    _monitor_thread = std::thread([this] { monitor_loop(); });
}

RuntimeManager::~RuntimeManager() {
    {
        auto lock = std::lock_guard(_mutex);
        _stop_monitor = true;
    }
    _condition.notify_all();
    if (_monitor_thread.joinable()) {
        _monitor_thread.join();
    }
    remove_pid_file();
}

void RuntimeManager::set_stop_callback(std::function<void()> callback) {
    auto lock = std::lock_guard(_mutex);
    _stop_callback = std::move(callback);
}

void RuntimeManager::set_last_error(std::string message) {
    auto lock = std::lock_guard(_mutex);
    _last_error = std::move(message);
}

RuntimeStatus RuntimeManager::status(const CritiqueService& service) {
    auto lock = std::lock_guard(_mutex);
    const auto now = Clock::now();
    prune_expired_locked(now);
    return status_locked(service, now);
}

RuntimeLeaseResponse RuntimeManager::renew_lease(const RuntimeLeaseRequest& request) {
    if (request.client.empty()) {
        throw api::ApiError(400, "invalid_request", "runtime lease client must not be empty");
    }
    if (request.instance_id.empty()) {
        throw api::ApiError(400, "invalid_request", "runtime lease instance_id must not be empty");
    }

    auto lock = std::lock_guard(_mutex);
    const auto now = Clock::now();
    prune_expired_locked(now);

    auto ttl_seconds = request.ttl_seconds;
    if (ttl_seconds <= 0) {
        ttl_seconds = _options.default_lease_ttl_seconds;
    }

    _leases[request.instance_id] = LeaseEntry{
        .client = request.client,
        .expires_at = now + std::chrono::seconds(ttl_seconds),
    };
    _lease_shutdown_deadline.reset();

    return RuntimeLeaseResponse{
        .state = state_locked(now),
        .expires_in_seconds = ttl_seconds,
        .active_lease_count = static_cast<int>(_leases.size()),
    };
}

RuntimeStatus RuntimeManager::release_lease(const CritiqueService& service, const std::string& instance_id) {
    if (instance_id.empty()) {
        throw api::ApiError(400, "invalid_request", "runtime lease instance_id must not be empty");
    }

    auto lock = std::lock_guard(_mutex);
    const auto now = Clock::now();
    prune_expired_locked(now);
    _leases.erase(instance_id);
    if (_leases.empty() && !_explicit_shutdown) {
        _lease_shutdown_deadline = now + std::chrono::seconds(_options.shutdown_grace_seconds);
    }
    maybe_request_stop_locked(now);
    return status_locked(service, now);
}

RuntimeStatus RuntimeManager::request_shutdown(const CritiqueService& service) {
    auto lock = std::lock_guard(_mutex);
    const auto now = Clock::now();
    prune_expired_locked(now);
    _explicit_shutdown = true;
    _explicit_stop_deadline.reset();
    maybe_request_stop_locked(now);
    _condition.notify_all();
    return status_locked(service, now);
}

RuntimeManager::JobGuard RuntimeManager::try_begin_job() {
    auto lock = std::lock_guard(_mutex);
    const auto now = Clock::now();
    prune_expired_locked(now);
    if (state_locked(now) == "draining") {
        return {};
    }

    ++_jobs_in_flight;
    return JobGuard{this};
}

void RuntimeManager::JobGuard::release() {
    if (_owner != nullptr) {
        _owner->finish_job();
        _owner = nullptr;
    }
}

void RuntimeManager::monitor_loop() {
    auto lock = std::unique_lock(_mutex);
    while (!_stop_monitor) {
        _condition.wait_for(lock, std::chrono::milliseconds(_options.monitor_interval_ms));
        if (_stop_monitor) {
            break;
        }

        const auto now = Clock::now();
        prune_expired_locked(now);
        maybe_request_stop_locked(now);

        if (_stop_issued && _stop_callback) {
            auto callback = _stop_callback;
            lock.unlock();
            notify_stop_callback(std::move(callback));
            lock.lock();
        }
    }
}

void RuntimeManager::finish_job() {
    auto lock = std::lock_guard(_mutex);
    if (_jobs_in_flight > 0) {
        --_jobs_in_flight;
    }
    const auto now = Clock::now();
    prune_expired_locked(now);
    maybe_request_stop_locked(now);
    _condition.notify_all();
}

void RuntimeManager::prune_expired_locked(Clock::time_point now) {
    for (auto it = _leases.begin(); it != _leases.end();) {
        if (it->second.expires_at <= now) {
            it = _leases.erase(it);
        } else {
            ++it;
        }
    }

    if (!_explicit_shutdown && _leases.empty() && !_lease_shutdown_deadline.has_value()) {
        _lease_shutdown_deadline = now + std::chrono::seconds(_options.shutdown_grace_seconds);
    }
    if (!_explicit_shutdown && !_leases.empty()) {
        _lease_shutdown_deadline.reset();
    }
}

RuntimeStatus RuntimeManager::status_locked(const CritiqueService& service, Clock::time_point now) const {
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - _started_at).count();
    return RuntimeStatus{
        .state = state_locked(now),
        .reachable = true,
        .service = runtime::kServiceName,
        .version = runtime::kServiceVersion,
        .pid = runtime::current_process_id(),
        .uptime_seconds = static_cast<std::uint64_t>(std::max<std::int64_t>(uptime, 0)),
        .jobs_in_flight = _jobs_in_flight,
        .active_lease_count = static_cast<int>(_leases.size()),
        .lease_ttl_seconds = _options.default_lease_ttl_seconds,
        .provider = service.config().semantic.default_provider,
        .model = service.config().ollama.model,
        .last_error = _last_error,
    };
}

std::string RuntimeManager::state_locked(Clock::time_point now) const {
    if (_explicit_shutdown) {
        return "draining";
    }
    if (_lease_shutdown_deadline.has_value() && now >= *_lease_shutdown_deadline) {
        return "draining";
    }
    return "running";
}

void RuntimeManager::maybe_request_stop_locked(Clock::time_point now) {
    if (_stop_issued) {
        return;
    }

    if (_explicit_shutdown) {
        if (_jobs_in_flight == 0) {
            if (!_explicit_stop_deadline.has_value()) {
                _explicit_stop_deadline = now + kExplicitStopDelay;
            }
            if (now >= *_explicit_stop_deadline) {
                _stop_issued = true;
            }
        } else {
            _explicit_stop_deadline.reset();
        }
        return;
    }

    if (_lease_shutdown_deadline.has_value() && _leases.empty() &&
        now >= *_lease_shutdown_deadline && _jobs_in_flight == 0) {
        _stop_issued = true;
    }
}

void RuntimeManager::notify_stop_callback(std::function<void()> callback) {
    if (callback) {
        callback();
    }
}

void RuntimeManager::write_pid_file() const {
    std::filesystem::create_directories(_paths.state_dir);
    auto output = std::ofstream(_paths.pid_file);
    if (output) {
        output << runtime::current_process_id() << '\n';
    }
}

void RuntimeManager::remove_pid_file() const {
    std::error_code error;
    std::filesystem::remove(_paths.pid_file, error);
}

}  // namespace ppa
