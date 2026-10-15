#include "server.h"

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

bool SendMsg(socket_t socket_fd, const char* data, size_t length) 
{
    Print ("Message to send: " + std::string(data));

    while (length > 0) 
    {
        int bytes_sent = send(socket_fd, data, static_cast<int>(length), 0);
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

std::string RecvMsg(socket_t client_fd)
{
    char buffer[4096] = {0};
    //memset (buffer, '\0', sizeof(buffer));
    std::string response = "";
    std::string prefix = "(client-" + std::to_string(client_fd) + ") ";

    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        response = std::string(buffer);

        Print (prefix + "Msg received: " + response + "\n");
    }
    return response;
}

int main() 
{
    if (!StartUp())
        return 1;

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

    Print ("Server PID: " + std::to_string(getpid()));

    std::thread accept_thread(AcceptHandler, server_fd);
    SLEEP(5);

    int choice;
    socket_t client_fd;
    while (true)
    {
        Print ("Choices:\n1. List Out Clients\n2. Check Heartbeat\n3. Recv File\n4. Send File\n5. Reset\n");
        Print ("Enter choice: ");
        std::cin >> choice;
        switch (choice)
        {
            //List out clients
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

            //send heartbeat msg
            case 2:
            {
                //std::string msg = "Are you alive?";
                std::string cmd = "heartbeat";
                Print ("Client Id: ");
                std::cin >> client_fd;
                Print ("Command: ");
                //std::getline (std::cin, msg);
                //std::cin >> cmd;
                bool ret = SendMsg(client_fd, cmd.c_str(), cmd.size());
                if (ret) RecvMsg(client_fd);
            }
            break;

            //sendfile msg
            case 3:
            {
                Print ("Client Id: ");
                std::cin >> client_fd;
                std::string cmd = "sendfile";
                bool ret = SendMsg(client_fd, cmd.c_str(), cmd.size());
                if (ret) RecvFile(client_fd);
            }
            break;

            //recvfile msg
            case 4:
            {
                Print ("Client Id: ");
                std::cin >> client_fd;
                std::string cmd = "recvfile";
                bool ret = SendMsg(client_fd, cmd.c_str(), cmd.size());
                if (ret) ret = SendFile(client_fd, "server.log");
                if (ret) Print ("File sent");
                else Print ("File not sent");
            }
            break;

            case 5:
            break;

            default:
            break;
        }
    }

    serverUp = false;

    //server_fd is no longer usable, unblocks accept()
    shutdown(server_fd, SHUT_RDWR);

    Print ("Waiting for thread to finish.");
    accept_thread.join();
    Print ("Thread joined.");

    CLOSE_SOCKET(server_fd);

    CleanUp();

    return 0;
}
