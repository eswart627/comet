#pragma once

#include "event_logger.hpp"
#include "telemetry.hpp"
#include "types.hpp"

#include "allocator.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace comet::dashboard
{

    class Workload
    {
    public:
        Workload();
        ~Workload();

        Workload(const Workload &) = delete;
        Workload &operator=(const Workload &) = delete;

        void start();
        void pause();
        void reset();

        void configure(const WorkloadConfig &config);

        bool running() const;
        WorkloadConfig configuration() const;

        TelemetrySample snapshot() const;
        std::vector<TelemetrySample> history(std::size_t limit = 300) const;

        std::vector<comet::Allocator::MemoryBlockInfo> memory_map() const;

        EventLogger &logger();

    private:
        void stop_workers();
        void sample_loop();

        mutable std::mutex control_mutex_;
        mutable std::mutex allocator_mutex_;

        WorkloadConfig config_;
        std::unique_ptr<comet::Allocator> allocator_;

        std::atomic<bool> running_{false};
        std::atomic<bool> stop_requested_{false};

        std::vector<std::thread> workers_;
        std::thread sampler_;

        EventLogger logger_;
        Telemetry telemetry_;
    };

}