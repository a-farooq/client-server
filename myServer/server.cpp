#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
#endif

int main() 
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) 
	{
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    const unsigned short port = 50005;
    //const char* server_ip = "127.0.0.1";
    const char* server_ip = "172.18.114.232";

    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) 
	{
        std::cerr << "socket() failed\n";
        return 1;
    }

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
        std::cerr << "bind() failed\n";
        CLOSE_SOCKET(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) 
	{
        std::cerr << "listen() failed\n";
        CLOSE_SOCKET(server_fd);
        return 1;
    }

    std::cout << "Server listening on port " << port << '\n';

    while (true) 
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
            std::cerr << "accept() failed\n";
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));

        std::cout << "Client connected from: " << client_ip << '\n';

        char buffer[4096];
        memset (buffer, '\0', sizeof(buffer));

        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        buffer[bytes_received] = '\0';
        std::cout << "Client details: " << buffer << '\n';

        memset (buffer, '\0', sizeof(buffer));

        while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) 
		{
            buffer[bytes_received] = '\0';

            std::cout << "Data received: " << buffer << '\n';

            const char reply[] = "OK\n";
            send(client_fd, reply, static_cast<int>(sizeof(reply) - 1), 0);
        }

        std::cout << "Client disconnected\n";
        CLOSE_SOCKET(client_fd);
    }

    CLOSE_SOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
