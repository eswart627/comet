#pragma once

#include "types.hpp"

#include <mutex>
#include <vector>

namespace comet::dashboard
{

    class Telemetry
    {
    public:
        void record(const TelemetrySample &sample);

        std::vector<TelemetrySample> history(
            std::size_t limit = 300) const;

        void clear();

    private:
        mutable std::mutex mutex_;
        std::vector<TelemetrySample> samples_;

        static constexpr std::size_t MAX_SAMPLES = 3600;
    };

}