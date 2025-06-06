﻿// File: NetworkCommon.h
// RiftForged Game Development
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include <optional>          // For std::optional
#include <vector>            // While S2C_Response now uses DetachedBuffer, std::vector might be used elsewhere.
#include "flatbuffers/flatbuffers.h" // For flatbuffers::DetachedBuffer.

// We no longer need to include GamePacketHeader.h just for the old MessageType enum.
// The GamePacketHeader is for the transport layer; S2C_Response is for the application layer.
// #include "GamePacketHeader.h" // Removed this include

#include "NetworkEndpoint.h" // For RiftForged::Networking::NetworkEndpoint.

// Include the FlatBuffers generated S2C messages header.
// This is necessary to get the `S2C_UDP_Payload` enum, which defines your
// application-level S2C message types.
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // Assuming this is the correct path to your generated S2C FlatBuffers header.

namespace RiftForged {
    namespace Networking {

        /**
         * @brief Represents a Server-to-Client (S2C) response generated by an application message handler.
         * This structure encapsulates the necessary information for the network layer (UDPPacketHandler)
         * to send a response back to a client or broadcast it.
         */
        struct S2C_Response {
            // The serialized FlatBuffer data for the S2C message.
            // `flatbuffers::DetachedBuffer` is efficient for passing ownership of serialized data
            // without extra copies.
            flatbuffers::DetachedBuffer data;

            // The application-level message type of the FlatBuffer payload.
            // This enum comes directly from your FlatBuffers S2C schema.
            UDP::S2C::S2C_UDP_Payload flatbuffer_payload_type; // Changed from MessageType to FlatBuffers S2C enum

            // Flag indicating whether this response should be broadcast to all relevant clients.
            bool broadcast = false;

            // The specific network endpoint (IP:Port) to which this response should be sent
            // if it's not a broadcast message.
            NetworkEndpoint specific_recipient;

            // Default constructor.
            S2C_Response() = default;

            // Constructor to initialize fields, making it easier to create responses.
            S2C_Response(flatbuffers::DetachedBuffer&& data_buf,
                UDP::S2C::S2C_UDP_Payload payload_type,
                bool is_broadcast,
                const NetworkEndpoint& recipient_ep = NetworkEndpoint()) // Default construct for broadcast or if not set
                : data(std::move(data_buf)),
                flatbuffer_payload_type(payload_type),
                broadcast(is_broadcast),
                specific_recipient(recipient_ep)
            {
            }
        };

    } // namespace Networking
} // namespace RiftForged