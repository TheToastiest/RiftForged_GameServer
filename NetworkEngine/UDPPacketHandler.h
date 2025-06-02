// File: UDPPacketHandler.h
// RiftForged Game Development
// Purpose: Handles UDP packet-level logic, including custom reliability,
//          and acts as a bridge between the raw Network IO layer and
//          the application-level MessageHandler.

#pragma once

#include "INetworkIOEvents.h"     // Implements this interface to receive events from UDPSocketAsync
#include "NetworkEndpoint.h"      // For representing remote client addresses
#include "GamePacketHeader.h"     // Defines GamePacketHeader structure and MessageType enum
#include "UDPReliabilityProtocol.h"// Defines ReliableConnectionState and associated reliability logic/types
#include "NetworkCommon.h"        // For common network types like S2C_Response (if PacketHandler forms responses)
//#include "../Gameplay/PlayerManager.h" // Required for notifying game logic about disconnections
                                      // and potentially for getting lists of clients for broadcasts.

#include <string>
#include <vector>
#include <map>
#include <memory>   // For std::shared_ptr
#include <mutex>    // For std::mutex
#include <thread>   // For std::thread (reliability thread)
#include <atomic>   // For std::atomic_bool
#include <optional> // For std::optional (handling responses from MessageHandler)
#include <chrono>   // For std::chrono::steady_clock

// Forward declarations for interfaces this class will use
namespace RiftForged {
    namespace Networking {
        class INetworkIO;           // Interface to the underlying network transport (e.g., UDPSocketAsync)
        class IMessageHandler;      // Interface to the application message processor
        struct OverlappedIOContext; // Defined in OverlappedIOContext.h, passed by INetworkIOEvents
    }
    // PlayerManager is directly included above
    namespace Server { // <<< ADDED for GameServerEngine
        class GameServerEngine;
    }
}

