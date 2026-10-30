#include "workload.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <algorithm>
#include <csignal>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

namespace
{

    comet::dashboard::Workload workload;

    int read_integer(const std::string &body,
                     const std::string &key,
                     int fallback)
    {
        const auto pos = body.find("\"" + key + "\"");

        if (pos == std::string::npos)
        {
            return fallback;
        }

        const auto colon = body.find(':', pos);

        if (colon == std::string::npos)
        {
            return fallback;
        }

        try
        {
            return std::stoi(body.substr(colon + 1));
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::string state_json()
    {
        const auto s = workload.snapshot();
        const auto c = workload.configuration();

        std::ostringstream out;

        out << "{"
            << "\"running\":" << (workload.running() ? "true" : "false")
            << ",\"pool_size\":" << s.pool_size
            << ",\"allocated_bytes\":" << s.allocated_bytes
            << ",\"free_bytes\":" << s.free_bytes
            << ",\"live_allocations\":" << s.live_allocations
            << ",\"total_allocations\":" << s.total_allocations
            << ",\"total_deallocations\":" << s.total_deallocations
            << ",\"failed_allocations\":" << s.failed_allocations
            << ",\"allocation_size\":" << c.allocation_size
            << ",\"thread_count\":" << c.thread_count
            << ",\"deallocation_probability\":"
            << c.deallocation_probability
            << "}";

        return out.str();
    }

    std::string history_json()
    {
        const auto samples = workload.history();

        std::ostringstream out;
        out << "{\"samples\":[";

        bool first = true;

        for (const auto &s : samples)
        {
            if (!first)
                out << ',';
            first = false;

            const auto time = std::chrono::duration_cast<
                                  std::chrono::milliseconds>(
                                  s.timestamp.time_since_epoch())
                                  .count();

            out << "{\"timestamp_ms\":" << time
                << ",\"pool_size\":" << s.pool_size
                << ",\"allocated_bytes\":" << s.allocated_bytes
                << ",\"free_bytes\":" << s.free_bytes
                << ",\"live_allocations\":" << s.live_allocations
                << ",\"total_allocations\":" << s.total_allocations
                << ",\"total_deallocations\":" << s.total_deallocations
                << ",\"failed_allocations\":" << s.failed_allocations
                << '}';
        }

        out << "]}";
        return out.str();
    }

    std::string memory_map_json()
    {
        const auto blocks = workload.memory_map();
        const auto s = workload.snapshot();

        std::ostringstream out;
        out << "{\"pool_size\":" << s.pool_size << ",\"blocks\":[";

        bool first = true;
        for (const auto &b : blocks)
        {
            if (!first)
                out << ',';
            first = false;

            out << "{\"offset\":" << b.offset
                << ",\"span\":" << b.span
                << ",\"requested_size\":" << b.requested_size
                << ",\"free\":" << (b.free ? "true" : "false")
                << '}';
        }

        out << "]}";
        return out.str();
    }

    http::response<http::string_body> response(
        http::status status,
        const std::string &body,
        unsigned version = 11)
    {

        http::response<http::string_body> res{status, version};

        res.set(http::field::server, "Comet-Dashboard");
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin,
                "*");
        res.set(http::field::access_control_allow_methods,
                "GET, POST, OPTIONS");
        res.set(http::field::access_control_allow_headers,
                "Content-Type");

        res.body() = body;
        res.prepare_payload();

        return res;
    }

    void handle_http(tcp::socket socket)
    {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;

        beast::error_code ec;
        http::read(socket, buffer, req, ec);

        if (ec)
        {
            return;
        }

        const std::string target(req.target());
        const std::string body = req.body();

        auto res = response(http::status::ok, "{}",
                            req.version());

        try
        {
            if (req.method() == http::verb::options)
            {
                res = response(http::status::no_content, "",
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/state")
            {
                res = response(http::status::ok, state_json(),
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/history")
            {
                res = response(http::status::ok, history_json(),
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/memory-map")
            {
                res = response(http::status::ok, memory_map_json(),
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/logs")
            {
                res = response(
                    http::status::ok,
                    "{\"events\":" + workload.logger().recent_json(100) + "}",
                    req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/start")
            {
                workload.start();
                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/pause")
            {
                workload.pause();
                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/reset")
            {
                workload.reset();
                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/config")
            {
                auto config = workload.configuration();

                config.allocation_size = static_cast<std::size_t>(
                    std::clamp(read_integer(
                                   body, "allocation_size",
                                   static_cast<int>(config.allocation_size)),
                               1, 1024 * 1024));

                config.thread_count = std::clamp(
                    read_integer(body, "thread_count",
                                 config.thread_count),
                    1, 16);

                config.deallocation_probability = std::clamp(
                    read_integer(body, "deallocation_probability",
                                 config.deallocation_probability),
                    0, 100);

                workload.configure(config);

                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else
            {
                res = response(http::status::not_found,
                               "{\"error\":\"not found\"}",
                               req.version());
            }
        }
        catch (const std::exception &e)
        {
            // Avoid returning internal implementation details to clients.
            res = response(http::status::internal_server_error,
                           "{\"error\":\"request failed\"}",
                           req.version());

            std::cerr << "Dashboard request failed: "
                      << e.what() << '\n';
        }

        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }

    void handle_websocket(tcp::socket socket)
    {
        try
        {
            websocket::stream<tcp::socket> ws(std::move(socket));

            ws.accept();

            ws.text(true);

            while (true)
            {
                const auto message = state_json();

                ws.write(asio::buffer(message));

                std::this_thread::sleep_for(
                    std::chrono::seconds(1));
            }
        }
        catch (const std::exception &)
        {
            // Client disconnects are normal.
        }
    }

    void handle_connection(tcp::socket socket)
    {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;

        beast::error_code ec;
        http::read(socket, buffer, req, ec);

        if (ec)
        {
            return;
        }

        if (websocket::is_upgrade(req) &&
            req.target() == "/ws")
        {
            try
            {
                websocket::stream<tcp::socket> ws(std::move(socket));

                ws.accept(req);
                ws.text(true);

                while (true)
                {
                    const std::string message =
                        "{\"type\":\"snapshot\",\"state\":" +
                        state_json() +
                        ",\"events\":" +
                        workload.logger().recent_json(20) +
                        ",\"memory_map\":" +
                        memory_map_json() +
                        "}";

                    ws.write(asio::buffer(message));

                    std::this_thread::sleep_for(
                        std::chrono::seconds(1));
                }
            }
            catch (const std::exception &)
            {
                // The browser closed the WebSocket.
            }

            return;
        }

        // Reconstructing the request is avoided by handling HTTP here.
        const std::string target(req.target());
        const std::string body = req.body();

        auto res = response(http::status::not_found,
                            "{\"error\":\"not found\"}",
                            req.version());

        try
        {
            if (req.method() == http::verb::options)
            {
                res = response(http::status::no_content, "",
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/state")
            {
                res = response(http::status::ok, state_json(),
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/history")
            {
                res = response(http::status::ok, history_json(),
                               req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/memory-map")
            {
                res = response(http::status::ok, memory_map_json(),
                               req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/start")
            {
                workload.start();
                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/pause")
            {
                workload.pause();
                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/reset")
            {
                workload.reset();
                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::post &&
                     target == "/api/config")
            {
                auto c = workload.configuration();

                c.allocation_size = static_cast<std::size_t>(
                    std::clamp(read_integer(
                                   body, "allocation_size",
                                   static_cast<int>(c.allocation_size)),
                               1, 1024 * 1024));

                c.thread_count = std::clamp(
                    read_integer(body, "thread_count", c.thread_count),
                    1, 16);

                c.deallocation_probability = std::clamp(
                    read_integer(body, "deallocation_probability",
                                 c.deallocation_probability),
                    0, 100);

                workload.configure(c);

                res = response(http::status::ok,
                               "{\"ok\":true}", req.version());
            }
            else if (req.method() == http::verb::get &&
                     target == "/api/logs")
            {
                res = response(
                    http::status::ok,
                    "{\"events\":" + workload.logger().recent_json(100) + "}",
                    req.version());
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Request error: " << e.what() << '\n';

            res = response(http::status::internal_server_error,
                           "{\"error\":\"request failed\"}",
                           req.version());
        }

        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }

}

int main()
{
    try
    {
        asio::io_context context{1};
        tcp::acceptor acceptor{
            context,
            tcp::endpoint(asio::ip::make_address("127.0.0.1"), 8787)};

        std::cout
            << "Comet dashboard backend listening on "
            << "http://127.0.0.1:8787\n";

        while (true)
        {
            tcp::socket socket{context};
            acceptor.accept(socket);

            std::thread(
                handle_connection,
                std::move(socket))
                .detach();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << '\n';
        return 1;
    }
}