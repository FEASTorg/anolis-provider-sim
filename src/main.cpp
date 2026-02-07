#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "protocol.pb.h"
#include "handlers.hpp"
#include "transport/framed_stdio.hpp"

static void set_binary_mode_stdio()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

static void log_err(const std::string &msg)
{
    std::cerr << "anolis-provider-sim: " << msg << "\n";
}

int main(int argc, char **argv)
{
    // Parse --crash-after argument for chaos testing
    double crash_after_sec = -1.0;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--crash-after" && i + 1 < argc)
        {
            try
            {
                crash_after_sec = std::stod(argv[++i]);
            }
            catch (...)
            {
                log_err("invalid --crash-after value");
                return 1;
            }
        }
    }

    set_binary_mode_stdio();
    log_err("starting (transport=stdio+uint32_le)");

    // Start crash timer if requested (for supervision testing)
    if (crash_after_sec > 0.0)
    {
        log_err("CHAOS MODE: will crash after " + std::to_string(crash_after_sec) + " seconds");
        std::thread([crash_after_sec]()
                    {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<long long>(crash_after_sec * 1000)));
            std::cerr << "anolis-provider-sim: CRASHING NOW (exit 42)\n" << std::flush;
            std::exit(42); })
            .detach();
    }

    std::vector<uint8_t> frame;
    std::string io_err;

    while (true)
    {
        frame.clear();
        const bool ok = transport::read_frame(std::cin, frame, io_err);
        if (!ok)
        {
            if (io_err.empty())
            {
                log_err("EOF on stdin; exiting cleanly");
                return 0;
            }
            log_err(std::string("read_frame error: ") + io_err);
            return 2;
        }

        anolis::deviceprovider::v0::Request req;
        if (!req.ParseFromArray(frame.data(), static_cast<int>(frame.size())))
        {
            log_err("failed to parse Request protobuf");
            return 3;
        }

        anolis::deviceprovider::v0::Response resp;
        resp.set_request_id(req.request_id());
        resp.mutable_status()->set_code(anolis::deviceprovider::v0::Status::CODE_INTERNAL);
        resp.mutable_status()->set_message("uninitialized");

        // Dispatch
        if (req.has_hello())
        {
            handlers::handle_hello(req.hello(), resp);
        }
        else if (req.has_list_devices())
        {
            handlers::handle_list_devices(req.list_devices(), resp);
        }
        else if (req.has_describe_device())
        {
            handlers::handle_describe_device(req.describe_device(), resp);
        }
        else if (req.has_read_signals())
        {
            handlers::handle_read_signals(req.read_signals(), resp);
        }
        else if (req.has_call())
        {
            handlers::handle_call(req.call(), resp);
        }
        else if (req.has_get_health())
        {
            handlers::handle_get_health(req.get_health(), resp);
        }
        else
        {
            handlers::handle_unimplemented(resp);
        }

        std::string resp_bytes;
        if (!resp.SerializeToString(&resp_bytes))
        {
            log_err("failed to serialize Response protobuf");
            return 4;
        }

        if (!transport::write_frame(std::cout,
                                    reinterpret_cast<const uint8_t *>(resp_bytes.data()),
                                    resp_bytes.size(),
                                    io_err))
        {
            log_err(std::string("write_frame error: ") + io_err);
            return 5;
        }
    }
}
