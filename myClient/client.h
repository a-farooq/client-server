#include <cstdlib>
#include <cstring>
#include <iostream>
//#include <fstream>
#include <string>
#include <fcntl.h>
#include <cstdint>

#ifdef _WIN32
    #include <winsock2.h> //must come first
    #include <ws2tcpip.h>
    //#pragma comment(lib, "ws2_32.lib")
    #define PLATFORM "Windows"
    #define socket_t SOCKET 
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SLEEP(sec) Sleep(sec*1000)
    #define WSACLEANUP() WSACleanup()
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define GET_ERRNO() WSAGetLastError()

    uint64_t be64toh(uint64_t x) { return _byteswap_uint64(x); }
    uint64_t htobe64(uint64_t x) { return _byteswap_uint64(x); }

#elif __linux__
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <sys/sendfile.h>
    #include <unistd.h>

    #define PLATFORM "Linux"
    #define socket_t int 
    #define CLOSE_SOCKET(s) close(s)
    #define SLEEP(sec) sleep(sec)
    #define WSACLEANUP() 
    #define INVALID_SOCKET_VALUE -1
    #define GET_ERRNO() errno

#endif


int StartUp()
{
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) 
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif
    return 0;
}

void Print (std::string msgToPrint)
{
    std::cout << msgToPrint << std::endl;
}


std::string GetOSInfo()
{
    std::string os = "";
#ifdef _WIN32
    os = "Windows";
/*
    OSVERSIONINFOEXW version = {};
    version.dwOSVersionInfoSize = sizeof(version);

    SYSTEM_INFO system_info;
    GetNativeSystemInfo(&system_info);

    if (GetVersionExW(
            reinterpret_cast<LPOSVERSIONINFOW>(&version))) {
        os += " ";
        os += version.dwMajorVersion;
        os += ".";
        os += version.dwMinorVersion; 
        os += " (Build ";
        os += version.dwBuildNumber;
        os += "), ";
    }

    os += "Architecture: ";

    switch (system_info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            os += "x86-64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            os += "ARM64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            os += "x86";
            break;
        default:
            os += "Unknown";
            break;
    }*/
#elif __linux__
    os = "Linux";
    /*
    struct utsname info;

    if (uname(&info) == 0) {
        os = "OS: " + info.sysname + ',';
        os += "Release: " + info.release + ',';
        os += "Version: " + info.version + ',';
        os += "Architecture: " + info.machine + ',';
    }*/
#endif    
    return os;
}

