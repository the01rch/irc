#include "IRCCore.hpp"
#include "helpers.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <csignal>
#include <iostream>

extern volatile sig_atomic_t g_caught_sig;

IRCCore::IRCCore(int port, const std::string& password)
	: _listenSock(-1), _portNum(port), _secret(password),
	  _hostname("ft_irc"), _active(false)
{
	std::cout << "=== IRC Server Initializing ===" << std::endl;
	std::cout << "Port: " << port << std::endl;
	initSocket();
}

IRCCore::~IRCCore()
{
	shutdown();
}

void IRCCore::initSocket()
{
	_listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenSock < 0)
		fatal("Failed to create socket");

	int opt = 1;
	if (setsockopt(_listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(_listenSock);
		fatal("setsockopt SO_REUSEADDR failed");
	}

	if (!enable_nonblock(_listenSock))
	{
		close(_listenSock);
		fatal("Failed to set listening socket non-blocking");
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_portNum);

	if (bind(_listenSock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		close(_listenSock);
		fatal("Bind failed — port may already be in use");
	}

	if (listen(_listenSock, 10) < 0)
	{
		close(_listenSock);
		fatal("Listen failed");
	}

	struct pollfd pfd;
	pfd.fd = _listenSock;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_watchers.push_back(pfd);

	std::cout << "Server socket created and listening" << std::endl;
	std::cout << "==================================" << std::endl;
}

void IRCCore::onIncomingConnection()
{
	struct sockaddr_in peer;
	socklen_t peerLen = sizeof(peer);

	int fd = accept(_listenSock, (struct sockaddr*)&peer, &peerLen);
	if (fd < 0)
		return;

	if (!enable_nonblock(fd))
	{
		std::cerr << "Failed to set client socket non-blocking" << std::endl;
		close(fd);
		return;
	}

	Session* sess = new Session(fd);
	_sessions[fd] = sess;

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_watchers.push_back(pfd);

	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &peer.sin_addr, ip, INET_ADDRSTRLEN);

	std::cout << "\n[NEW CONNECTION]" << std::endl;
	std::cout << "  FD: " << fd << std::endl;
	std::cout << "  IP: " << ip << std::endl;
	std::cout << "  Total clients: " << _sessions.size() << std::endl;
}

void IRCCore::onDataAvailable(int idx)
{
	char buf[512];
	memset(buf, 0, sizeof(buf));

	int fd = _watchers[idx].fd;

	if (_sessions.find(fd) == _sessions.end())
		return;

	Session* sess = _sessions[fd];

	ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);

	if (n <= 0)
	{
		if (n == 0)
			std::cout << "\n[DISCONNECTION]\n  FD: " << fd << std::endl;
		dropConnection(idx);
		return;
	}

	buf[n] = '\0';

	// Filter out non-printable / control characters (telnet negotiation, binary junk)
	std::string filtered;
	for (ssize_t i = 0; i < n; ++i)
	{
		unsigned char c = static_cast<unsigned char>(buf[i]);
		if (c == '\r' || c == '\n' || (c >= 32 && c <= 126))
			filtered += static_cast<char>(c);
	}

	if (filtered.empty())
		return;

	sess->feedRecvBuf(filtered);

	// Protect against oversized buffers (no \n received for too long)
	if (sess->getRecvBuf().length() > 4096)
	{
		std::cout << "\n[FLOOD] FD " << fd << ": buffer limit exceeded, dropping" << std::endl;
		dropConnection(idx);
		return;
	}

	// Only log if there is visible content (not just \r\n)
	bool hasVisible = false;
	for (size_t i = 0; i < filtered.size(); ++i)
	{
		if (filtered[i] != '\r' && filtered[i] != '\n')
		{
			hasVisible = true;
			break;
		}
	}
	if (hasVisible)
		std::cout << "\n[RECEIVED] FD " << fd << ": " << filtered;

	std::string pending = sess->getRecvBuf();
	size_t pos;

	while ((pos = pending.find('\n')) != std::string::npos)
	{
		std::string line = pending.substr(0, pos);

		if (!line.empty() && line[line.length() - 1] == '\r')
			line = line.substr(0, line.length() - 1);

		pending = pending.substr(pos + 1);

		if (!line.empty())
		{
			dispatch(*sess, line);

			if (_sessions.find(fd) == _sessions.end())
				return;
		}
	}

	sess->resetRecvBuf();
	sess->feedRecvBuf(pending);

	// Update poll flags for all clients that may have queued data
	for (std::map<int, Session*>::iterator sit = _sessions.begin();
		sit != _sessions.end(); ++sit)
	{
		refreshPollFlags(sit->first);
	}
}

void IRCCore::dropConnection(int idx)
{
	int fd = _watchers[idx].fd;
	Session* sess = _sessions[fd];

	purgeFromRooms(sess);
	close(fd);
	delete sess;
	_sessions.erase(fd);
	_watchers.erase(_watchers.begin() + idx);

	std::cout << "  Client removed" << std::endl;
	std::cout << "  Remaining clients: " << _sessions.size() << std::endl;
}

void IRCCore::loop()
{
	_active = true;

	std::cout << "\n=== SERVER STARTED ===" << std::endl;
	std::cout << "Waiting for connections..." << std::endl;
	std::cout << "Press Ctrl+C to stop\n" << std::endl;

	while (_active)
	{
		if (g_caught_sig)
		{
			std::cout << "\n\nReceived interrupt signal" << std::endl;
			break;
		}

		int ready = poll(&_watchers[0], _watchers.size(), -1);

		if (ready < 0)
		{
			if (g_caught_sig)
				break;
			std::cerr << "Poll error" << std::endl;
			break;
		}

		for (size_t i = 0; i < _watchers.size(); ++i)
		{
			if (_watchers[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (_watchers[i].fd != _listenSock)
				{
					size_t before = _watchers.size();
					dropConnection(i);
					if (_watchers.size() < before)
						--i;
					continue;
				}
			}

			if (_watchers[i].revents & POLLIN)
			{
				if (_watchers[i].fd == _listenSock)
				{
					onIncomingConnection();
				}
				else
				{
					size_t before = _watchers.size();
					onDataAvailable(i);
					if (_watchers.size() < before)
						--i;
				}
			}

			if (i < _watchers.size() && (_watchers[i].revents & POLLOUT))
			{
				size_t before = _watchers.size();
				onReadyToSend(i);
				if (_watchers.size() < before)
					--i;
			}
		}
	}
	std::cout << "\n=== SERVER STOPPED ===" << std::endl;
}

void IRCCore::shutdown()
{
	_active = false;

	std::cout << "\nClosing all connections..." << std::endl;

	for (std::map<int, Session*>::iterator it = _sessions.begin();
		it != _sessions.end(); ++it)
	{
		close(it->second->getSocket());
		delete it->second;
	}
	_sessions.clear();

	for (std::map<std::string, Room*>::iterator it = _rooms.begin();
		it != _rooms.end(); ++it)
		delete it->second;
	_rooms.clear();

	if (_listenSock >= 0)
	{
		close(_listenSock);
		_listenSock = -1;
	}

	_watchers.clear();

	std::cout << "Server stopped cleanly." << std::endl;
}
