#include "IRCCore.hpp"

void IRCCore::cmdPrivmsg(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "411", ":No recipient given (PRIVMSG)");
		return;
	}

	std::string target = args;
	std::string body = "";
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		target = args.substr(0, sp);
		body = args.substr(sp + 1);
		if (!body.empty() && body[0] == ':')
			body = body.substr(1);
	}

	if (body.empty())
	{
		replyNumeric(sess, "412", ":No text to send");
		return;
	}

	std::string fullMsg = buildPrefix(sess)
		+ " PRIVMSG " + target + " :" + body + "\r\n";

	if (target[0] == '#')
	{
		std::map<std::string, Room*>::iterator it = _rooms.find(target);
		if (it == _rooms.end())
		{
			replyNumeric(sess, "403", target + " :No such channel");
			return;
		}

		Room* room = it->second;

		if (!room->hasUser(&sess))
		{
			replyNumeric(sess, "442", target + " :You're not on that channel");
			return;
		}

		room->relay(fullMsg, &sess);
	}
	else
	{
		Session* dest = locateByNick(target);
		if (!dest)
		{
			replyNumeric(sess, "401", target + " :No such nick/channel");
			return;
		}
		enqueueReply(*dest, fullMsg);
	}
}
