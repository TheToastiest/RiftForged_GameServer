// File: NetworkCommon.h (example structure)
#pragma once

#include <optional>
#include <vector> // If S2C_Response uses it directly (currently not)
#include "flatbuffers/flatbuffers.h" // For flatbuffers::DetachedBuffer

#include "GamePacketHeader.h" // For RiftForged::Networking::MessageType
#include "NetworkEndpoint.h"  // For RiftForged::Networking::NetworkEndpoint

namespace RiftForged {
    namespace Networking {

        struct S2C_Response {
            flatbuffers::DetachedBuffer data;
            MessageType messageType; // Now correctly refers to the enum from GamePacketHeader.h
            bool broadcast = false;
            NetworkEndpoint specific_recipient;
        };

    } // namespace Networking
} // namespace RiftForged