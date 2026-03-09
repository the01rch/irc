#include "IRCCore.hpp"
#include <iostream>

void IRCCore::dispatch(Session& sess, const std::string& line)
{
	std::cout << "[COMMAND] FD " << sess.getSocket() << ": " << line << std::endl;

	std::string verb = parseVerb(line);
	std::string args = parseArgs(line);

	if (verb == "PASS")
		return cmdPass(sess, args);
	if (verb == "NICK")
		return cmdNick(sess, args);
	if (verb == "USER")
		return cmdUser(sess, args);
	if (verb == "QUIT")
		return cmdQuit(sess, args);

	if (!sess.isWelcomed())
	{
		replyNumeric(sess, "451", ":You have not registered");
		return;
	}

	if (verb == "JOIN")
		cmdJoin(sess, args);
	else if (verb == "PART")
		cmdPart(sess, args);
	else if (verb == "PRIVMSG")
		cmdPrivmsg(sess, args);
	else if (verb == "KICK")
		cmdKick(sess, args);
	else if (verb == "INVITE")
		cmdInvite(sess, args);
	else if (verb == "TOPIC")
		cmdTopic(sess, args);
	else if (verb == "MODE")
		cmdMode(sess, args);
	else if (verb == "PING")
		cmdPing(sess, args);
	else if (verb == "NAMES")
		cmdNames(sess, args);
	else if (verb == "LIST")
		cmdList(sess, args);
	else if (verb == "USERHOST" || verb == "WHO" || verb == "WHOIS")
        ;
	else
		replyNumeric(sess, "421", verb + " :Unknown command");
}
