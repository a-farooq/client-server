#include "client.h"

void SendFile(socket_t sock_fd, const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) 
    {
        Print ("Error: cannot open file");
        return;
    }

    struct stat st;
    fstat(fd, &st);

    //send filename + size as header
    char header[256] = {0};
    snprintf(header, sizeof(header), "%s %ld\n", path, (long)st.st_size);

    send(sock_fd, header, strlen(header), 0);

    //send file data in chunks
    char buf[65536];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        ssize_t sent = 0;
        while (sent < n)
        {
            ssize_t r = send(sock_fd, buf + sent, n - sent, 0);
            if (r <= 0)
            {
                Print ("Error: cannot send");
                close(fd);
                return;
            }
            sent += r;
        }
    }
    close(fd);
}

bool SendMsg(socket_t sock, const char* data, size_t length) 
{
    Print ("Message sent: " + std::string(data));

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

ssize_t RecvMsg(socket_t sock_fd)
{
    char buffer[4096];
    memset (buffer, '\0', sizeof(buffer));
    std::string prefix = "client-" + std::to_string(sock_fd) + ": ";

    ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        Print (prefix + "Data received: " + buffer + "\n");

        if (!strncmp(buffer, "sfile", 5))
        {
            SendFile(sock_fd, "output.log");
        }
    }
    return bytes_received;
}

int main(int argc, char* argv[]) 
{
    //const char* server_ip = argc > 1 ? argv[1] : "127.0.0.1";
    //unsigned short port = argc > 2 ? static_cast<unsigned short>(std::atoi(argv[2])) : 50005;

    ////////sanitize////////
    const char* server_ip = "172.18.114.232";
    unsigned short port = 50005;

    socket_t sock = INVALID_SOCKET_VALUE;
    sockaddr_in server_addr;
    bool connected = false;

    while (true)
    {
        if (sock == INVALID_SOCKET_VALUE) 
        {
            StartUp();
            std::cout << "Creating socket..." << std::endl;
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if (sock == INVALID_SOCKET_VALUE) 
            {
                std::cerr << "Could not create socket\n";
                SLEEP(10);
                continue;
                //WSACLEANUP();
                //return 1;
            }
            std::cout << "Socket created" << std::endl;

            std::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);

            if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) 
            {
                std::cerr << "Invalid IPv4 address: " << server_ip << '\n';
                SLEEP(10);
                continue;
                //CLOSE_SOCKET(sock);
                //WSACLEANUP();
                //return 1;
            }
        }

        while (!connected)
        {
            if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) 
            {
               std::cerr << "Could not connect to " << server_ip << ":" << port << '\n';
                std::cout << "Retrying..." << std::endl;
                SLEEP (10);
                continue;
                //CLOSE_SOCKET(sock);
                //WSACLEANUP();
            }
            connected = true;
            std::cout << "Connected to " << server_ip << ":" << port << '\n';

            std::string message = PLATFORM;
            SendMsg(sock, message.c_str(), message.size());
            break;
        }
        //std::cout << "Enter text. Press Ctrl+D (Linux) or Ctrl+Z then Enter (Windows) to exit.\n";

        ssize_t ret = RecvMsg(sock);

        if (ret <= 0)
        {
            std::cout << "Connection lost, reconnecting...\n";
            connected = false;
            CLOSE_SOCKET(sock);
            WSACLEANUP();
            sock = INVALID_SOCKET_VALUE;
            continue;
        }

        std::string message = "Done";
        SendMsg(sock, message.c_str(), message.size());
    }

    CLOSE_SOCKET(sock);
    WSACLEANUP();

    return 0;
}
