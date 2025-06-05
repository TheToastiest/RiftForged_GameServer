// File: GameServer/Main.cpp (Refactored Instantiation and Full Flow)

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory> // For std::unique_ptr
#include <thread> // For std::this_thread::sleep_for

// Core Engine Components
#include "../GameServer/GameServerEngine.h"

// Gameplay Components
#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/GameplayEngine.h"

// Physics Engine
#include "../PhysicsEngine/PhysicsEngine.h"

// Networking Components
#include "../NetworkEngine/INetworkIO.h"
#include "../NetworkEngine/UDPSocketAsync.h"
#include "../NetworkEngine/INetworkIOEvents.h"
#include "../NetworkEngine/UDPPacketHandler.h"
#include "../NetworkEngine/IMessageHandler.h"
#include "../NetworkEngine/PacketProcessor.h"
#include "../NetworkEngine/MessageDispatcher.h"

// Specific C2S Message Handlers
#include "../NetworkEngine/MovementMessageHandler.h"
#include "../NetworkEngine/RiftStepMessageHandler.h"
#include "../NetworkEngine/AbilityMessageHandler.h"
#include "../NetworkEngine/PingMessageHandler.h"
#include "../NetworkEngine/TurnMessageHandler.h"
#include "../NetworkEngine/BasicAttackMessageHandler.h"
#include "../NetworkEngine/JoinRequestMessageHandler.h"

#include "../Utils/Logger.h"

std::atomic<bool> g_isServerRunning = true;

