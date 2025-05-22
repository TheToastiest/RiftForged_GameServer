#pragma once
#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <thread>
#include <mutex>

class NetworkEngine {
public:
    NetworkEngine();
    ~NetworkEngine();

    bool Initialize();
    void Shutdown();
    bool AssociateSocket(SOCKET sock);
    void RunWorkerThreads(int numThreads);
    void PostCompletionStatus(ULONG_PTR key, DWORD bytesTransferred, OVERLAPPED* overlapped);

    bool engineRunning = true;

private:
    HANDLE iocpHandle;
    std::vector<std::thread> workerThreads;
    std::mutex engineMutex;

    static void WorkerThread(HANDLE iocpHandle);
};