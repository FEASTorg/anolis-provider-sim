#pragma once

#include "protocol.pb.h"

namespace handlers
{

    void handle_hello(const anolis::deviceprovider::v0::HelloRequest &req,
                      anolis::deviceprovider::v0::Response &resp);

    void handle_list_devices(const anolis::deviceprovider::v0::ListDevicesRequest &req,
                             anolis::deviceprovider::v0::Response &resp);

    void handle_describe_device(const anolis::deviceprovider::v0::DescribeDeviceRequest &req,
                                anolis::deviceprovider::v0::Response &resp);

    void handle_read_signals(const anolis::deviceprovider::v0::ReadSignalsRequest &req,
                             anolis::deviceprovider::v0::Response &resp);

    void handle_call(const anolis::deviceprovider::v0::CallRequest &req,
                     anolis::deviceprovider::v0::Response &resp);

    void handle_get_health(const anolis::deviceprovider::v0::GetHealthRequest &req,
                           anolis::deviceprovider::v0::Response &resp);

    void handle_unimplemented(anolis::deviceprovider::v0::Response &resp);

} // namespace handlers
