#include <iostream>
#include <cstring>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <signal.h>
#include <fcntl.h>
#include <unordered_map>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define socket_t SOCKET
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SLEEP(sec) Sleep(sec)
    #define get_tid() GetCurrentThreadId()
    #define SHUT_RDWR SD_BOTH
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define tid_t DWORD

#elif __linux__
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    #define socket_t int
    #define CLOSE_SOCKET(s) close(s)
    #define SLEEP(sec) sleep(sec)
    #define get_tid() syscall(SYS_gettid)
    #define INVALID_SOCKET_VALUE -1
    #define tid_t pid_t
#endif

struct Client
{
    //int id;
    int fd;
    std::string ip;
    std::string os;
    std::string comments;
};

std::unordered_map <socket_t, std::unique_ptr<Client>> clientmap;
std::atomic<bool> serverUp {true};
std::mutex mtx;
socket_t server_fd = INVALID_SOCKET_VALUE;

void Print (std::string msgToPrint)
{
    std::lock_guard <std::mutex> lock(mtx); 
    std::cout << msgToPrint << std::endl;
}

bool StartUp()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) 
    {
        Print ("Error: WSAStartup failed\n");
        return false;
    }
#endif
    return true;
}

void CleanUp()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void OnSIGINT(int)
{
    serverUp = false;
    shutdown(server_fd, SHUT_RDWR);
}
/*
void HandleClient (std::shared_ptr <Client> client)
{
    std::string prefix = "client-" + std::to_string(client->fd) + ": ";

    Print (prefix + "Worker thread started\n");

    char buffer[4096];
    memset (buffer, '\0', sizeof(buffer));

    int bytes_received;
    
    while ((bytes_received = recv(client->fd, buffer, sizeof(buffer) - 1, 0)) > 0) 
    {
        buffer[bytes_received] = '\0';

        Print (prefix + "Data received: " + buffer + "\n");

        const char reply[] = "OK\n";
        send(client->fd, reply, static_cast<int>(sizeof(reply) - 1), 0);
    }

    Print (prefix + "Client disconnected\n");
    CLOSE_SOCKET(client->fd);
}
*/
