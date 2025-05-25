//#include <iostream>
//#include <winsock2.h>
//#include "NetworkEngine.h"
//
//#pragma comment(lib, "ws2_32.lib")
//
//int main() {
//    // Initialize Winsock (only once in main)
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        std::cerr << "WSAStartup failed!" << std::endl;
//        return 1;
//    }
//
//    // Initialize NetworkEngine (IOCP for TCP)
//    NetworkEngine netEngine;
//    if (!netEngine.Initialize()) {
//        std::cerr << "Failed to initialize NetworkEngine!" << std::endl;
//        WSACleanup();
//        return 1;
//    }
//
//    // Create and associate TCP socket (overlapped)
//    SOCKET tcpSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
//    if (tcpSocket == INVALID_SOCKET) {
//        std::cerr << "Failed to create TCP socket!" << std::endl;
//        netEngine.Shutdown();
//        WSACleanup();
//        return 1;
//    }
//
//    netEngine.AssociateSocket(tcpSocket);
//
//    // Start IOCP worker threads
//    netEngine.RunWorkerThreads(4);  // Adjust thread count as needed
//
//    std::cout << "NetworkEngine running..." << std::endl;
//
//    // Cleanup
//    netEngine.Shutdown();
//    closesocket(tcpSocket);
//    WSACleanup();
//
//    return 0;
//}