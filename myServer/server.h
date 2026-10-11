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
    #include <stdlib.h>  // _byteswap_uint64

    #pragma comment(lib, "ws2_32.lib")
    #define socket_t SOCKET
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SLEEP(sec) Sleep(sec*1000)
    #define get_tid() GetCurrentThreadId()
    #define SHUT_RDWR SD_BOTH
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define tid_t DWORD

    uint64_t be64toh(uint64_t x) { return _byteswap_uint64(x); }
    uint64_t htobe64(uint64_t x) { return _byteswap_uint64(x); }

#elif __linux__
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/syscall.h>
    #include <sys/stat.h>
    #include <sys/sendfile.h>
    #include <unistd.h>
    #include <endian.h>

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

std::string RecvMsg(socket_t client_fd);

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
void AcceptHandler (socket_t server_fd)
{
    tid_t tid = get_tid();
    Print ("Thread ID: " + std::to_string(tid));
    //int cur_client_id = 1;
    while (serverUp) 
    {
        sockaddr_in client_address;
        socklen_t client_size = sizeof(client_address);

        socket_t client_fd = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_size
        );

        if (client_fd < 0) 
        {
            Print ("Error: accept() failed\n");
            if (errno == EINTR)
                break;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));

        Print ("Client connected from: " + std::string(client_ip) + '\n');

        //char buffer[4096];
        //memset (buffer, '\0', sizeof(buffer));

        //int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        //buffer[bytes_received] = '\0';

        //std::string platform = std::string(buffer);
        //Print ("Client details: " + platform + "\n");
        std::string platform = RecvMsg (client_fd);

        //Create client object
        std::unique_ptr<Client> client(new Client);
        //client->id = cur_client_id++;
        client->fd = client_fd;
        client->ip = client_ip;
        client->os = platform;
        client->comments = "";

        clientmap[client_fd] = std::move(client);
    }
    Print ("Server exited.");
}

