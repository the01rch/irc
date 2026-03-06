#include "Session.hpp"

Session::Session(int fd) : _sockFd(fd), _passOk(false), _welcomed(false)
{
}

Session::~Session()
{
}

int Session::getSocket() const { return _sockFd; }
std::string Session::getNick() const { return _nick; }
std::string Session::getUser() const { return _user; }
std::string Session::getRecvBuf() const { return _recvBuf; }
bool Session::hasValidPass() const { return _passOk; }
bool Session::isWelcomed() const { return _welcomed; }

void Session::setNick(const std::string& nick) { _nick = nick; }
void Session::setUser(const std::string& user) { _user = user; }
void Session::markPassOk(bool ok) { _passOk = ok; }
void Session::markWelcomed(bool w) { _welcomed = w; }

void Session::feedRecvBuf(const std::string& chunk) { _recvBuf += chunk; }
void Session::resetRecvBuf() { _recvBuf.clear(); }

void Session::pushToOutBuf(const std::string& data) { _outBuf += data; }
const std::string& Session::getOutBuf() const { return _outBuf; }
void Session::drainOutBuf(size_t bytes) { _outBuf.erase(0, bytes); }
bool Session::hasQueuedData() const { return !_outBuf.empty(); }
