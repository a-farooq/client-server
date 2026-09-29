#include <iostream>
#include <cstring>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <signal.h>
#include <unordered_map>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define socket_t SOCKET
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SHUT_RDWR SD_BOTH
    #define INVALID_SOCKET_VALUE INVALID_SOCKET

#elif __linux__
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #define socket_t int
    #define CLOSE_SOCKET(s) close(s)
    #define INVALID_SOCKET_VALUE -1
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

bool SendMsg(socket_t socket_fd, const char* data, size_t length) 
{
    Print ("Message sent: " + std::string(data));

    while (length > 0) 
    {
        int sent = send(socket_fd, data, static_cast<int>(length), 0);

        if (sent <= 0) {
            return false;
        }

        data += sent;
        length -= static_cast<size_t>(sent);
    }

    return true;
}

std::string RecvMsg(socket_t client_fd)
{
    char buffer[4096];
    memset (buffer, '\0', sizeof(buffer));
    int bytes_received;
    std::string prefix = "client-" + std::to_string(client_fd) + ": ";

    bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    buffer[bytes_received] = '\0';

    Print (prefix + "Data received: " + buffer + "\n");
    std::string response(buffer);
    return response;
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
void HandleAccept (socket_t server_fd)
{
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
        //std::thread client_th(HandleClient, client);
        //client_th.detach();
    }
    Print ("Server exited.");
}

int main() 
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) 
	{
        Print ("Error: WSAStartup failed\n");
        return 1;
    }
#endif

    const unsigned short port = 50005;
    //const char* server_ip = "127.0.0.1";
    const char* server_ip = "172.18.114.232";

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) 
	{
        Print ("Error: socket() failed\n");
        return 1;
    }

    //signal(SIGINT, OnSIGINT);

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    //address.sin_addr.s_addr = INADDR_ANY;
    address.sin_addr.s_addr = inet_addr(server_ip);
    address.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) 
	{
        Print ("Error: bind failed\n");
        CLOSE_SOCKET(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) 
	{
        Print ("Error: listen failed\n");
        CLOSE_SOCKET(server_fd);
        return 1;
    }

    Print ("Server listening on port " + std::to_string(port) + '\n');

    std::thread accept_thread(HandleAccept, server_fd);

    int choice;
    while (true)
    {
        Print ("Choices:\n1. List Out Clients\n2. Send Message\n3. Reset\n");
        Print ("Enter choice: ");
        std::cin >> choice;
        switch (choice)
        {
            case 1:
            {
                for (auto iter = clientmap.begin(); iter != clientmap.end(); iter++)
                {
                    Print (std::to_string(iter->first) + ": (" + iter->second->ip +
                        ", " + iter->second->os + 
                        ", " + iter->second->comments + 
                        ")\n");
                }
            }
            break;

            case 2:
            {
                socket_t client_fd;
                std::string msg = "Are you alive?";
                Print ("Client Id: ");
                std::cin >> client_fd;
                Print ("Message: ");
                //std::getline (std::cin, msg);
                SendMsg(client_fd, msg.c_str(), msg.size());
                RecvMsg(client_fd);
            }
            break;

            case 3:
            break;

            default:
            break;
        }
    }

    serverUp = false;

    //server_fd is no longer usable, unblocks accept()
    shutdown(server_fd, SHUT_RDWR);
    accept_thread.join();
    CLOSE_SOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