int main() {
    std::cout << "RiftForged GameServer Starting (Refactored Network Stack & Main)..." << std::endl;

    RiftForged::Utilities::Logger::Init();
    RF_CORE_INFO("Logger Initialized.");

    const unsigned short SERVER_PORT = 12345;
    const std::string LISTEN_IP_ADDRESS = "0.0.0.0";
    const size_t GAME_LOGIC_THREAD_POOL_SIZE = 12; // Or std::thread::hardware_concurrency() if appropriate
    const std::chrono::milliseconds GAME_TICK_INTERVAL_MS(5); // Approx 200 TPS

    // Declare unique_ptrs for RAII
    std::unique_ptr<RiftForged::Networking::UDPSocketAsync> udpSocket;
    std::unique_ptr<RiftForged::Networking::UDPPacketHandler> packetHandler;
    std::unique_ptr<RiftForged::Networking::PacketProcessor> packetProcessor;
    std::unique_ptr<RiftForged::Networking::MessageDispatcher> messageDispatcher;

    std::unique_ptr<RiftForged::Networking::UDP::C2S::MovementMessageHandler> movementHandler;
    std::unique_ptr<RiftForged::Networking::UDP::C2S::RiftStepMessageHandler> riftStepHandler;
    std::unique_ptr<RiftForged::Networking::UDP::C2S::AbilityMessageHandler> abilityHandler;
    std::unique_ptr<RiftForged::Networking::UDP::C2S::PingMessageHandler> pingHandler;
    std::unique_ptr<RiftForged::Networking::UDP::C2S::TurnMessageHandler> turnHandler;
    std::unique_ptr<RiftForged::Networking::UDP::C2S::BasicAttackMessageHandler> basicAttackHandler;
    std::unique_ptr<RiftForged::Networking::UDP::C2S::JoinRequestMessageHandler> joinRequestHandler;

    // Core Gameplay and Server Logic Objects (Stack allocated or managed by GameServerEngine if preferred)
    RiftForged::GameLogic::PlayerManager playerManager;
    RiftForged::Physics::PhysicsEngine physicsEngine; // Must be initialized before GameplayEngine if it's a dependency
    RiftForged::Gameplay::GameplayEngine gameplayEngine(playerManager, physicsEngine);

    RiftForged::Server::GameServerEngine gameServerEngine(
        playerManager,
        gameplayEngine,
        physicsEngine,
        GAME_LOGIC_THREAD_POOL_SIZE,
        GAME_TICK_INTERVAL_MS
    );

    try {
        RF_CORE_INFO("Initializing core systems and wiring dependencies...");

        if (!physicsEngine.Initialize()) {
            RF_CORE_CRITICAL("Server: PhysicsEngine initialization failed. Exiting.");
            return 1;
        }
        RF_CORE_INFO("PhysicsEngine initialized.");

        // --- Instantiate Specific C2S Message Handlers ---
        RF_CORE_INFO("Instantiating specific C2S message handlers...");
        movementHandler = std::make_unique<RiftForged::Networking::UDP::C2S::MovementMessageHandler>(
            gameServerEngine.GetPlayerManager(), gameplayEngine, &gameServerEngine.GetGameLogicThreadPool());
        riftStepHandler = std::make_unique<RiftForged::Networking::UDP::C2S::RiftStepMessageHandler>(
            gameServerEngine.GetPlayerManager(), gameplayEngine, &gameServerEngine.GetGameLogicThreadPool());
        abilityHandler = std::make_unique<RiftForged::Networking::UDP::C2S::AbilityMessageHandler>(
            gameServerEngine.GetPlayerManager(), gameplayEngine, &gameServerEngine.GetGameLogicThreadPool());
        pingHandler = std::make_unique<RiftForged::Networking::UDP::C2S::PingMessageHandler>( // Corrected based on previous error
            gameServerEngine.GetPlayerManager(),
            &gameServerEngine.GetGameLogicThreadPool()
        );
        turnHandler = std::make_unique<RiftForged::Networking::UDP::C2S::TurnMessageHandler>(
            gameServerEngine.GetPlayerManager(), gameplayEngine, &gameServerEngine.GetGameLogicThreadPool());
        basicAttackHandler = std::make_unique<RiftForged::Networking::UDP::C2S::BasicAttackMessageHandler>(
            gameServerEngine.GetPlayerManager(), gameplayEngine, &gameServerEngine.GetGameLogicThreadPool());
        joinRequestHandler = std::make_unique<RiftForged::Networking::UDP::C2S::JoinRequestMessageHandler>(gameServerEngine);
        RF_CORE_INFO("Specific C2S message handlers created.");

        // --- Instantiate MessageDispatcher ---
        messageDispatcher = std::make_unique<RiftForged::Networking::MessageDispatcher>(
            *movementHandler, *riftStepHandler, *abilityHandler, *pingHandler,
            *turnHandler, *basicAttackHandler, *joinRequestHandler,
            &gameServerEngine.GetGameLogicThreadPool()
        );
        RF_CORE_INFO("MessageDispatcher created.");

        // --- Instantiate PacketProcessor (implements IMessageHandler) ---
        packetProcessor = std::make_unique<RiftForged::Networking::PacketProcessor>(
            *messageDispatcher,
            gameServerEngine
        );
        RF_CORE_INFO("PacketProcessor (IMessageHandler) created.");

        // *******************************************************************
        // ** CRITICAL REORDERING FOR INetworkIO DEPENDENCY **
        // *******************************************************************

        // 1. Instantiate UDPSocketAsync (implements INetworkIO) FIRST
        udpSocket = std::make_unique<RiftForged::Networking::UDPSocketAsync>();
        RF_CORE_INFO("UDPSocketAsync (INetworkIO) created.");
        // Now udpSocket.get() will return a valid pointer.

        // 2. Instantiate UDPPacketHandler, providing the valid INetworkIO pointer
        packetHandler = std::make_unique<RiftForged::Networking::UDPPacketHandler>(
            udpSocket.get(), // Provide the actual INetworkIO instance
            packetProcessor.get(), // This is the IMessageHandler
            gameServerEngine
        );
        RF_CORE_INFO("UDPPacketHandler (INetworkIOEvents & Packet Logic) created with INetworkIO dependency.");
        // No need for packetHandler->SetNetworkIO(...) later if constructor injection is used.

        // *******************************************************************

        // --- Wire GameServerEngine with UDPPacketHandler for Outgoing Messages ---
        gameServerEngine.SetPacketHandler(packetHandler.get());
        RF_CORE_INFO("GameServerEngine wired with UDPPacketHandler.");

        // --- Initialize and Start Core Engine/Network Systems ---
        RF_CORE_INFO("Starting network layers...");

        // Initialize UDPSocketAsync, passing the packetHandler as the INetworkIOEvents listener.
        // This is where UDPSocketAsync gets the callback target for OnRawDataReceived etc.
        if (!udpSocket->Init(LISTEN_IP_ADDRESS, SERVER_PORT, packetHandler.get())) {
            RF_CORE_CRITICAL("Server: Failed to initialize UDP socket on %s:%hu. Exiting.",
                LISTEN_IP_ADDRESS.c_str(), SERVER_PORT);
            return 1;
        }
        RF_CORE_INFO("UDP Socket initialized.");

        // Start network listener threads and post initial receives.
        if (!udpSocket->Start()) {
            RF_CORE_CRITICAL("Server: Failed to start UDP socket listener. Exiting.");
            return 1;
        }
        RF_CORE_INFO("UDP Socket listener started.");

        // Start UDPPacketHandler's internal threads (e.g., reliability management).
        if (!packetHandler->Start()) {
            RF_CORE_CRITICAL("Server: Failed to start UDP Packet Handler. Exiting.");
            if (udpSocket) udpSocket->Stop(); // Clean up socket if handler fails to start
            return 1;
        }
        RF_CORE_INFO("UDPPacketHandler started.");

        // Initialize GameServerEngine (if it requires an explicit Init call)
        if (!gameServerEngine.Initialize()) {
            RF_CORE_CRITICAL("Server: GameServerEngine initialization failed. Exiting.");
            if (packetHandler) packetHandler->Stop();
            if (udpSocket) udpSocket->Stop();
            return 1;
        }
        RF_CORE_INFO("GameServerEngine initialized.");

        // --- Start the GameServerEngine's Simulation Loop ---
        gameServerEngine.StartSimulationLoop();
        RF_CORE_INFO("MAIN: GameServerEngine simulation loop started. Server is running.");
        std::cout << "Type 'q' or 'quit' and press Enter to stop the server." << std::endl;

        // --- Main application loop (blocking for user input to keep server alive) ---
        std::string input_line;
        while (g_isServerRunning.load(std::memory_order_relaxed)) { // Can use relaxed for this flag
            if (std::cin.rdbuf()->in_avail() > 0) {
                if (!std::getline(std::cin, input_line)) {
                    RF_CORE_ERROR("MAIN: std::cin error or EOF. Initiating shutdown.");
                    g_isServerRunning.store(false, std::memory_order_release);
                    break;
                }
                if (input_line == "q" || input_line == "quit") {
                    RF_CORE_INFO("MAIN: Shutdown command received via console.");
                    g_isServerRunning.store(false, std::memory_order_release);
                    break;
                }
                else if (!input_line.empty()) {
                    RF_CORE_INFO("MAIN: Unknown command '{}'. Type 'q' or 'quit' to exit.", input_line);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    }
    catch (const std::exception& e) {
        RF_CORE_CRITICAL("Server: Unhandled standard exception during startup or main loop: {}", e.what()); // Use {} for logger
        // Ensure main components that might have threads are stopped if an exception occurs mid-startup
        // Note: RAII from unique_ptr will handle destruction, but explicit stop might be cleaner for threads.
        if (gameServerEngine.isSimulating()) gameServerEngine.StopSimulationLoop();
        if (packetHandler) packetHandler->Stop();
        if (udpSocket) udpSocket->Stop();
        gameServerEngine.Shutdown(); // Ensure its thread pool is stopped.
        RiftForged::Utilities::Logger::FlushAll();
        RiftForged::Utilities::Logger::Shutdown();
        return 1;
    }
    catch (...) {
        RF_CORE_CRITICAL("Server: An unknown, unhandled exception occurred during startup or main loop.");
        if (gameServerEngine.isSimulating()) gameServerEngine.StopSimulationLoop(); // Add IsSimulating if it exists
        if (packetHandler) packetHandler->Stop();
        if (udpSocket) udpSocket->Stop();
        gameServerEngine.Shutdown();
        RiftForged::Utilities::Logger::FlushAll();
        RiftForged::Utilities::Logger::Shutdown();
        return 1;
    }

    // --- Server Shutdown Sequence ---
    RF_CORE_INFO("MAIN: Initiating graceful server shutdown...");

    // Stop simulation loop first to prevent further game logic processing
    gameServerEngine.StopSimulationLoop();

    // Stop network components
    if (packetHandler) {
        RF_CORE_INFO("MAIN: Signaling UDPPacketHandler to stop...");
        packetHandler->Stop();
    }
    if (udpSocket) {
        RF_CORE_INFO("MAIN: Signaling UDPSocketAsync (NetworkIO) to stop...");
        udpSocket->Stop();
    }

    // Shutdown GameServerEngine's internal resources (like its thread pool)
    gameServerEngine.Shutdown();

    // Message Handlers and Dispatcher will be cleaned up by unique_ptr destructors
    // PhysicsEngine, GameplayEngine, PlayerManager are stack-allocated or members managed by GameServerEngine

    RF_CORE_INFO("MAIN: Flushing and shutting down logger...");
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();

    std::cout << "MAIN: Server shut down gracefully." << std::endl;
    return 0;
}