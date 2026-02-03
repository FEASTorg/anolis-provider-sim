#pragma once

#include "protocol.pb.h"

namespace sim_health
{

    inline anolis::deviceprovider::v0::ProviderHealth make_provider_health_ok()
    {
        anolis::deviceprovider::v0::ProviderHealth h;
        h.set_state(anolis::deviceprovider::v0::ProviderHealth::STATE_OK);
        h.set_message("ok");
        (*h.mutable_metrics())["impl"] = "sim";
        return h;
    }

} // namespace sim_health
