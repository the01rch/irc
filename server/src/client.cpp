#include "../include/mplexserver.h"

MPlexServer::Client::Client() {
    this->fd = -1;
    // this->nickname = "";
}

MPlexServer::Client::Client(const int fd, const sockaddr_in addr) : fd(fd), client_addr(addr) {
}

MPlexServer::Client::~Client() {
}

MPlexServer::Client::Client(const Client &other) : fd(other.fd), client_addr(other.client_addr) {
}

MPlexServer::Client & MPlexServer::Client::operator=(const Client &other) {
    this->fd = other.fd;
    this->client_addr = other.client_addr;
    return *this;
}

int MPlexServer::Client::getFd() const {
    return this->fd;
}

std::string MPlexServer::Client::getIpv4() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip);
}

int MPlexServer::Client::getPort() const {
    return ntohs(client_addr.sin_port);
}
