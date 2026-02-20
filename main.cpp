#include <charconv>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>
#include <string>

#include "server/include/mplexserver.h"
#include "srvMgr/include/SrvMgr.h"

using namespace MPlexServer;

//constexpr int   PORT = 6666;
//constexpr auto SERVER_PASSWORD = "abc";
constexpr auto SERVER_NAME = "irc.LeMaDa.hn";

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "command needs to be in this format:\n./ircserv <port> <password>\n";
        return 1;
    }
    int PORT = -1;
    auto result = std::from_chars(argv[1],argv[1]+strlen(argv[1]),PORT);
    if (!(result.ec == std::errc() && *result.ptr == '\0')) {
        std::cout << "Error parsing port argument!!!" << std::endl;
        return 1;
    }
    std::string SERVER_PASSWORD = argv[2];

    Server  srv(PORT);
    //UserManager um(srv);
    SrvMgr sm(srv, SERVER_PASSWORD, SERVER_NAME);
    srv.setEventHandler(&sm);
    srv.setVerbose(2);  // 1: Reduce logging - only important messages 2: Debug info - verbose
    
    try {
        srv.activate();
        std::cout << "[SERVER] Started on port " << PORT << std::endl;
    } catch (std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
    
    // Heartbeat tracking
    auto server_start = std::chrono::steady_clock::now();
    auto last_heartbeat = std::chrono::steady_clock::now();
    const auto heartbeat_interval = std::chrono::seconds(10);
    
    std::cout << "[SERVER] Alive - waiting for connections..." << std::endl;
    
    while (true) {
        srv.poll();
        
        // Check if 10 seconds have passed for heartbeat
        auto now = std::chrono::steady_clock::now();
        if (now - last_heartbeat >= heartbeat_interval) {
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - server_start).count();
            std::cout << "[SERVER] Alive - Uptime: " << uptime << "s" << std::endl;
            last_heartbeat = now;
        }
    }

    return 0;
}