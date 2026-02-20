#include "../include/mplexserver.h"
#include <iomanip>
#include <sstream>

MPlexServer::Server::Server(uint16_t port, const std::string ipv4)
    : port(port == 0 ? 6667 : port), ipv4(ipv4) {
    this->verbose = 0;
    this->server_fd = -1;
    this->epollfd = -1;
    this->clientCount = 0;
    this->handler = nullptr;
}

MPlexServer::Server::~Server() {
    deactivate();
}

void MPlexServer::Server::sendTo(const Client &c, std::string msg) {
    log("Queueing " + std::to_string(msg.size()) + " bytes for fd " + std::to_string(c.getFd()) + ": [" + msg.substr(0, std::min(size_t(50), msg.size())) + "...", 2);
    if (send_buffer.find(c.getFd()) == send_buffer.end()) {
        send_buffer[c.getFd()] = msg;
        epoll_event ev{};
        ev.data.fd = c.getFd();
        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
        if (epoll_ctl(epollfd,EPOLL_CTL_MOD,c.getFd(),&ev) == -1) {
            log("Couldnt modify epoll instance.",1);
        }
    } else {
        log("Appending to existing buffer (size was " + std::to_string(send_buffer[c.getFd()].size()) + ")", 2);
        send_buffer[c.getFd()] += msg;
    }
}

void MPlexServer::Server::activate() {
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if (listen_fd < 0) {
        throw ServerError("Failed to open socket");
    }
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(listen_fd);
        throw ServerError("Failed to set SO_REUSEADDR");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (this->ipv4 != "") {
        if (inet_pton(AF_INET, this->ipv4.c_str(), &addr.sin_addr.s_addr) <= 0) {
            close(listen_fd);
            throw ServerSettingsError("Invalid IPv4 address");
        }
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
        close(listen_fd);
        throw ServerError("Failed to bind socket");
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        close(listen_fd);
        throw ServerError("Failed to listen socket");
    }
    setNonBlocking(listen_fd);
    const int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        close(listen_fd);
        throw ServerError("Failed to create epoll instance");
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        close(listen_fd);
        close(epoll_fd);
        throw ServerError("Failed to add server fd to epoll instance");
    }

    this->epollfd = epoll_fd;
    this->server_fd = listen_fd;

    log("Server successfully activated",1);
}

void MPlexServer::Server::log(const std::string message, const int required_level) const {
    if (this->verbose >= required_level) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = *std::localtime(&now_time);
        std::cout << "[MPlexServer][" << std::put_time(&local_tm, "%Y-%m-%d@%H:%M:%S") << "] " << message << std::endl;
    }
}

void MPlexServer::Server::deactivate() {
    for (const auto& [fd,c] : client_map) {
        if (epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,nullptr) == -1) {
            log("Critical error could not delete fd from epoll.",0);
        }
        close(fd);
    }
    this->clientCount = 0;
    this->client_map.clear();
    this->send_buffer.clear();
    this->recv_buffer.clear();
    if (server_fd != -1) close(server_fd);
    if (epollfd != -1) close(epollfd);
    server_fd = -1;
    epollfd = -1;
    log("Server has been deactivated.",1);
}

void MPlexServer::Server::setVerbose(const int level) {
    if (level <= VERBOSITY_MAX && level >= 0) {
        this->verbose = level;
    } else {
        throw ServerSettingsError("Verbosity level does not exist");
    }
}

int MPlexServer::Server::getVerbose() const {
    return this->verbose;
}

int MPlexServer::Server::getConnectedClientsCount() const {
    return this->clientCount;
}

void MPlexServer::Server::recv_from_fd(const int fd) {
    char buffer[MAX_MSG_LEN];
    std::string& r_buffer = recv_buffer[fd];
    const ssize_t n = recv(fd, buffer, MAX_MSG_LEN,0);
    if (n == 0) {
        log("Client disconnected (EOF)", 1);
        disconnectClient(fd);
        return;
    }
    if (n < 0) {
        switch (errno) {
            case EAGAIN:
                break;
            case ECONNRESET:
                log("Connection of client has been reset",1);
                disconnectClient(fd);
                break;
            case ETIMEDOUT:
                log("Client has timed out",1);
                disconnectClient(fd);
                break;
            default:
                log("Unkown error occured while reading from client",1);
                disconnectClient(fd);
                break;

        }
        return;
    }
    const size_t safe_len = std::min<size_t>(n,MAX_MSG_LEN-1);
    buffer[safe_len] = 0;
    r_buffer.append(buffer);
    while (r_buffer.find("\r\n") != std::string::npos) {
        std::string msg = r_buffer.substr(0,r_buffer.find("\r\n")+1);
        r_buffer.erase(0,r_buffer.find("\r\n")+2);
        log(buffer,2);
        callHandler(EventType::MESSAGE,client_map[fd],Message(msg,client_map[fd]));
    }
    // log(buffer,2);
}