// Constants for the reliability protocol managed by this PacketHandler.
// Suffix _PKT helps distinguish if similar constants exist elsewhere.
const int RELIABILITY_THREAD_SLEEP_MS_PKT = 20; // How often the reliability thread wakes up.
const float DEFAULT_RTO_MS_PKT = 200.0f;      // Default Retransmission Timeout in milliseconds.
const int DEFAULT_MAX_RETRIES_PKT = 5;        // Max times a reliable packet is resent before giving up.
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
             * @param playerManager A reference to the PlayerManager, used for client disconnect
             * notifications by the reliability layer and potentially for
             * obtaining client lists for broadcast responses.
             */
            UDPPacketHandler(INetworkIO* networkIO,
                IMessageHandler* messageHandler,
                RiftForged::Server::GameServerEngine& gameServerEngine); // <<< MODIFIED

            // Virtual destructor to ensure proper cleanup if inherited from.
            ~UDPPacketHandler() override;

            // Disable copy and assignment to prevent accidental copying of state
            // like threads, mutexes, and connection maps.
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
             * Can be used for logging or advanced send management if necessary.
             * The OverlappedIOContext for sends is typically deleted by UDPSocketAsync after this call.
             */
            void OnSendCompleted(OverlappedIOContext* context,
                bool success,
                uint32_t bytesSent) override;

            /**
             * @brief Called by UDPSocketAsync when a non-operation-specific network error occurs.
             */
            void OnNetworkError(const std::string& errorMessage, int errorCode = 0) override;


            // --- Public Sending Interface ---
            // These methods are called by higher layers (e.g., MessageHandler responses, game systems)
            // to send data to clients.

            /**
             * @brief Sends a packet reliably to a specific recipient.
             * Handles adding reliability headers and queuing for potential retransmission.
             * @param recipient The target client endpoint.
             * @param messageId The MessageType of the application payload.
             * @param flatbufferPayload The application payload (FlatBuffer bytes).
             * @param additionalFlags Any extra flags for the GamePacketHeader (e.g., IS_HEARTBEAT).
             * @return True if the packet was successfully prepared and queued for sending, false otherwise.
             */
            bool SendReliablePacket(const NetworkEndpoint& recipient,
                MessageType messageId,
                const std::vector<uint8_t>& flatbufferPayload,
                uint8_t additionalFlags = 0);

            /**
             * @brief Sends a packet unreliably to a specific recipient.
             * Adds basic packet headers but does not queue for retransmission.
             * @param recipient The target client endpoint.
             * @param messageId The MessageType of the application payload.
             * @param flatbufferPayload The application payload (FlatBuffer bytes).
             * @param additionalFlags Any extra flags for the GamePacketHeader.
             * @return True if the packet was successfully prepared and sent, false otherwise.
             */
            bool SendUnreliablePacket(const NetworkEndpoint& recipient,
                MessageType messageId,
                const std::vector<uint8_t>& flatbufferPayload,
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
            // These methods encapsulate the logic moved from UDPSocketAsync and/or your
            // UDPReliabilityProtocol.h/cpp.

            void ReliabilityManagementThread(); // Manages retransmissions, timeouts, sending pending ACKs.

            // Gets or creates a reliability state for a given client endpoint.
            std::shared_ptr<ReliableConnectionState> GetOrCreateReliabilityState(const NetworkEndpoint& endpoint);

            /**
             * @brief Processes the reliability aspects of an incoming packet's header.
             * Updates ACKs, sequence numbers, detects duplicates, etc.
             * @param connectionState The reliability state for the sender.
             * @param header The received GamePacketHeader.
             * @param payloadAfterGameHeader Pointer to the data *after* the GamePacketHeader.
             * @param payloadAfterGameHeaderSize Size of the data *after* the GamePacketHeader.
             * @param outAppPayload Output: If the packet is valid and contains an application payload,
             * this will point to the start of that payload (FlatBuffer data).
             * @param outAppPayloadSize Output: The size of the application payload.
             * @return True if the application payload (if any) should be processed further by MessageHandler.
             * False if it's a duplicate, pure ACK, or invalid for application processing.
             */
            bool ProcessIncomingReliabilityHeader(ReliableConnectionState& connectionState,
                const GamePacketHeader& header,
                const uint8_t* payloadAfterGameHeader,
                uint16_t payloadAfterGameHeaderSize,
                const uint8_t** outAppPayload,
                uint16_t* outAppPayloadSize);

            /**
             * @brief Constructs the full network packet (GamePacketHeader + application payload)
             * including reliability information.
             * @param connectionState The reliability state for the recipient.
             * @param messageId The MessageType of the application payload.
             * @param flatbufferPayloadData Pointer to the application payload (FlatBuffer bytes).
             * @param flatbufferPayloadSize Size of the application payload.
             * @param flags Flags for the GamePacketHeader (e.g., IS_RELIABLE, IS_ACK_ONLY).
             * @return A byte vector containing the fully constructed packet ready for sending.
             * Returns an empty vector on error.
             */
            std::vector<uint8_t> PrepareOutgoingPacketBuffer(ReliableConnectionState& connectionState,
                MessageType messageId,
                const uint8_t* flatbufferPayloadData,
                uint16_t flatbufferPayloadSize,
                uint8_t flags);

            // Retrieves packets that need to be retransmitted for a given connection state.
            std::vector<std::vector<uint8_t>> GetPacketsForRetransmission(
                ReliableConnectionState& state,
                std::chrono::steady_clock::time_point currentTime,
                int maxRetries);

            // Helper to handle responses returned by IMessageHandler
            void HandleResponseMessage(const std::optional<S2C_Response>& responseOpt, const NetworkEndpoint& originalSender);


            // --- Member Variables ---
            INetworkIO* m_networkIO;          // Pointer to the underlying network IO layer (UDPSocketAsync)
            IMessageHandler* m_messageHandler;  // Pointer to the application message processor
			RiftForged::Server::GameServerEngine& m_gameServerEngine; // Reference to the GameServerEngine for game logic interactions
            std::atomic<bool> m_isRunning;      // Controls the reliability thread loop

            // Reliability-specific state, moved from UDPSocketAsync
            std::map<NetworkEndpoint, std::shared_ptr<ReliableConnectionState>> m_reliabilityStates;
            std::mutex m_reliabilityStatesMutex; // Protects m_reliabilityStates and m_endpointLastSeenTime
            std::thread m_reliabilityThread;     // Thread dedicated to reliability tasks
            std::map<NetworkEndpoint, std::chrono::steady_clock::time_point> m_endpointLastSeenTime; // Tracks last communication
        };

    } // namespace Networking
} // namespace RiftForged