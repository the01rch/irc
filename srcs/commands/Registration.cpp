#include "IRCCore.hpp"
#include <iostream>
#include <cctype>
#include <sstream>

void IRCCore::cmdPass(Session& sess, const std::string& args)
{
	if (sess.isWelcomed())
	{
		replyNumeric(sess, "462", ":You may not reregister");
		return;
	}
	if (args.empty())
	{
		replyNumeric(sess, "461", "PASS :Not enough parameters");
		return;
	}
	if (args == _secret)
	{
		sess.markPassOk(true);
		std::cout << "[AUTH] FD " << sess.getSocket()
			<< ": Password accepted" << std::endl;
	}
	else
		replyNumeric(sess, "464", ":Password incorrect");
}

void IRCCore::cmdNick(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "431", ":No nickname given");
		return;
	}
	if (!sess.hasValidPass())
	{
		replyNumeric(sess, "464", ":Password incorrect - use PASS first");
		return;
	}

	std::string nick = args;
	size_t sp = nick.find(' ');
	if (sp != std::string::npos)
		nick = nick.substr(0, sp);

	if (!std::isalpha(nick[0]) && nick[0] != '[' && nick[0] != ']'
		&& nick[0] != '\\' && nick[0] != '^' && nick[0] != '_'
		&& nick[0] != '{' && nick[0] != '}' && nick[0] != '|')
	{
		replyNumeric(sess, "432", nick + " :Erroneous nickname");
		return;
	}

	for (size_t i = 1; i < nick.size(); ++i)
	{
		char c = nick[i];
		if (!std::isalnum(c) && c != '[' && c != ']' && c != '\\'
			&& c != '^' && c != '_' && c != '{' && c != '}' && c != '|'
			&& c != '-')
		{
			replyNumeric(sess, "432", nick + " :Erroneous nickname");
			return;
		}
	}

	Session* existing = locateByNick(nick);
	if (existing && existing != &sess)
	{
		replyNumeric(sess, "433", nick + " :Nickname is already in use");
		return;
	}

	std::string prev = sess.getNick();
	sess.setNick(nick);
	std::cout << "[NICK] FD " << sess.getSocket() << ": " << nick << std::endl;

	if (sess.isWelcomed() && !prev.empty())
	{
		std::string note = ":" + prev + "!" + sess.getUser()
			+ "@localhost NICK :" + nick + "\r\n";
		enqueueReply(sess, note);

		for (std::map<std::string, Room*>::iterator it = _rooms.begin();
			it != _rooms.end(); ++it)
		{
			if (it->second->hasUser(&sess))
				it->second->relay(note, &sess);
		}
	}

	tryFinalize(sess);
}

void IRCCore::cmdUser(Session& sess, const std::string& args)
{
	if (sess.isWelcomed())
	{
		replyNumeric(sess, "462", ":You may not reregister");
		return;
	}
	if (args.empty())
	{
		replyNumeric(sess, "461", "USER :Not enough parameters");
		return;
	}
	if (!sess.hasValidPass())
	{
		replyNumeric(sess, "464", ":Password incorrect - use PASS first");
		return;
	}

	std::istringstream iss(args);
    std::string username, mode, unused;
    
    if (!(iss >> username >> mode >> unused))
    {
        replyNumeric(sess, "461", "USER :Not enough parameters");
        return;
    }
    
    sess.setUser(username);
    std::cout << "[USER] FD " << sess.getSocket() << ": " << username << std::endl;

    tryFinalize(sess);
}

void IRCCore::tryFinalize(Session& sess)
{
	if (sess.isWelcomed())
		return;

	if (sess.hasValidPass() && !sess.getNick().empty()
		&& !sess.getUser().empty())
	{
		sess.markWelcomed(true);

		std::string welcome = ":Welcome to the " + _hostname + " Network, "
			+ sess.getNick() + "!" + sess.getUser() + "@localhost";
		replyNumeric(sess, "001", welcome);

		std::cout << "[REGISTERED] " << sess.getNick()
			<< " is now registered" << std::endl;
	}
}

void IRCCore::cmdQuit(Session& sess, const std::string& args)
{
	std::string reason = "Quit";
	if (!args.empty())
	{
		reason = args;
		if (reason[0] == ':')
			reason = reason.substr(1);
	}

	std::string quitLine = buildPrefix(sess) + " QUIT :" + reason + "\r\n";

	for (std::map<std::string, Room*>::iterator it = _rooms.begin();
		it != _rooms.end(); ++it)
	{
		if (it->second->hasUser(&sess))
			it->second->relay(quitLine, &sess);
	}

	// Refresh poll flags so QUIT messages are sent promptly
	for (std::map<int, Session*>::iterator sit = _sessions.begin();
		sit != _sessions.end(); ++sit)
	{
		if (sit->first != sess.getSocket())
			refreshPollFlags(sit->first);
	}

	std::cout << "[QUIT] " << sess.getNick() << ": " << reason << std::endl;

	for (size_t i = 1; i < _watchers.size(); ++i)
	{
		if (_watchers[i].fd == sess.getSocket())
		{
			dropConnection(i);
			return;
		}
	}
}
