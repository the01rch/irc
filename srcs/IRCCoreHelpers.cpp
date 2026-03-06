#include "IRCCore.hpp"
#include <sys/socket.h>
#include <iostream>
#include <cctype>

void IRCCore::enqueueReply(Session& sess, const std::string& message)
{
	std::string msg = message;
	if (msg.length() < 2 || msg.substr(msg.length() - 2) != "\r\n")
		msg += "\r\n";
	sess.pushToOutBuf(msg);
	refreshPollFlags(sess.getSocket());
}

void IRCCore::replyNumeric(Session& sess, const std::string& code,
	const std::string& body)
{
	std::string nick = sess.getNick().empty() ? "*" : sess.getNick();
	std::string reply = ":" + _hostname + " " + code + " " + nick + " " + body;
	enqueueReply(sess, reply);
}

std::string IRCCore::buildPrefix(Session& sess)
{
	return ":" + sess.getNick() + "!" + sess.getUser() + "@localhost";
}

std::string IRCCore::parseVerb(const std::string& raw)
{
	size_t start = raw.find_first_not_of(" \t\r");
	if (start == std::string::npos)
		return "";

	std::string trimmed = raw.substr(start);

	size_t space = trimmed.find_first_of(" \t");
	std::string verb = (space != std::string::npos)
		? trimmed.substr(0, space) : trimmed;

	for (size_t i = 0; i < verb.size(); ++i)
		verb[i] = std::toupper(verb[i]);

	return verb;
}

std::string IRCCore::parseArgs(const std::string& raw)
{
	size_t start = raw.find_first_not_of(" \t\r");
	if (start == std::string::npos)
		return "";

	std::string trimmed = raw.substr(start);

	size_t space = trimmed.find_first_of(" \t");
	if (space == std::string::npos)
		return "";

	std::string args = trimmed.substr(space + 1);
	size_t begin = args.find_first_not_of(" \t");
	if (begin == std::string::npos)
		return "";

	return args.substr(begin);
}

Session* IRCCore::locateByNick(const std::string& nick)
{
	for (std::map<int, Session*>::iterator it = _sessions.begin();
		it != _sessions.end(); ++it)
	{
		if (it->second->getNick() == nick)
			return it->second;
	}
	return NULL;
}

Room* IRCCore::requireRoom(Session& sess, const std::string& label, bool needOp)
{
	std::map<std::string, Room*>::iterator it = _rooms.find(label);
	if (it == _rooms.end())
	{
		replyNumeric(sess, "403", label + " :No such channel");
		return NULL;
	}
	Room* room = it->second;
	if (!room->hasUser(&sess))
	{
		replyNumeric(sess, "442", label + " :You're not on that channel");
		return NULL;
	}
	if (needOp && !room->isAdmin(&sess))
	{
		replyNumeric(sess, "482", label + " :You're not channel operator");
		return NULL;
	}
	return room;
}

void IRCCore::purgeFromRooms(Session* sess)
{
	for (std::map<std::string, Room*>::iterator it = _rooms.begin();
		it != _rooms.end(); )
	{
		std::string quitLine = buildPrefix(*sess) + " QUIT :Connection closed\r\n";

		if (it->second->hasUser(sess))
		{
			it->second->relay(quitLine, sess);
			flushRoomBuffers(it->second, sess);
		}

		it->second->eraseUser(sess);

		if (it->second->getUserList().empty())
		{
			delete it->second;
			_rooms.erase(it++);
		}
		else
			++it;
	}
}

void IRCCore::refreshPollFlags(int fd)
{
	for (size_t i = 0; i < _watchers.size(); ++i)
	{
		if (_watchers[i].fd == fd)
		{
			if (_sessions.find(fd) != _sessions.end()
				&& _sessions[fd]->hasQueuedData())
				_watchers[i].events = POLLIN | POLLOUT;
			else
				_watchers[i].events = POLLIN;
			return;
		}
	}
}

void IRCCore::onReadyToSend(int idx)
{
	int fd = _watchers[idx].fd;

	if (_sessions.find(fd) == _sessions.end())
		return;

	Session* sess = _sessions[fd];
	const std::string& buf = sess->getOutBuf();

	if (buf.empty())
	{
		_watchers[idx].events = POLLIN;
		return;
	}

	ssize_t n = send(fd, buf.c_str(), buf.length(), 0);
	if (n > 0)
	{
		sess->drainOutBuf(n);
		if (!sess->hasQueuedData())
			_watchers[idx].events = POLLIN;
	}
	else if (n < 0)
	{
		dropConnection(idx);
	}
}

void IRCCore::flushRoomBuffers(Room* room, Session* except)
{
	std::vector<Session*>& users = room->getUserList();
	for (size_t i = 0; i < users.size(); ++i)
	{
		if (users[i] != except)
			refreshPollFlags(users[i]->getSocket());
	}
}

void IRCCore::cmdPing(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "409", ":No origin specified");
		return;
	}
	enqueueReply(sess, ":" + _hostname + " PONG " + _hostname + " :" + args);
}
