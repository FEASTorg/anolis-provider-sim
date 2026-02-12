#pragma once

#include "protocol.pb.h"

namespace handlers {

void handle_hello(const anolis::deviceprovider::v1::HelloRequest &req,
                  anolis::deviceprovider::v1::Response &resp);

void handle_wait_ready(const anolis::deviceprovider::v1::WaitReadyRequest &req,
                       anolis::deviceprovider::v1::Response &resp);

void handle_list_devices(
    const anolis::deviceprovider::v1::ListDevicesRequest &req,
    anolis::deviceprovider::v1::Response &resp);

void handle_describe_device(
    const anolis::deviceprovider::v1::DescribeDeviceRequest &req,
    anolis::deviceprovider::v1::Response &resp);

void handle_read_signals(
    const anolis::deviceprovider::v1::ReadSignalsRequest &req,
    anolis::deviceprovider::v1::Response &resp);

void handle_call(const anolis::deviceprovider::v1::CallRequest &req,
                 anolis::deviceprovider::v1::Response &resp);

void handle_get_health(const anolis::deviceprovider::v1::GetHealthRequest &req,
                       anolis::deviceprovider::v1::Response &resp);

void handle_unimplemented(anolis::deviceprovider::v1::Response &resp);

} // namespace handlers
