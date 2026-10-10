#include "client.h"

void send_all(int fd, const char* buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        ssize_t bytes_sent = send(fd, buf + total, len - total, 0);
        if (bytes_sent <= 0)
        {
            perror("Error: send failed");
            return;
        }
        total += bytes_sent;
    }
}

void SendFileData(socket_t sock_fd, int fd, long int file_size)
{
#ifdef _WIN32
    char buf[65536];
    while (true)
    {
        ssize_t bytes_read = read(fd, buf, sizeof(buf));
        if (bytes_read <= 0)
            break;

        send_all(sock_fd, buf, (size_t)bytes_read);
    }
#elif __linux__
    off_t offset = 0;
    while (offset < file_size)
    {
        ssize_t r = sendfile(sock_fd, fd, &offset, file_size - offset);
        if (r <= 0)
        {
            perror("Error: sendfile failed");
            break;
        }
    }
#endif
}

bool SendFile(socket_t sock_fd, const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        Print ("Error: open failed");
        return false;
    }

    Print ("File opened for read");

    struct stat st;
    fstat(fd, &st);

    // 1. Filename length
    uint32_t name_len = htonl((uint32_t)strlen(path));
    send_all(sock_fd, (char*)&name_len, 4);

    // 2. Filename
    send_all(sock_fd, path, strlen(path));

    // 3. File size
    uint64_t file_size = htobe64((uint64_t)st.st_size);
    send_all(sock_fd, (char*)&file_size, 8);

    // 4. File data (zero-copy)
    SendFileData(sock_fd, fd, st.st_size);

    close(fd);
    return true;
}

bool SendMsg(socket_t sock, const char* data, size_t length) 
{
    Print ("Message to send: " + std::string(data));

    while (length > 0) 
    {
        int bytes_sent = send(sock, data, static_cast<int>(length), 0);
        if (bytes_sent <= 0)
        {
            Print ("Error: send failed");
            return false;
        }

        //move pointer by sent bytes
        data += bytes_sent;
        //update length of unsent bytes
        length -= static_cast<size_t>(bytes_sent);
    }

    Print ("Returning true");

    return true;
}

ssize_t RecvMsg(socket_t sock_fd)
{
    char buffer[4096] = {0};
    //memset (buffer, '\0', sizeof(buffer));
    std::string prefix = "client-" + std::to_string(sock_fd) + ": ";

    ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        std::string response(buffer);
        Print (prefix + "Msg received: " + response + "\n");

        if (!strncmp(buffer, "sfile", 5))
        {
            if (!SendFile(sock_fd, "client.log"))
            {
                std::string message = "-1";
                SendMsg(sock_fd, message.c_str(), message.size());
            }
            else
            {
                Print ("File sent");
            }
        }
        else
        {
            std::string message = "Done";
            SendMsg(sock_fd, message.c_str(), message.size());
        }
    }
    else
    {
        Print ("Error: recv failed");
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
                //auto t1 = std::chrono::steady_clock::now();
                SLEEP (10);
                //auto t2 = std::chrono::steady_clock::now();
                //auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
                //std::cout << "Slept for " << (long long)ms << "ms\n";
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
