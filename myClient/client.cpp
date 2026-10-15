#include "client.h"

ssize_t recv_exactly(int fd, char* buf, size_t len)
{
    size_t total = 0;
    //Print ("Start recv_exactly");
    while (total < len)
    {
        ssize_t bytes_received = recv(fd, buf + total, len - total, 0);
        //Print ("buf: " + std::string(buf));
        if (bytes_received <= 0)
        {
            Print ("Error: recv failed");
            return -1;
        }
        if (!strncmp(buf, "ERR", 5))
        {
            Print ("Error: File open failed");
            return -1;
        }
        total += bytes_received;
    }
    //Print ("End recv_exactly");
    return (ssize_t)total;
}

void RecvFile(socket_t client_fd)
{
    //Print ("Start RecvFile");
    // 1. Filename length
    uint32_t name_len;
    if (recv_exactly(client_fd, (char*)&name_len, 4) < 0)
        return;

    //Print ("name_len: " + std::to_string(name_len));

    if(name_len <= 0)
    {
        Print ("Error in receiving file");
        return;
    }

    name_len = ntohl(name_len);
    Print ("name_len: " + std::to_string(name_len));

    // 2. Filename
    char filename[name_len + 1];
    if (recv_exactly(client_fd, filename, name_len) < 0)
        return;

    filename[name_len] = '\0';
    Print ("file name: " + std::string(filename));

    // 3. File size
    uint64_t file_size;
    if (recv_exactly(client_fd, (char*)&file_size, 8) < 0)
        return;

    file_size = be64toh(file_size);
    Print ("file size: " + std::to_string(file_size));

    // 4. File data
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("Error: open failed."); 
        return;
    }

    char buf[65536] = {0};
    uint64_t total = 0;
    while (total < file_size)
    {
        size_t to_read = (file_size - total > sizeof(buf)) ? sizeof(buf) : (size_t)(file_size - total);
        ssize_t bytes_received = recv(client_fd, buf, to_read, 0);
        if (bytes_received <= 0)
        {
            perror("Error: recv failed.");
            break;
        }

        size_t total_written = 0;
        while (total_written < (size_t)bytes_received)
        {
            ssize_t bytes_written = write(fd, buf + total_written, bytes_received - total_written);
            if (bytes_written < 0)
            {
                perror("Error: write failed");
                break;
            }
            total_written += bytes_written;
        }
        total += bytes_received;
    }

    close(fd);

    char output[4096] = {0};
    sprintf(output, "Received: %s (%llu bytes)\n", filename, (unsigned long long)file_size);
    Print (std::string(output));
    //Print ("End RecvFile");
}

bool send_all(int fd, const char* buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        ssize_t bytes_sent = send(fd, buf + total, len - total, 0);
        if (bytes_sent <= 0)
        {
            perror("Error: send failed");
            return false;
        }
        total += bytes_sent;
    }
    return true;
}

bool SendFileData(socket_t sock_fd, int fd, long int file_size)
{
    bool ret = true;
#ifdef _WIN32
    char buf[65536];
    while (true)
    {
        ssize_t bytes_read = read(fd, buf, sizeof(buf));
        if (bytes_read <= 0)
            break;

        ret = send_all(sock_fd, buf, (size_t)bytes_read);
        if (!ret) break;
    }
#elif __linux__
    off_t offset = 0;
    while (offset < file_size)
    {
        ssize_t r = sendfile(sock_fd, fd, &offset, file_size - offset);
        if (r <= 0)
        {
            perror("Error: sendfile failed");
            ret = false;
            break;
        }
    }
#endif
    return ret;
}

bool SendFile(socket_t sock_fd, const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        Print ("Error: open failed");
        std::string error_msg = "ERR";
        send_all(sock_fd, error_msg.c_str(), 4);

        return false;
    }

    Print ("File opened for read");

    bool ret = true;
    struct stat st;
    fstat(fd, &st);

    // 1. Filename length
    uint32_t name_len = htonl((uint32_t)strlen(path));
    ret = send_all(sock_fd, (char*)&name_len, 4);

    // 2. Filename
    ret = send_all(sock_fd, path, strlen(path));

    // 3. File size
    uint64_t file_size = htobe64((uint64_t)st.st_size);
    ret = send_all(sock_fd, (char*)&file_size, 8);

    // 4. File data (zero-copy)
    ret = SendFileData(sock_fd, fd, st.st_size);

    close(fd);
    return ret;
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
    std::string prefix = "(client-" + std::to_string(sock_fd) + ") ";

    ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0)
    {
        Print ("Error: recv failed");
    }
    else
    {
        buffer[bytes_received] = '\0';
        std::string response(buffer);
        Print (prefix + "Msg received: " + response + "\n");

        if (!strncmp(buffer, "sendfile", 8))
        {
            if (!SendFile(sock_fd, "client.log"))
            {
                Print ("Error: File not sent");
            }
            else
            {
                Print ("File sent");
            }
        }
        else if (!strncmp(buffer, "recvfile", 8))
        {
            RecvFile(sock_fd);
        }
        else if (!strncmp(buffer, "heartbeat", 9))
        {
            std::string message = "Hello, I am listening.";
            SendMsg(sock_fd, message.c_str(), message.size());
        }
        else
        {
            std::string message = "Done";
            SendMsg(sock_fd, message.c_str(), message.size());
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

            std::string pid = std::to_string(getpid());
            Print ("Client PID: " + pid);

            std::string message = "Connected from " + std::string(PLATFORM) + " : " + pid;
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

        //std::string message = "Done";
        //SendMsg(sock, message.c_str(), message.size());
    }

    CLOSE_SOCKET(sock);
    WSACLEANUP();

    return 0;
}
