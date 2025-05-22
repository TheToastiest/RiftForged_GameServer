//#ifndef UDP_HANDLER_H
//#define UDP_HANDLER_H
//
//#include <winsock2.h>
//#include <windows.h>
//#include <unordered_map>
//#include <vector>
//#include <thread>
//#include <queue>
//#include <mutex>
//#include <condition_variable>
//#include <iostream>
//#include "Logger.h"
//#include "GamePacketHeader.h"
//
//// ThreadPool Class for Efficient Handling
//class ThreadPool {
//public:
//    explicit ThreadPool(size_t threadCount);
//    ~ThreadPool();
//
//    void enqueueTask(std::function<void()> task);
//
//private:
//    std::vector<std::thread> workers;
//    std::queue<std::function<void()>> tasks;
//    std::mutex queueMutex;
//    std::condition_variable condition;
//    bool stop;
//};
//
//class UDPHandler {
//public:
//    explicit UDPHandler(Logger* loggerInstance, int gameplayPort, int movementPort, int diagnosticsPort);
//    ~UDPHandler();
//
//    void start();
//    void stop();
//    void sendPacket(SOCKET sock);
//    void sendACK(uint32_t packetID, sockaddr_in& destAddr, SOCKET sock);
//
//private:
//    SOCKET gameplaySocket, movementSocket, diagnosticsSocket;
//    sockaddr_in gameplayAddr, movementAddr, diagnosticsAddr;
//    Logger* logger;
//    ThreadPool threadPool;
//    bool running;
//
//    std::unordered_map<uint32_t, bool> receivedACKs;
//
//    void setupSocket(SOCKET& sock, sockaddr_in& addr, int port);
//    void receivePackets(SOCKET sock);
//    bool validatePacket(const char* buffer, int size);
//    void handleSocketError(const std::string& operation);
//    void cleanup();
//};
//
//#endif // UDP_HANDLER_H