void MPlexServer::Server::send_to_fd(int fd) {
    auto& buf = send_buffer[fd];
    if (buf.empty()) {
        modifyEpollFlags(fd, EPOLLIN | EPOLLRDHUP);
        send_buffer.erase(fd);
        return;
    }
    
    ssize_t sent = send(fd, buf.data(), buf.size(), MSG_NOSIGNAL);
    
    if (sent > 0) {
        log("Sent " + std::to_string(sent) + " bytes of " + std::to_string(buf.size()), 2);
        buf.erase(0, sent);
    }
    else if (sent < 0 && errno == EAGAIN) {
        log("Send would block (EAGAIN)", 2);
        return;
    } 
    else if (sent == 0) {
        log("Send returned 0, connection closed", 1);
        disconnectClient(fd);
        return;
    }
    else {
        log("Unknown error occurred while sending to client, errno: " + std::to_string(errno), 0);
        disconnectClient(fd);
        return;
    }
    
    if (buf.empty()) {
        modifyEpollFlags(fd, EPOLLIN | EPOLLRDHUP);
        send_buffer.erase(fd);
    }
}

void MPlexServer::Server::accept_client() {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    const int clientFd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr),&len);
    len = sizeof(client_addr);              // unneccessary? two lines up already done
    try {
        setNonBlocking(clientFd);
    } catch (std::runtime_error &e) {
        log(e.what(),0);
        close(clientFd);
        return;
    }

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = clientFd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientFd, &ev) == -1) {
        close(clientFd);
        log("Failed to add client to epoll.",0);
        return;
    }
    client_map[clientFd] = Client(clientFd, client_addr);
    clientCount++;
    log("New client accepted.",1);
    callHandler(EventType::CONNECTED,client_map[clientFd]);
}

void MPlexServer::Server::poll() {
    epoll_event events[MAX_EPOLL_EVENTS];
    int numEvents = 0;
    while (true) {
        numEvents = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, 1); // timeout (in ms) of 0 return immediately (potentially negating the use of poll?)
        if (numEvents == EAGAIN) {
            continue;
        }
        if (numEvents == -1) {
            log("Failed to poll events.",0);
            return;
        }
        break;
    }

    for (int i = 0; i < numEvents; ++i) {
        if (events[i].data.fd == this->server_fd && events[i].events & EPOLLIN) {
            accept_client();
        } else {
            if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                log("Client disconnected.", 1);
                disconnectClient(events[i].data.fd);
                continue;
            }
            if (events[i].events & EPOLLIN) {
                recv_from_fd(events[i].data.fd);
            }
            if (events[i].events & EPOLLOUT) {
                if (send_buffer.find(events[i].data.fd) == send_buffer.end()) {
                    modifyEpollFlags(events[i].data.fd,EPOLLIN | EPOLLRDHUP);
                    continue;
                }
                send_to_fd(events[i].data.fd);
            }
        }
    }
    for (const int fd : disconnect_queue) {
        deleteClient(fd);
    }
    disconnect_queue.clear();
}

void MPlexServer::Server::modifyEpollFlags(const int fd, const int flags) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = flags;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD,fd,&ev) == -1) {
        log("Failed to mod epoll.",0);
    }
}

void MPlexServer::Server::disconnectClient(const Client& c) {
    if (client_map.find(c.getFd()) == client_map.end())
        return;
    callHandler(EventType::DISCONNECTED,client_map[c.getFd()]);
    disconnect_queue.push_back(c.getFd());
}

void MPlexServer::Server::disconnectClient(const int fd) {
    if (client_map.find(fd) == client_map.end())
        return;
    callHandler(EventType::DISCONNECTED,client_map[fd]);
    disconnect_queue.push_back(fd);
}

void MPlexServer::Server::deleteClient(const int fd) {
    client_map.erase(fd);
    clientCount--;
    send_buffer.erase(fd);
    recv_buffer.erase(fd);
    if (epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,nullptr) == -1) {
        log("Critical error could not delete fd from epoll.",0);
    }
    log("Closing client file descriptor.",2);
    close(fd);
}

void MPlexServer::Server::setEventHandler(EventHandler *handler) {
    this->handler = handler;
}

void MPlexServer::Server::callHandler(EventType event, Client client, Message msg) const {
    if (handler == nullptr)
        return;
    switch (event) {
        case EventType::CONNECTED:
            handler->onConnect(client);
            break;
        case EventType::DISCONNECTED:
            handler->onDisconnect(client);
            break;
        case EventType::MESSAGE:
            handler->onMessage(msg);
    }
}

void MPlexServer::Server::broadcast(std::string message) {
    for (const auto& [fd, c] : client_map) {
        sendTo(c, message);
    }
}

void MPlexServer::Server::broadcastExcept(const Client& except, std::string message) {
    for (const auto& [fd, c] : client_map) {
        if (fd != except.getFd()) {
            sendTo(c, message);
        }
    }
}

void MPlexServer::Server::multisend(const std::vector<Client> &clients, std::string message) {
    for (const auto&c : clients) {
        sendTo(c, message);
    }
}
