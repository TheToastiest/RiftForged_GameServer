// File: UDPPacketHandler.h
// RiftForged Game Development
// Purpose: Handles UDP packet-level logic, including custom reliability,
//          and acts as a bridge between the raw Network IO layer and
//          the application-level MessageHandler.

#pragma once

#include "INetworkIOEvents.h"      // Implements this interface to receive events from UDPSocketAsync
#include "NetworkEndpoint.h"       // For representing remote client addresses
#include "GamePacketHeader.h"      // Defines GamePacketHeader structure (now simplified, no app MessageType)
#include "UDPReliabilityProtocol.h"// Defines ReliableConnectionState and associated reliability logic/types
#include "NetworkCommon.h"         // For common network types like S2C_Response (now uses FB S2C payload type)

// Include FlatBuffers generated headers that define payload enums
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_UDP_Payload
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_UDP_Payload

#include <string>
#include <vector>
#include <map>
#include <memory>      // For std::shared_ptr
#include <mutex>       // For std::mutex
#include <thread>      // For std::thread (reliability thread)
#include <atomic>      // For std::atomic_bool
#include <optional>    // For std::optional (handling responses from MessageHandler)
#include <chrono>      // For std::chrono::steady_clock

// Forward declarations for interfaces this class will use
namespace RiftForged {
    namespace Networking {
        class INetworkIO;          // Interface to the underlying network transport (e.g., UDPSocketAsync)
        class IMessageHandler;     // Interface to the application message processor
        struct OverlappedIOContext; // Defined in OverlappedIOContext.h, passed by INetworkIOEvents
    }
    namespace Server {
        class GameServerEngine;    // Reference to the GameServerEngine
    }
}

// Constants for the reliability protocol managed by this PacketHandler.
const int RELIABILITY_THREAD_SLEEP_MS_PKT = 20; // How often the reliability thread wakes up.
// DEFAULT_RTO_MS_PKT and DEFAULT_MAX_RETRIES_PKT are now defined/used in UDPReliabilityProtocol.h
const int STALE_CONNECTION_TIMEOUT_SECONDS_PKT = 60; // Duration of inactivity before a connection is considered stale.


namespace RiftForged {
    namespace Networking {

        class UDPPacketHandler : public INetworkIOEvents {
        public:
            /**
             * @brief Constructor for UDPPacketHandler.
             * @param networkIO A pointer to an INetworkIO compliant object (e.g., UDPSocketAsync instance)
             * which this handler will use to send raw data.
             * @param messageHandler A pointer to an IMessageHandler compliant object which will
             * process application-level payloads extracted by this handler.
             * @param gameServerEngine A reference to the GameServerEngine, used for client disconnect
             * notifications and obtaining client lists for broadcast responses.
             */
            UDPPacketHandler(INetworkIO* networkIO,
                IMessageHandler* messageHandler,
                RiftForged::Server::GameServerEngine& gameServerEngine);

            // Virtual destructor to ensure proper cleanup if inherited from.
            ~UDPPacketHandler() override;

            // Disable copy and assignment to prevent accidental copying of state
            UDPPacketHandler(const UDPPacketHandler&) = delete;
            UDPPacketHandler& operator=(const UDPPacketHandler&) = delete;

            /**
             * @brief Starts the PacketHandler's operations, notably the reliability management thread.
             * @return True if successfully started, false otherwise.
             */
            bool Start();

            /**
             * @brief Stops the PacketHandler's operations, primarily by signalling and joining
             * the reliability management thread.
             */
            void Stop();

            // --- INetworkIOEvents Implementation ---
            // These methods are called by the INetworkIO layer (UDPSocketAsync).

            /**
             * @brief Called by UDPSocketAsync when a raw datagram is received.
             * This is the main entry point for incoming packets. This method will
             * parse the GamePacketHeader, run reliability checks, and if an application
             * payload is present and valid, pass it to the IMessageHandler.
             */
            void OnRawDataReceived(const NetworkEndpoint& sender,
                const uint8_t* data,
                uint32_t size,
                OverlappedIOContext* context) override;

            /**
             * @brief Called by UDPSocketAsync when an asynchronous send operation completes.
             */
            void OnSendCompleted(OverlappedIOContext* context,
                bool success,
                uint32_t bytesSent) override;

