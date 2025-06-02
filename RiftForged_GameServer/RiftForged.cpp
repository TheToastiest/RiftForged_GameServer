// File: GameServer/Main.cpp (Refactored Instantiation)

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

// Core Engine Components
#include "../GameServer/GameServerEngine.h" // Forward declares UDPPacketHandler

// Gameplay Components
#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/GameplayEngine.h"

// Physics Engine
#include "../PhysicsEngine/PhysicsEngine.h"

// Networking Components
#include "../NetworkEngine/INetworkIO.h"
#include "../NetworkEngine/UDPSocketAsync.h"
#include "../NetworkEngine/INetworkIOEvents.h" // UDPPacketHandler implements this
#include "../NetworkEngine/UDPPacketHandler.h" // Full definition needed before GameServerEngine can set it
#include "../NetworkEngine/IMessageHandler.h"   // PacketProcessor implements this
#include "../NetworkEngine/PacketProcessor.h"
#include "../NetworkEngine/MessageDispatcher.h"
// Specific C2S Message Handlers
#include "../NetworkEngine/MovementMessageHandler.h"
#include "../NetworkEngine/RiftStepMessageHandler.h"
#include "../NetworkEngine/AbilityMessageHandler.h"
#include "../NetworkEngine/PingMessageHandler.h"
#include "../NetworkEngine/TurnMessageHandler.h"
#include "../NetworkEngine/BasicAttackMessageHandler.h"

#include "../Utils/Logger.h"

std::atomic<bool> g_isServerRunning = true;

