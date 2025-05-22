//#include "NetworkEngine2.h"
//#include <iostream>
//
//NetworkEngine::NetworkEngine() : iocpHandle(nullptr) {}
//
//NetworkEngine::~NetworkEngine() {
//    for (auto& worker : workerThreads) {
//        if (worker.joinable()) {
//            worker.join();  // Ensure proper cleanup
//        }
//    }
//    Shutdown();
//}
//
//bool NetworkEngine::Initialize() {
//    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//    if (!iocpHandle) {
//        std::cerr << "Failed to create IOCP instance!" << std::endl;
//        return false;
//    }
//    return true;
//}
//
//
//void NetworkEngine::Shutdown() {
//    engineRunning = false; // Stop worker threads
//    CloseHandle(iocpHandle);
//    iocpHandle = nullptr;
//}
//
//
//bool NetworkEngine::AssociateSocket(SOCKET sock) {
//    if (!CreateIoCompletionPort((HANDLE)sock, iocpHandle, (ULONG_PTR)sock, 0)) {
//        std::cerr << "Failed to associate socket with IOCP!" << std::endl;
//        return false;
//    }
//    return true;
//}
//
//void NetworkEngine::RunWorkerThreads(int numThreads) {
//    for (int i = 0; i < numThreads; ++i) {
//        workerThreads.emplace_back(WorkerThread, iocpHandle);
//    }
//}
//
//void NetworkEngine::PostCompletionStatus(ULONG_PTR key, DWORD bytesTransferred, OVERLAPPED* overlapped) {
//    PostQueuedCompletionStatus(iocpHandle, bytesTransferred, key, overlapped);
//}
//
//void NetworkEngine::WorkerThread(HANDLE iocpHandle) {
//    DWORD bytesTransferred;
//    ULONG_PTR completionKey;
//    OVERLAPPED* overlapped;
//    
//if (!iocpHandle) {
//        std::cerr << "Error: IOCP handle is NULL before worker thread execution!" << std::endl;
//        return;
//    }
//
//    while (true) {
//
//        BOOL success = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);
//        if (!success) {
//            std::cerr << "Worker thread encountered an error! Last Error: " << GetLastError() << std::endl;
//			break;  // Exit on error
//        }
//
//
//        // Handle the completed I/O operation (custom processing here)
//        std::cout << "Completion Key: " << completionKey << ", Bytes Transferred: " << bytesTransferred << std::endl;
//    }
//}
