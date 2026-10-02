#include "server.h"

void RecvFile(socket_t client_fd)
{
    char header[256] = {0};
    recv(client_fd, header, sizeof(header)-1, 0);

    char filename[256] = {0};
    long filesize;
    sscanf(header, "%s %ld", filename, &filesize);

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) 
    {
        Print ("Error: cannot open file to write.");
        return;
    }

    Print ("Receiving and writing to file");
    char buf[65536] = {0};
    long total = 0;
    while (total < filesize)
    {
        ssize_t r = recv(client_fd, buf, sizeof(buf), 0);
        if (r <= 0)
        {
            Print ("Error: recv failed");
            break;
        }
        write(fd, buf, r);
        total += r;
    }
    close(fd);
}

bool SendMsg(socket_t socket_fd, const char* data, size_t length) 
{
    Print ("Message to send: " + std::string(data));

    while (length > 0) 
    {
        int sent = send(socket_fd, data, static_cast<int>(length), 0);

        if (sent <= 0)
        {
            Print ("Error: send failed");
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

    std::thread accept_thread(HandleAccept, server_fd);
    SLEEP(3);

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
                //std::string msg = "Are you alive?";
                std::string msg = "";
                Print ("Client Id: ");
                std::cin >> client_fd;
                Print ("Message: ");
                //std::getline (std::cin, msg);
                std::cin >> msg;
                SendMsg(client_fd, msg.c_str(), msg.size());
                if (msg == "sfile")
                {
                    RecvFile(client_fd);
                }
                else
                {
                    RecvMsg(client_fd);
                }
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

    Print ("Waiting for thread to finish.");
    accept_thread.join();
    Print ("Thread joined.");

    CLOSE_SOCKET(server_fd);

    CleanUp();

    return 0;
}