int main() {
    std::cout << "RiftForged GameServer Starting (Refactored Network Stack & Main)..." << std::endl;
    RiftForged::Utilities::Logger::Init();
    RF_CORE_INFO("Logger Initialized.");

    // --- 1. Instantiate Core Managers and Game Engines (Lowest Level Dependencies) ---
    RF_CORE_INFO("Instantiating core managers and game engines...");
    RiftForged::GameLogic::PlayerManager playerManager;
    RF_CORE_INFO("PlayerManager created.");

    RiftForged::Physics::PhysicsEngine physicsEngine;
    if (!physicsEngine.Initialize()) { /* ... error handling ... */ return -1; }
    RF_CORE_INFO("PhysicsEngine initialized.");
    // Ground plane creation code (commented out by user) can go here if desired

    RiftForged::Gameplay::GameplayEngine gameplayEngine(playerManager, physicsEngine);
    RF_CORE_INFO("GameplayEngine created.");

    // --- 2. Instantiate Specific C2S Message Handlers ---
    // These now ideally take GameServerEngine& if they need to SubmitPlayerCommand after parsing
    // For now, assuming they are self-contained or MessageDispatcher handles GSE interaction.
    // If they need GameServerEngine, it must be created before them, or they also use setters/pointers.
    // Let's defer this change for handlers for now and focus on the main components.
    // If they need PlayerManager or GameplayEngine, GameplayEngine can provide PlayerManager via GetPlayerManager().
    RF_CORE_INFO("Instantiating specific C2S message handlers...");
	RiftForged::Networking::UDP::C2S::MovementMessageHandler movementHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::RiftStepMessageHandler riftStepHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::AbilityMessageHandler abilityHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::PingMessageHandler pingHandler(playerManager); // Ping might just need PM or even GSE
    RiftForged::Networking::UDP::C2S::TurnMessageHandler turnHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::BasicAttackMessageHandler basicAttackHandler(playerManager, gameplayEngine);
    RF_CORE_INFO("Specific C2S message handlers created.");

    // --- 3. Instantiate MessageDispatcher ---
    RiftForged::Networking::MessageDispatcher messageDispatcher(
        movementHandler, riftStepHandler, abilityHandler, pingHandler, turnHandler, basicAttackHandler
        // If MessageDispatcher directly calls GameServerEngine::SubmitPlayerCommand, it needs GameServerEngine&
    );
    RF_CORE_INFO("MessageDispatcher created.");

    // --- 4. Instantiate GameServerEngine (without UDPPacketHandler initially) ---
    RiftForged::Server::GameServerEngine gameServerEngine(
        playerManager,
        gameplayEngine,
        // packetHandler, // This will be set later
        physicsEngine
    );
    RF_CORE_INFO("GameServerEngine object created (PacketHandler not set yet).");

    // --- 5. Instantiate PacketProcessor (IMessageHandler) ---
    // PacketProcessor now needs GameServerEngine for endpoint/playerid logic.
    RiftForged::Networking::PacketProcessor packetProcessor(messageDispatcher, gameServerEngine); // Pass actual gameServerEngine
    RF_CORE_INFO("PacketProcessor (IMessageHandler) created.");

    // --- 6. Instantiate Network IO Layer (UDPSocketAsync) ---
    RiftForged::Networking::UDPSocketAsync udpSocket;
    RF_CORE_INFO("UDPSocketAsync (INetworkIO) object created.");

    // --- 7. Instantiate Packet Handler Layer (UDPPacketHandler) ---
    // UDPPacketHandler needs GameServerEngine for disconnect notifications and broadcasts.
    RiftForged::Networking::UDPPacketHandler packetHandler(&udpSocket, &packetProcessor, gameServerEngine); // Pass gameServerEngine
    RF_CORE_INFO("UDPPacketHandler (INetworkIOEvents & Packet Logic) created.");

    // --- 8. Complete GameServerEngine Setup by Setting PacketHandler ---
    gameServerEngine.SetPacketHandler(&packetHandler); // Now GameServerEngine can send packets

    // --- 9. Initialize and Start Network Layers ---
    RF_CORE_INFO("Initializing network layers...");
    if (!udpSocket.Init("0.0.0.0", 12345, &packetHandler)) { /* ... error handling ... */ return -1; }
    RF_CORE_INFO("UDPSocketAsync (NetworkIO) initialized.");

    if (!packetHandler.Start()) { /* ... error handling ... */ return -1; }
    RF_CORE_INFO("UDPPacketHandler started.");

    if (!udpSocket.Start()) { /* ... error handling ... */ return -1; }
    RF_CORE_INFO("UDPSocketAsync (NetworkIO) started.");

    // --- 10. Start the GameServerEngine's Simulation Loop ---
    // Ensure PacketHandler is set on GameServerEngine BEFORE starting its loop if it sends anything early.
    gameServerEngine.StartSimulationLoop();
    RF_CORE_INFO("MAIN: GameServerEngine simulation loop started. Server is running.");
    std::cout << "Type 'q' or 'quit' and press Enter to stop the server." << std::endl;

    // --- 11. Main application loop ---
    // ... (same as your provided code) ...
    std::string input_line;
    while (g_isServerRunning.load()) {
        if (std::cin.rdbuf()->in_avail() > 0) { // Non-blocking check for input
            if (!std::getline(std::cin, input_line)) {
                RF_CORE_ERROR("MAIN: std::cin error or EOF. Initiating shutdown.");
                g_isServerRunning.store(false);
                break;
            }
            if (input_line == "q" || input_line == "quit") {
                RF_CORE_INFO("MAIN: Shutdown command received via console.");
                g_isServerRunning.store(false);
                break;
            }
            else if (!input_line.empty()) {
                RF_CORE_INFO("MAIN: Unknown command '{}'. Type 'q' or 'quit' to exit.", input_line);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    // --- Shutdown Sequence ---
    RF_CORE_INFO("MAIN: Shutdown sequence initiated...");
    // Stop GameServerEngine first, which should ideally signal game logic to wrap up.
    // It also handles physics shutdown internally if it's responsible.
    RF_CORE_INFO("MAIN: Signaling GameServerEngine to stop...");
    gameServerEngine.StopSimulationLoop();

    RF_CORE_INFO("MAIN: Signaling UDPPacketHandler to stop...");
    packetHandler.Stop();

    RF_CORE_INFO("MAIN: Signaling UDPSocketAsync (NetworkIO) to stop...");
    udpSocket.Stop();

    // If PhysicsEngine is not shutdown by GameServerEngine, do it here.
    // physicsEngine.Shutdown(); // Assuming GameServerEngine::StopSimulationLoop calls this now.

    RF_CORE_INFO("MAIN: Flushing and shutting down logger...");
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();
    std::cout << "MAIN: Server shut down gracefully." << std::endl;
    return 0;
}