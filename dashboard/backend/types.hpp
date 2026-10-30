#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace comet::dashboard {

enum class EventType {
    Allocation,
    Deallocation,
    AllocationFailure,
    WorkloadStarted,
    WorkloadPaused,
    WorkloadReset,
    ConfigurationChanged
};

struct Event {
    std::uint64_t id = 0;
    EventType type = EventType::Allocation;
    std::chrono::system_clock::time_point timestamp;
    std::size_t size = 0;
    bool success = true;
    std::thread::id worker_id;
    std::string message;
};

struct TelemetrySample {
    std::chrono::system_clock::time_point timestamp;
    std::size_t pool_size = 0;
    std::size_t allocated_bytes = 0;
    std::size_t free_bytes = 0;
    std::size_t live_allocations = 0;
    std::size_t total_allocations = 0;
    std::size_t total_deallocations = 0;
    std::size_t failed_allocations = 0;
};

struct WorkloadConfig {
    std::size_t allocation_size = 64;
    int thread_count = 1;
    int deallocation_probability = 50;
};

}