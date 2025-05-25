//// main.cpp - Revised
//#include "UDPSocketAsync.h"     // Adjust path as per your project structure
//#include "MessageDispatcher.h" // Adjust path
//#include "MovementMessageHandler.h" // Adjust path
//#include "RiftStepMessageHandler.h" // Adjust path
//#include "AbilityMessageHandler.h"  // Adjust path
//#include "PingMessageHandler.h"   // Adjust path
//#include "PlayerManager.h"
//#include "../GameServer/GameServerEngine.h"
//#include <iostream>
//#include <string>
//#include <vector>
//#include <thread>         // For std::this_thread::sleep_for
//#include <chrono>         // For std::chrono::milliseconds
//
// Define MAX_WORKER_THREADS (can also be a const in UDPSocketAsync.h or from config)
// const int MAX_WORKER_THREADS = 6; // This was in your UDPSocketAsync.cpp, ensure it's accessible or defined
//
// Global flag to signal shutdown for the input thread and main loop
//std::atomic<bool> g_serverRunning(true);
//std::condition_variable g_shutdownCondition;
//std::mutex g_shutdownMutex;
//
//void AdminConsoleThread() {
//    std::string line;
//    std::cout << "Server Admin Console: Type 'q' or 'quit' to initiate shutdown." << std::endl;
//    while (g_applicationRunning.load()) {
//        std::cout << "> " << std::flush;
//        if (std::getline(std::cin, line)) {
//            if (line == "q" || line == "quit") {
//                if (g_applicationRunning.exchange(false) == true) { // Ensure we only signal once
//                    std::cout << "ADMIN: Shutdown command received." << std::endl;
//                }
//                break;
//            }
//            else if (line.rfind("broadcast ", 0) == 0) {
//                if (line.length() > 10) {
//                    std::string message_to_broadcast = line.substr(10);
//                    std::cout << "ADMIN: Broadcast requested: " << message_to_broadcast << std::endl;
//                    // TODO: Implement a thread-safe queue to pass this message to GameServerEngine
//                    // for it to build the S2C_SystemBroadcastMsg and send via UDPSocketAsync
//                }
//            }
//            // Add other admin commands here
//        }
//        else if (std::cin.eof() || std::cin.bad()) {
//            if (g_applicationRunning.exchange(false) == true) {
//                std::cout << "ADMIN: Console input error or EOF. Initiating shutdown." << std::endl;
//            }
//            break;
//        }
//    }
//    std::cout << "AdminConsoleThread exiting." << std::endl;
//}
////int main() {
//    std::cout << "RiftForged UDPServer Starting..." << std::endl;
//
//    RiftForged::GameLogic::PlayerManager playerManager;
//    // GameplayEngine gameplayEngine; // Instantiate when ready
//
//    RiftForged::Networking::UDP::C2S::MovementMessageHandler movementHandler(playerManager /*, gameplayEngine*/);
//    RiftForged::Networking::UDP::C2S::RiftStepMessageHandler riftStepHandler(playerManager /*, gameplayEngine*/);
//    RiftForged::Networking::UDP::C2S::AbilityMessageHandler abilityHandler(playerManager /*, gameplayEngine*/);
//    RiftForged::Networking::UDP::C2S::PingMessageHandler pingHandler;
//
//    RiftForged::Networking::MessageDispatcher messageDispatcher(
//        movementHandler, riftStepHandler, abilityHandler, pingHandler);
//
//    RiftForged::Networking::PacketProcessor packetProcessor(messageDispatcher);
//
//    RiftForged::Networking::UDPSocketAsync udpSocket(playerManager, packetProcessor, "0.0.0.0", 12345);
//
//    RiftForged::Server::GameServerEngine gameEngine(playerManager, udpSocket /*, gameplayEngine */);
//
//     if (!udpSocket.Init()) {
//        std::cerr << "MAIN: Failed to initialize UDPSocketAsync." << std::endl;
//        return -1;
//    }
//    std::cout << "MAIN: UDPSocketAsync initialized." << std::endl;
//
//    if (!udpSocket.Start()) {
//        std::cerr << "MAIN: Failed to start UDPSocketAsync." << std::endl;
//        udpSocket.Stop();
//        return -1;
//    }
//    std::cout << "MAIN: UDPSocketAsync started." << std::endl;
//
//    gameEngine.StartSimulationLoop();
//    std::cout << "MAIN: GameServerEngine simulation loop started. Server is running." << std::endl;
//
//    // Start admin console input thread
//    std::thread adminThread(AdminConsoleThread);
//
//    // Main application loop - just waits for g_applicationRunning to become false
//    while (g_applicationRunning.load()) {
//        std::this_thread::sleep_for(std::chrono::milliseconds(200));
//    }
//
//    std::cout << "MAIN: Shutdown sequence initiated..." << std::endl;
//
//    std::cout << "MAIN: Signaling GameServerEngine to stop..." << std::endl;
//    gameEngine.StopSimulationLoop(); // This will join its thread
//
//    std::cout << "MAIN: Signaling UDPSocketAsync to stop..." << std::endl;
//    udpSocket.Stop(); // This will post to IOCP and join its threads
//
//    std::cout << "MAIN: Joining admin console thread..." << std::endl;
//    if (adminThread.joinable()) { // Ensure admin thread is joinable
//        // To make adminThread truly joinable if it's stuck on std::getline,
//        // you'd need a more complex non-blocking input or close std::cin,
//        // which is platform-dependent and tricky. For now, detach or simple 'q' command is okay.
//        // If using detached: adminThread.detach(); // and remove join.
//        // For this example, let's assume 'q' in admin console sets g_applicationRunning.
//        // Or main sets g_applicationRunning false, and admin thread checks it.
//        // The current AdminConsoleThread will exit when g_applicationRunning is false and it gets an input or EOF.
//        // To make it exit without waiting for input, more advanced console handling is needed.
//        // For now, just let it be. If it was detached, we don't join. If not, we try.
//        // For this simple example, if you type 'q' in main console (or admin console), it will work.
//        // If adminThread used std::cin, and main sets g_applicationRunning to false, adminThread might still block on getline.
//        // A better AdminConsoleThread would use non-blocking input or check g_applicationRunning more frequently.
//        // For a clean exit of adminThread here, if it's blocked on std::cin, one might need to
//        // simulate an input or use platform specific ways to interrupt std::cin.
//        // Simplest for robust shutdown: the admin console 'q' command is the primary trigger for shutdown.
//        // The main loop 'q' here is redundant if admin console has it.
//       // Let's assume 'q' in admin console correctly sets g_applicationRunning = false.
//        if (adminThread.joinable()) {
//            adminThread.join();
//        }
//    }
//
//
//    std::cout << "MAIN: Server shut down gracefully." << std::endl;
//    return 0;
//}