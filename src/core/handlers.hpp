#pragma once

/**
 * @file handlers.hpp
 * @brief ADPP request handlers for anolis-provider-sim.
 */

#include "protocol.pb.h"

namespace handlers {

/** @brief Handle the ADPP `Hello` handshake and advertise provider metadata. */
void handle_hello(const anolis::deviceprovider::v1::HelloRequest &req,
                  anolis::deviceprovider::v1::Response &resp);

/** @brief Report provider readiness and startup diagnostics. */
void handle_wait_ready(const anolis::deviceprovider::v1::WaitReadyRequest &req,
                       anolis::deviceprovider::v1::Response &resp);

/** @brief List active devices and optional device-health snapshots. */
void handle_list_devices(
    const anolis::deviceprovider::v1::ListDevicesRequest &req,
    anolis::deviceprovider::v1::Response &resp);

/** @brief Describe one device and its capability surface. */
void handle_describe_device(
    const anolis::deviceprovider::v1::DescribeDeviceRequest &req,
    anolis::deviceprovider::v1::Response &resp);

/** @brief Read signal values for one device through the device manager. */
void handle_read_signals(
    const anolis::deviceprovider::v1::ReadSignalsRequest &req,
    anolis::deviceprovider::v1::Response &resp);

/** @brief Execute a device function by numeric or resolved symbolic identifier. */
void handle_call(const anolis::deviceprovider::v1::CallRequest &req,
                 anolis::deviceprovider::v1::Response &resp);

/** @brief Return provider and device health derived from startup state. */
void handle_get_health(const anolis::deviceprovider::v1::GetHealthRequest &req,
                       anolis::deviceprovider::v1::Response &resp);

/** @brief Return the standard unimplemented status for unsupported operations. */
void handle_unimplemented(anolis::deviceprovider::v1::Response &resp);

} // namespace handlers
