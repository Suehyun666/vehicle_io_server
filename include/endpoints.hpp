#pragma once

namespace endpoints {
    inline constexpr const char* kHalPub  = "ipc:///tmp/sdv_hal.ipc";   // HAL → 모두
    inline constexpr const char* kFeatPub = "ipc:///tmp/sdv_feat.ipc";  // Feature → HAL/Arbiter
}
