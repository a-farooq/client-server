#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SLEEP(sec) Sleep(sec)
    #define WSACLEANUP() WSACleanup()
    const socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;

#elif __linux__
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
    #define SLEEP(sec) sleep(sec)
    #define WSACLEANUP() 
    const socket_t INVALID_SOCKET_VALUE = -1;

#endif

bool send_all(socket_t sock, const char* data, size_t length) 
{
    while (length > 0) 
    {
        int sent = send(sock, data, static_cast<int>(length), 0);

        if (sent <= 0) {
            return false;
        }

        data += sent;
        length -= static_cast<size_t>(sent);
    }

    return true;
}

int main(int argc, char* argv[]) 
{
    //const char* server_ip = argc > 1 ? argv[1] : "127.0.0.1";
    //unsigned short port = argc > 2 ? static_cast<unsigned short>(std::atoi(argv[2])) : 50005;
    const char* server_ip = "172.18.114.232";
    unsigned short port = 50005;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) 
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    while (true)
    {
        std::cout << "Creating socket..." << std::endl;
        socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (sock == INVALID_SOCKET_VALUE) 
        {
            std::cerr << "Could not create socket\n";
            //WSACLEANUP();
            SLEEP(10);
            continue;
            //return 1;
        }
        std::cout << "Socket created" << std::endl;

        sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) 
        {
            std::cerr << "Invalid IPv4 address: " << server_ip << '\n';
            //CLOSE_SOCKET(sock);
            //WSACLEANUP();
            SLEEP(10);
            continue;
            //return 1;
        }

        while (true)
        {
            if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) 
            {
                std::cerr << "Could not connect to " << server_ip << ":" << port << '\n';
                //CLOSE_SOCKET(sock);
                //WSACLEANUP();
                std::cout << "Retrying..." << std::endl;
                SLEEP (10);
                continue;
            }
            std::cout << "Connected" << std::endl;
            break;
        }

        std::cout << "Connected to " << server_ip << ":" << port << '\n';
        std::cout << "Enter text. Press Ctrl+D (Linux) or Ctrl+Z then Enter (Windows) to exit.\n";

        std::string message;
        char reply[256];

        while (std::getline(std::cin, message)) 
        {
            message += '\n';

            if (!send_all(sock, message.c_str(), message.size())) 
            {
                std::cerr << "Send failed\n";
                break;
            }

            int received = recv(sock, reply, sizeof(reply) - 1, 0);

            if (received <= 0) 
            {
                std::cout << "Server disconnected\n";
                break;
            }

            reply[received] = '\0';
            std::cout << "Server reply: " << reply;
        }

        CLOSE_SOCKET(sock);
    }
    WSACLEANUP();

    return 0;
}
