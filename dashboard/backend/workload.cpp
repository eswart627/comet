#include "workload.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>

namespace comet::dashboard
{

    Workload::Workload()
        : logger_("dashboard/backend/logs/comet-events.jsonl")
    {
        comet::AllocatorConfig config;
        config.pool_size = 64 * 1024 * 1024;

        allocator_ = std::make_unique<comet::Allocator>(config);
    }

    Workload::~Workload()
    {
        pause();
    }

    void Workload::configure(const WorkloadConfig &config)
    {
        std::lock_guard<std::mutex> lock(control_mutex_);

        if (running_)
        {
            throw std::runtime_error(
                "Pause the workload before changing configuration");
        }

        config_.allocation_size =
            std::clamp<std::size_t>(config.allocation_size, 1, 1024 * 1024);

        config_.thread_count = std::clamp(config.thread_count, 1, 16);

        config_.deallocation_probability =
            std::clamp(config.deallocation_probability, 0, 100);

        logger_.record(
            EventType::ConfigurationChanged,
            config_.allocation_size,
            true,
            "Workload configuration updated");
    }

    WorkloadConfig Workload::configuration() const
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        return config_;
    }

    bool Workload::running() const
    {
        return running_.load();
    }

    void Workload::start()
    {
        std::lock_guard<std::mutex> lock(control_mutex_);

        if (running_)
        {
            return;
        }

        stop_requested_ = false;
        running_ = true;

        const auto config = config_;

        for (int i = 0; i < config.thread_count; ++i)
        {
            workers_.emplace_back([this, config, i]()
                                  {
            std::mt19937 rng(
                static_cast<unsigned>(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count()) + i);

            std::vector<void*> live;

            while (!stop_requested_.load()) {
                void* ptr = nullptr;

                {
                    std::lock_guard<std::mutex> lock(allocator_mutex_);
                    ptr = allocator_->allocate(config.allocation_size);
                }

                if (ptr) {
                    logger_.record(
                        EventType::Allocation,
                        config.allocation_size);
                    live.push_back(ptr);
                } else {
                    logger_.record(
                        EventType::AllocationFailure,
                        config.allocation_size,
                        false);
                }

                if (!live.empty() &&
                    static_cast<int>(rng() % 100) <
                        config.deallocation_probability) {
                    const auto index =
                        static_cast<std::size_t>(rng()) % live.size();

                    bool ok;

                    {
                        std::lock_guard<std::mutex> lock(allocator_mutex_);
                        ok = allocator_->deallocate(live[index]);
                    }

                    logger_.record(
                        EventType::Deallocation,
                        config.allocation_size,
                        ok);

                    live[index] = live.back();
                    live.pop_back();
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(12));
            }

            for (void* ptr : live) {
                bool ok;

                {
                    std::lock_guard<std::mutex> lock(allocator_mutex_);
                    ok = allocator_->deallocate(ptr);
                }

                logger_.record(
                    EventType::Deallocation, 0, ok, "Worker cleanup");
            } });
        }

        sampler_ = std::thread(&Workload::sample_loop, this);

        logger_.record(EventType::WorkloadStarted);
    }

    void Workload::stop_workers()
    {
        stop_requested_ = true;

        for (auto &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        workers_.clear();

        if (sampler_.joinable())
        {
            sampler_.join();
        }

        running_ = false;
    }

    void Workload::pause()
    {
        {
            std::lock_guard<std::mutex> lock(control_mutex_);

            if (!running_ && workers_.empty())
            {
                return;
            }

            stop_workers();
        }

        logger_.record(EventType::WorkloadPaused);
    }

    void Workload::reset()
    {
        pause();

        std::lock_guard<std::mutex> control_lock(control_mutex_);

        {
            std::lock_guard<std::mutex> allocator_lock(allocator_mutex_);

            comet::AllocatorConfig config;
            config.pool_size = 64 * 1024 * 1024;

            allocator_ = std::make_unique<comet::Allocator>(config);
        }

        telemetry_.clear();

        logger_.record(EventType::WorkloadReset);
    }

    TelemetrySample Workload::snapshot() const
    {
        TelemetrySample sample;
        sample.timestamp = std::chrono::system_clock::now();

        std::lock_guard<std::mutex> lock(allocator_mutex_);
        const auto stats = allocator_->stats();

        sample.pool_size = stats.pool_size;
        sample.allocated_bytes = stats.allocated_bytes;
        sample.free_bytes = stats.free_bytes;
        sample.live_allocations = stats.live_allocations;
        sample.total_allocations = stats.total_allocations;
        sample.total_deallocations = stats.total_deallocations;
        sample.failed_allocations = stats.failed_allocations;

        return sample;
    }

    void Workload::sample_loop()
    {
        while (!stop_requested_.load())
        {
            telemetry_.record(snapshot());

            std::this_thread::sleep_for(
                std::chrono::seconds(1));
        }

        telemetry_.record(snapshot());
    }

    std::vector<TelemetrySample> Workload::history(
        std::size_t limit) const
    {
        return telemetry_.history(limit);
    }

    std::vector<comet::Allocator::MemoryBlockInfo> Workload::memory_map() const
    {
        std::lock_guard<std::mutex> lock(allocator_mutex_);
        if (!allocator_)
        {
            return {};
        }
        return allocator_->memory_map();
    }

    EventLogger &Workload::logger()
    {
        return logger_;
    }

}