#include "telemetry.hpp"

#include <algorithm>

namespace comet::dashboard
{

    void Telemetry::record(const TelemetrySample &sample)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        samples_.push_back(sample);

        if (samples_.size() > MAX_SAMPLES)
        {
            samples_.erase(samples_.begin());
        }
    }

    std::vector<TelemetrySample> Telemetry::history(
        std::size_t limit) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto count = std::min(limit, samples_.size());

        return {
            samples_.end() - count,
            samples_.end()};
    }

    void Telemetry::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
    }

}