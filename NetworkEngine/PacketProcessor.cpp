#include "PacketProcessor.h"
#include "MessageDispatcher.h" // For m_messageDispatcher.DispatchC2SMessage
// GamePacketHeader.h is included via PacketProcessor.h
// NetworkCommon.h (for S2C_Response) is included via PacketProcessor.h
// NetworkEndpoint.h is included via PacketProcessor.h -> NetworkCommon.h
#include <iostream>                     // Replace with your actual logging system
#include <cstring>                      // For memcpy

namespace RiftForged {
    namespace Networking {

        PacketProcessor::PacketProcessor(MessageDispatcher& dispatcher)
            : m_messageDispatcher(dispatcher)
            // If you add m_reliabilityManager:
            // , m_reliabilityManager(reliabilityManager) 
        {
            std::cout << "PacketProcessor: Initialized." << std::endl;
        }

        std::optional<S2C_Response> PacketProcessor::ProcessIncomingRawPacket(
            const char* raw_buffer,
            int raw_length,
            const NetworkEndpoint& sender_endpoint) {

            // 1. Validate basic packet size
            if (raw_length < static_cast<int>(GetGamePacketHeaderSize())) {
                std::cerr << "PacketProcessor: Packet from " << sender_endpoint.ToString()
                    << " too small (" << raw_length << " bytes) to contain GamePacketHeader." << std::endl;
                return std::nullopt; // Return empty optional: no valid response to generate from this error
            }

            // 2. Deserialize our custom GamePacketHeader from the start of the buffer
            GamePacketHeader header; // Our custom header struct
            std::memcpy(&header, raw_buffer, GetGamePacketHeaderSize());

            // 3. Validate Protocol ID
            if (header.protocolId != CURRENT_PROTOCOL_ID_VERSION) {
                std::cerr << "PacketProcessor: Mismatched protocol ID from " << sender_endpoint.ToString()
                    << ". Expected: " << CURRENT_PROTOCOL_ID_VERSION << ", Got: " << header.protocolId << std::endl;
                // TODO: Increment counter or handle version mismatch more actively
                return std::nullopt; // No valid response
            }

            // 4. TODO: Process Reliability Information from the header
            // Example:
            // bool should_dispatch_payload = m_reliabilityManager.ProcessIncomingHeader(sender_endpoint, header);
            // std::optional<S2C_Response> ack_response = m_reliabilityManager.GenerateAckResponseIfNeeded(sender_endpoint);
            // if (ack_response.has_value()) {
            //     // If reliability layer generated a direct ACK packet to send, send it.
            //     // This logic would be in WorkerThread based on what ProcessIncomingRawPacket returns.
            //     // For now, let's assume this function should return that ACK response.
            //     return ack_response; 
            // }
            // if (!should_dispatch_payload) {
            //     // e.g., it was a pure ACK, or a duplicate reliable packet already processed.
            //     return std::nullopt; // No game message to dispatch, no further response from this function.
            // }
            // For now, we'll bypass detailed reliability logic and proceed to dispatch.


            // 5. Determine the start and size of the FlatBuffer payload
            const uint8_t* flatbuffer_payload_ptr = reinterpret_cast<const uint8_t*>(raw_buffer + GetGamePacketHeaderSize());
            int flatbuffer_payload_size = raw_length - static_cast<int>(GetGamePacketHeaderSize());

            if (flatbuffer_payload_size < 0) {
                std::cerr << "PacketProcessor: Negative FlatBuffer payload size calculated from "
                    << sender_endpoint.ToString() << ". Header size: " << GetGamePacketHeaderSize()
                    << ", Total length: " << raw_length << std::endl;
                return std::nullopt; // Error, no valid response
            }

            // If the message type indicates there should be no payload, but there is, it could be an error.
            // Or, if payload_size is 0, but the messageType expects a payload.
            // For now, MessageDispatcher will handle FlatBuffer verification.
            // A simple check for payload_size == 0 could be done here if some MessageTypes are payload-less.
            // Example: if (header.messageType == MessageType::SomeTypeWithoutPayload && flatbuffer_payload_size > 0) { log error }

            // 6. Dispatch to the MessageDispatcher and return whatever S2C_Response it generates (if any)
            return m_messageDispatcher.DispatchC2SMessage(header, flatbuffer_payload_ptr, flatbuffer_payload_size, sender_endpoint);
        }

    } // namespace Networking
} // namespace RiftForged