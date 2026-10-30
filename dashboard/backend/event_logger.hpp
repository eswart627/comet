#pragma once

#include "types.hpp"

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace comet::dashboard {

class EventLogger {
public:
    explicit EventLogger(const std::string& path);

    Event record(
        EventType type,
        std::size_t size = 0,
        bool success = true,
        std::string message = {});

    std::vector<Event> recent(std::size_t limit = 100);
    std::string recent_json(std::size_t limit = 100);

private:
    std::ofstream file_;
    std::mutex mutex_;
    std::vector<Event> events_;
    std::uint64_t next_id_ = 1;

    static constexpr std::size_t MAX_EVENTS = 1000;
};

}