// In UDPServerApp.cpp or main.cpp
// ... (includes for other components) ...
#include "GameServerEngine.h" // Include the new engine
#include "../NetworkEngine/UDPSocketAsync.h" // Include the UDPSocketAsync header
#include "../NetworkEngine/PacketProcessor.h" // Include the PacketProcessor header
#include "../NetworkEngine/MovementMessageHandler.h"
#include "../NetworkEngine/RiftStepMessageHandler.h"
#include "../NetworkEngine/AbilityMessageHandler.h"
#include "../NetworkEngine/MessageDispatcher.h"
#include "../NetworkEngine/PingMessageHandler.h"
#include "Gameplay/PlayerManager.h"
#include "Gameplay/GameplayEngine.h"

std::atomic<bool> m_isSimulating = false; // Global atomic flag for simulation state

int main() {
    std::cout << "RiftForged UDPServer (GameServerEngine) Starting..." << std::endl;

    // 1. Instantiate Core Game Logic and Player Management
    RiftForged::GameLogic::PlayerManager playerManager;
    RiftForged::Gameplay::GameplayEngine gameplayEngine; // For later

    // 2. Instantiate Message Handlers
    RiftForged::Networking::UDP::C2S::MovementMessageHandler movementHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::RiftStepMessageHandler riftStepHandler(playerManager, gameplayEngine);
    RiftForged::Networking::UDP::C2S::AbilityMessageHandler abilityHandler(playerManager);
    RiftForged::Networking::UDP::C2S::PingMessageHandler pingHandler(playerManager);

    // 3. Instantiate MessageDispatcher
    RiftForged::Networking::MessageDispatcher messageDispatcher(
        movementHandler, riftStepHandler, abilityHandler, pingHandler);
    std::cout << "MessageDispatcher created." << std::endl;

    // 4. Instantiate PacketProcessor
    RiftForged::Networking::PacketProcessor packetProcessor(messageDispatcher);
    std::cout << "PacketProcessor created." << std::endl;

    // 5. Instantiate UDPSocketAsync
    RiftForged::Networking::UDPSocketAsync udpSocket(playerManager, packetProcessor, "0.0.0.0", 12345);
    std::cout << "UDPSocketAsync object created." << std::endl;

    // 6. Instantiate GameServerEngine
    RiftForged::Server::GameServerEngine gameServerEngine(playerManager, udpSocket);
    std::cout << "GameServerEngine object created." << std::endl;

    // 7. Initialize Network Layer
    if (!udpSocket.Init()) {
        std::cerr << "MAIN: Failed to initialize UDPSocketAsync. Exiting." << std::endl;
        return -1;
    }
    std::cout << "MAIN: UDPSocketAsync initialized." << std::endl;

    // 8. Start Network Layer
    if (!udpSocket.Start()) {
        std::cerr << "MAIN: Failed to start UDPSocketAsync. Exiting." << std::endl;
        udpSocket.Stop();
        return -1;
    }
    std::cout << "MAIN: UDPSocketAsync started." << std::endl;

    // 9. Start the GameServerEngine's Simulation Loop
    gameServerEngine.StartSimulationLoop();
    std::cout << "MAIN: GameServerEngine simulation loop started. Server is running." << std::endl;
    std::cout << "Type 'q' and press Enter to stop the server." << std::endl;

    // 10. Main application loop - waits for 'q' input in this main thread
    std::string input_line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input_line)) {
            // EOF or error on std::cin
            std::cout << "MAIN: std::cin error or EOF. Initiating shutdown." << std::endl;
            break;
        }
        if (input_line == "q" || input_line == "quit") {
            std::cout << "MAIN: Shutdown command received." << std::endl;
            break;
        }
        else if (input_line.rfind("broadcast ", 0) == 0) {
            if (input_line.length() > 10) {
                std::string message_to_broadcast = input_line.substr(10);
                std::cout << "MAIN: Broadcast requested: " << message_to_broadcast << std::endl;
                // TODO: Implement a thread-safe way to pass this to GameServerEngine
                // For example, GameServerEngine could have a method like:
                // gameServerEngine.QueueSystemBroadcast(message_to_broadcast);
                // Its SimulationTick would then pick this up and send it.
            }
        }
        else if (!input_line.empty()) {
            std::cout << "MAIN: Unknown command '" << input_line << "'. Type 'q' or 'quit' to exit." << std::endl;
        }
    }

    // --- Shutdown Sequence ---
    std::cout << "MAIN: Shutdown sequence initiated..." << std::endl;

    std::cout << "MAIN: Signaling GameServerEngine to stop..." << std::endl;
    gameServerEngine.StopSimulationLoop(); // This will set its internal m_isSimulatingThread to false and join

    std::cout << "MAIN: Signaling UDPSocketAsync to stop..." << std::endl;
    udpSocket.Stop(); // This will set its internal m_isRunning to false, post to IOCP, and join

    std::cout << "MAIN: Server shut down gracefully." << std::endl;
    return 0;
}