#include "event_logger.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace comet::dashboard
{
    namespace
    {

        std::string escape_json(const std::string &text)
        {
            std::ostringstream out;

            for (unsigned char c : text)
            {
                switch (c)
                {
                case '"':
                    out << "\\\"";
                    break;
                case '\\':
                    out << "\\\\";
                    break;
                case '\n':
                    out << "\\n";
                    break;
                case '\r':
                    out << "\\r";
                    break;
                case '\t':
                    out << "\\t";
                    break;
                default:
                    if (c < 0x20)
                    {
                        out << "\\u"
                            << std::hex << std::setw(4)
                            << std::setfill('0') << static_cast<int>(c)
                            << std::dec;
                    }
                    else
                    {
                        out << static_cast<char>(c);
                    }
                }
            }

            return out.str();
        }

        const char *event_name(EventType type)
        {
            switch (type)
            {
            case EventType::Allocation:
                return "allocation";
            case EventType::Deallocation:
                return "deallocation";
            case EventType::AllocationFailure:
                return "allocation_failure";
            case EventType::WorkloadStarted:
                return "workload_started";
            case EventType::WorkloadPaused:
                return "workload_paused";
            case EventType::WorkloadReset:
                return "workload_reset";
            case EventType::ConfigurationChanged:
                return "configuration_changed";
            }

            return "unknown";
        }

        std::string timestamp_string(std::chrono::system_clock::time_point timestamp)
        {
            const auto value = std::chrono::system_clock::to_time_t(timestamp);
            std::tm utc{};

#ifdef _WIN32
            gmtime_s(&utc, &value);
#else
            gmtime_r(&value, &utc);
#endif

            std::ostringstream out;
            out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
            return out.str();
        }

        std::string event_json(const Event &event)
        {
            std::ostringstream out;
            std::ostringstream worker;

            if (event.worker_id != std::thread::id{})
            {
                worker << event.worker_id;
            }

            out << "{\"id\":" << event.id
                << ",\"type\":\"" << event_name(event.type)
                << "\",\"timestamp\":\"" << timestamp_string(event.timestamp)
                << "\",\"size\":" << event.size
                << ",\"success\":" << (event.success ? "true" : "false")
                << ",\"worker_id\":\"" << worker.str()
                << "\",\"message\":\"" << escape_json(event.message)
                << "\"}";

            return out.str();
        }

    }

    EventLogger::EventLogger(const std::string &path)
        : file_(path, std::ios::out | std::ios::app)
    {
        if (!file_)
        {
            throw std::runtime_error("Cannot open event log: " + path);
        }
    }

    Event EventLogger::record(
        EventType type,
        std::size_t size,
        bool success,
        std::string message)
    {
        Event event;
        event.type = type;
        event.timestamp = std::chrono::system_clock::now();
        event.size = size;
        event.success = success;
        event.worker_id = std::this_thread::get_id();
        event.message = std::move(message);

        std::lock_guard<std::mutex> lock(mutex_);

        event.id = next_id_++;

        file_ << event_json(event) << '\n';
        file_.flush();

        if (!file_)
        {
            throw std::runtime_error("Failed to write event log");
        }

        events_.push_back(event);

        if (events_.size() > MAX_EVENTS)
        {
            events_.erase(events_.begin());
        }

        return event;
    }

    std::vector<Event> EventLogger::recent(std::size_t limit)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto count = std::min(limit, events_.size());

        return {
            events_.end() - count,
            events_.end()};
    }

    std::string EventLogger::recent_json(std::size_t limit) {
    const auto events = recent(limit);

    std::ostringstream out;
    out << "[";

    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i > 0) {
            out << ",";
        }

        out << event_json(events[i]);
    }

    out << "]";
    return out.str();
}
}