            /**
             * @brief Called by UDPSocketAsync when a non-operation-specific network error occurs.
             */
            void OnNetworkError(const std::string& errorMessage, int errorCode = 0) override;

            void SetNetworkIO(INetworkIO* networkIO) {
                m_networkIO = networkIO;
            }

            // --- Public Sending Interface ---
            // These methods are called by higher layers (e.g., MessageHandler responses, game systems)
            // to send data to clients.

            /**
             * @brief Sends a packet reliably to a specific recipient.
             * Handles adding reliability headers and queuing for potential retransmission.
             * @param recipient The target client endpoint.
             * @param flatbufferPayloadType The FlatBuffer payload's type (e.g., S2C_UDP_Payload_EntityStateUpdate).
             * @param flatbufferPayload The serialized application payload (FlatBuffer bytes).
             * @param additionalFlags Any extra flags for the GamePacketHeader (e.g., IS_HEARTBEAT).
             * @return True if the packet was successfully prepared and queued for sending, false otherwise.
             */
            bool SendReliablePacket(const NetworkEndpoint& recipient,
                UDP::S2C::S2C_UDP_Payload flatbufferPayloadType, // <<< CHANGED TYPE
                const flatbuffers::DetachedBuffer& flatbufferPayload, // <<< CHANGED TYPE
                uint8_t additionalFlags = 0);

            /**
             * @brief Sends a packet unreliably to a specific recipient.
             * Adds basic packet headers but does not queue for retransmission.
             * @param recipient The target client endpoint.
             * @param flatbufferPayloadType The FlatBuffer payload's type (e.g., S2C_UDP_Payload_Pong).
             * @param flatbufferPayload The serialized application payload (FlatBuffer bytes).
             * @param additionalFlags Any extra flags for the GamePacketHeader.
             * @return True if the packet was successfully prepared and sent, false otherwise.
             */
            bool SendUnreliablePacket(const NetworkEndpoint& recipient,
                UDP::S2C::S2C_UDP_Payload flatbufferPayloadType, // <<< CHANGED TYPE
                const flatbuffers::DetachedBuffer& flatbufferPayload, // <<< CHANGED TYPE
                uint8_t additionalFlags = 0);

            /**
             * @brief Sends an ACK-only packet, typically triggered by the reliability protocol.
             * @param recipient The endpoint to send the ACK to.
             * @param connectionState The reliability state for the connection, containing ACK info.
             * @return True if the ACK packet was sent, false otherwise.
             */
            bool SendAckPacket(const NetworkEndpoint& recipient, ReliableConnectionState& connectionState);

        private:
            // --- Internal Reliability Protocol Methods ---

            void ReliabilityManagementThread(); // Manages retransmissions, timeouts, sending pending ACKs.

            // Gets or creates a reliability state for a given client endpoint.
            std::shared_ptr<ReliableConnectionState> GetOrCreateReliabilityState(const NetworkEndpoint& endpoint);

            INetworkIO* m_networkIO = nullptr; // Member to store the network IO instance  

            /**
             * @brief Helper to handle responses returned by IMessageHandler.
             * This function will decide whether to send a reliable or unreliable packet
             * based on the S2C_Response's details.
             */
            void HandleResponseMessage(const std::optional<S2C_Response>& responseOpt);


            // --- Member Variables ---
            //INetworkIO* m_networkIO;           // Pointer to the underlying network IO layer (UDPSocketAsync)
            IMessageHandler* m_messageHandler; // Pointer to the application message processor
            RiftForged::Server::GameServerEngine& m_gameServerEngine; // Reference to the GameServerEngine for game logic interactions
            std::atomic<bool> m_isRunning;     // Controls the reliability thread loop

            // Reliability-specific state
            std::map<NetworkEndpoint, std::shared_ptr<ReliableConnectionState>> m_reliabilityStates;
            std::mutex m_reliabilityStatesMutex; // Protects m_reliabilityStates and m_endpointLastSeenTime
            std::thread m_reliabilityThread;     // Thread dedicated to reliability tasks
            std::map<NetworkEndpoint, std::chrono::steady_clock::time_point> m_endpointLastSeenTime; // Tracks last communication
        };

    } // namespace Networking
} // namespace RiftForged