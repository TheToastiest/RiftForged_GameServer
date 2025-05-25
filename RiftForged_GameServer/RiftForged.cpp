// RiftForged_GameServer/GameServer/Main.cpp

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono> // Required for std::chrono::milliseconds

// Core Engine Components
#include "../GameServer/GameServerEngine.h" 

// Gameplay Components
#include "../Gameplay/PlayerManager.h"    // Ensure this path is correct
#include "../Gameplay/GameplayEngine.h"   // Ensure this path is correct

// Physics Engine
#include "../PhysicsEngine/PhysicsEngine.h" // Added for PhysicsEngine

// Networking Components
#include "../NetworkEngine/UDPSocketAsync.h"
#include "../NetworkEngine/PacketProcessor.h"
#include "../NetworkEngine/MessageDispatcher.h"
#include "../NetworkEngine/MovementMessageHandler.h"
#include "../NetworkEngine/RiftStepMessageHandler.h"
#include "../NetworkEngine/AbilityMessageHandler.h"
#include "../NetworkEngine/PingMessageHandler.h"
#include "../NetworkEngine/TurnMessageHandler.h"
#include "../NetworkEngine/BasicAttackMessageHandler.h"

// Utilities
#include "../Utils/Logger.h"

// Global atomic flag (Consider if GameServerEngine should manage its own running state)
std::atomic<bool> g_isServerRunning = true;

int main() {
    std::cout << "RiftForged GameServer Starting..." << std::endl;
    RiftForged::Utilities::Logger::Init();
    RF_CORE_INFO("Logger Initialized.");

    // --- 1. Instantiate Core Managers and Engines ---
    RF_CORE_INFO("Instantiating core managers and engines...");
    RiftForged::GameLogic::PlayerManager playerManager;
    RF_CORE_INFO("PlayerManager created.");

    RiftForged::Physics::PhysicsEngine physicsEngine; // Create PhysicsEngine
    if (!physicsEngine.Initialize()) {                // Initialize PhysicsEngine
        RF_CORE_CRITICAL("MAIN: Failed to initialize PhysicsEngine. Exiting.");
        RiftForged::Utilities::Logger::Shutdown();
        return -1;
    }
    RF_CORE_INFO("PhysicsEngine initialized.");

    // GameplayEngine requires PlayerManager and PhysicsEngine
    // This assumes your GameplayEngine.h and .cpp are updated for this constructor
    RiftForged::Gameplay::GameplayEngine gameplayEngine(playerManager, physicsEngine);
    RF_CORE_INFO("GameplayEngine created.");

    // --- 2. Instantiate Message Handlers ---
    RF_CORE_INFO("Instantiating message handlers...");
    // Ensure constructors of these handlers match what they are passed
    RiftForged::Networking::UDP::C2S::MovementMessageHandler movementHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::RiftStepMessageHandler riftStepHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::AbilityMessageHandler abilityHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::PingMessageHandler pingHandler(playerManager);
    RiftForged::Networking::UDP::C2S::TurnMessageHandler turnHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::BasicAttackMessageHandler basicAttackHandler(playerManager, gameplayEngine);
    RF_CORE_INFO("Message handlers created.");

    // --- 3. Instantiate MessageDispatcher ---
    RiftForged::Networking::MessageDispatcher messageDispatcher(
        movementHandler, riftStepHandler, abilityHandler, pingHandler, turnHandler, basicAttackHandler);
    RF_CORE_INFO("MessageDispatcher created.");

    // --- 4. Instantiate PacketProcessor ---
    RiftForged::Networking::PacketProcessor packetProcessor(messageDispatcher, gameplayEngine);
    RF_CORE_INFO("PacketProcessor created.");

    // --- 5. Instantiate UDPSocketAsync ---
    RiftForged::Networking::UDPSocketAsync udpSocket(playerManager, packetProcessor, "0.0.0.0", 12345);
    RF_CORE_INFO("UDPSocketAsync object created.");

    // --- 6. Instantiate GameServerEngine ---
    // This now matches the constructor in your current GameServerEngine.h
    RiftForged::Server::GameServerEngine gameServerEngine(
        playerManager,
        gameplayEngine, // Pass the created GameplayEngine
        udpSocket,
        physicsEngine   // Pass the created PhysicsEngine
        // Uses the default tickInterval from GameServerEngine.h (8ms)
    );
    RF_CORE_INFO("GameServerEngine object created.");

    // --- 7. Initialize Network Layer ---
    if (!udpSocket.Init()) {
        RF_CORE_CRITICAL("MAIN: Failed to initialize UDPSocketAsync. Exiting.");
        physicsEngine.Shutdown();
        RiftForged::Utilities::Logger::Shutdown();
        return -1;
    }
    RF_CORE_INFO("MAIN: UDPSocketAsync initialized.");

    // --- 8. Start Network Layer ---
    if (!udpSocket.Start()) {
        RF_CORE_CRITICAL("MAIN: Failed to start UDPSocketAsync. Exiting.");
        udpSocket.Stop();
        physicsEngine.Shutdown();
        RiftForged::Utilities::Logger::Shutdown();
        return -1;
    }
    RF_CORE_INFO("MAIN: UDPSocketAsync started.");

    // --- 9. Start the GameServerEngine's Simulation Loop ---
    gameServerEngine.StartSimulationLoop();
    RF_CORE_INFO("MAIN: GameServerEngine simulation loop started. Server is running.");
    std::cout << "Type 'q' or 'quit' and press Enter to stop the server." << std::endl;

    // --- 10. Main application loop ---
    std::string input_line;
    while (g_isServerRunning.load()) {
        if (std::cin.rdbuf()->in_avail() > 0) {
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
            else if (input_line.rfind("broadcast ", 0) == 0) {
                if (input_line.length() > 10) {
                    std::string message_to_broadcast = input_line.substr(10);
                    RF_CORE_INFO("MAIN: Broadcast requested via console: {}", message_to_broadcast);
                    // TODO: gameServerEngine.QueueSystemBroadcast(message_to_broadcast);
                }
            }
            else if (!input_line.empty()) {
                RF_CORE_INFO("MAIN: Unknown command '{}'. Type 'q' or 'quit' to exit.", input_line);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // --- Shutdown Sequence ---
    RF_CORE_INFO("MAIN: Shutdown sequence initiated...");

    RF_CORE_INFO("MAIN: Signaling GameServerEngine to stop...");
    gameServerEngine.StopSimulationLoop(); // This should also call m_physicsEngine.Shutdown() internally

    RF_CORE_INFO("MAIN: Signaling UDPSocketAsync to stop...");
    udpSocket.Stop();

    // physicsEngine.Shutdown(); // Should be handled by GameServerEngine::StopSimulationLoop 
                               // as GSE now holds a reference to it and your GSE.cpp calls Shutdown.

    RF_CORE_INFO("MAIN: Flushing and shutting down logger...");
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();
    std::cout << "MAIN: Server shut down gracefully." << std::endl;
    return 0;
}