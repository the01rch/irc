#include "IRCCore.hpp"
#include <sstream>

static std::vector<std::string> splitComma(const std::string& s)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream stream(s);
	while (std::getline(stream, token, ','))
		if (!token.empty())
			tokens.push_back(token);
	return tokens;
}

void IRCCore::cmdPrivmsg(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "411", ":No recipient given (PRIVMSG)");
		return;
	}

	std::string targetGroup = args;
	std::string body = "";
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		targetGroup = args.substr(0, sp);
		body = args.substr(sp + 1);
		if (!body.empty() && body[0] == ':')
			body = body.substr(1);
	}

	if (body.empty())
	{
		replyNumeric(sess, "412", ":No text to send");
		return;
	}

	std::vector<std::string> targets = splitComma(targetGroup);

	for (size_t i = 0; i < targets.size(); ++i)
	{
		const std::string& target = targets[i];
		std::string fullMsg = buildPrefix(sess)
			+ " PRIVMSG " + target + " :" + body + "\r\n";

		if (target[0] == '#')
		{
			std::map<std::string, Room*>::iterator it = _rooms.find(target);
			if (it == _rooms.end())
			{
				replyNumeric(sess, "403", target + " :No such channel");
				continue;
			}

			Room* room = it->second;

			if (!room->hasUser(&sess))
			{
				replyNumeric(sess, "442", target + " :You're not on that channel");
				continue;
			}

			room->relay(fullMsg, &sess);
		}
		else
		{
			Session* dest = locateByNick(target);
			if (!dest)
			{
				replyNumeric(sess, "401", target + " :No such nick/channel");
				continue;
			}
			enqueueReply(*dest, fullMsg);
		}
	}
}

