#include "IRCCore.hpp"
#include <iostream>
#include <cstdlib>
#include <sstream>

void IRCCore::cmdKick(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "461", "KICK :Not enough parameters");
		return;
	}

	std::string rest = args;
	std::string chanGroup = "";
	size_t sp = rest.find(' ');
	if (sp != std::string::npos)
	{
		chanGroup = rest.substr(0, sp);
		rest = rest.substr(sp + 1);
	}
	else
	{
		replyNumeric(sess, "461", "KICK :Not enough parameters");
		return;
	}

	std::string nickGroup = rest;
	std::string reason = sess.getNick();
	sp = rest.find(' ');
	if (sp != std::string::npos)
	{
		nickGroup = rest.substr(0, sp);
		reason = rest.substr(sp + 1);
		if (!reason.empty() && reason[0] == ':')
			reason = reason.substr(1);
	}

	std::vector<std::string> chans = splitComma(chanGroup);
	std::vector<std::string> nicks = splitComma(nickGroup);

	for (size_t i = 0; i < chans.size(); ++i)
	{
		Room* room = requireRoom(sess, chans[i], true);
		if (!room)
			continue;

		for (size_t j = 0; j < nicks.size(); ++j)
		{
			Session* target = locateByNick(nicks[j]);
			if (!target)
			{
				replyNumeric(sess, "401", nicks[j] + " :No such nick/channel");
				continue;
			}
			if (!room->hasUser(target))
			{
				replyNumeric(sess, "441",
					nicks[j] + " " + chans[i] + " :They aren't on that channel");
				continue;
			}

			std::string kickLine = buildPrefix(sess) + " KICK " + chans[i]
				+ " " + nicks[j] + " :" + reason + "\r\n";
			room->relayAll(kickLine);

			room->eraseUser(target);

			if (room->getUserList().empty())
			{
				_rooms.erase(chans[i]);
				delete room;
				room = NULL;
				std::cout << "[CHANNEL] Deleted: " << chans[i] << std::endl;
				break;
			}

			std::cout << "[KICK] " << sess.getNick() << " kicked "
				<< nicks[j] << " from " << chans[i] << std::endl;
		}
	}
}

void IRCCore::cmdInvite(Session& sess, const std::string& args)
{
	std::string nick = args;
	std::string roomLabel = "";
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		nick = args.substr(0, sp);
		roomLabel = args.substr(sp + 1);
	}

	if (nick.empty() || roomLabel.empty())
	{
		replyNumeric(sess, "461", "INVITE :Not enough parameters");
		return;
	}

	Room* room = requireRoom(sess, roomLabel, true);
	if (!room)
		return;

	Session* dest = locateByNick(nick);
	if (!dest)
	{
		replyNumeric(sess, "401", nick + " :No such nick/channel");
		return;
	}
	if (room->hasUser(dest))
	{
		replyNumeric(sess, "443",
			nick + " " + roomLabel + " :is already on channel");
		return;
	}

	room->addGuest(dest);

	replyNumeric(sess, "341", nick + " " + roomLabel);

	std::string invLine = buildPrefix(sess) + " INVITE "
		+ nick + " " + roomLabel + "\r\n";
	enqueueReply(*dest, invLine);

	std::cout << "[INVITE] " << sess.getNick() << " invited "
		<< nick << " to " << roomLabel << std::endl;
}

void IRCCore::cmdTopic(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "461", "TOPIC :Not enough parameters");
		return;
	}

	std::string roomLabel = args;
	std::string newSubject = "";
	bool hasSubject = false;
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		roomLabel = args.substr(0, sp);
		newSubject = args.substr(sp + 1);
		hasSubject = true;
		if (!newSubject.empty() && newSubject[0] == ':')
			newSubject = newSubject.substr(1);
	}

	Room* room = requireRoom(sess, roomLabel, false);
	if (!room)
		return;

	if (!hasSubject)
	{
		if (room->getSubject().empty())
			replyNumeric(sess, "331", roomLabel + " :No topic is set");
		else
			replyNumeric(sess, "332",
				roomLabel + " :" + room->getSubject());
	}
	else
	{
		if (room->hasLockedSubject() && !room->isAdmin(&sess))
		{
			replyNumeric(sess, "482",
				roomLabel + " :You're not channel operator");
			return;
		}

		room->changeSubject(newSubject);

		std::string topicLine = buildPrefix(sess) + " TOPIC "
			+ roomLabel + " :" + newSubject + "\r\n";
		room->relayAll(topicLine);

		std::cout << "[TOPIC] " << sess.getNick() << " set topic of "
			<< roomLabel << " to: " << newSubject << std::endl;
	}
}

void IRCCore::showChannelModes(Session& sess, const std::string& target)
{
	Room* room = requireRoom(sess, target, false);
	if (!room)
		return;

	std::string flags = "+";
	std::string extra = "";

	if (room->isRestricted())
		flags += "i";
	if (room->hasLockedSubject())
		flags += "t";
	if (!room->getPassphrase().empty())
	{
		flags += "k";
		extra += " " + room->getPassphrase();
	}
	if (room->getMaxUsers() > 0)
	{
		flags += "l";
		std::ostringstream oss;
		oss << room->getMaxUsers();
		extra += " " + oss.str();
	}

	replyNumeric(sess, "324", target + " " + flags + extra);
}

void IRCCore::applyChannelModes(Session& sess, const std::string& target, 
	const std::string& modeStr, 
	const std::vector<std::string>& modeArgs)
{
	Room* room = requireRoom(sess, target, true);
	if (!room)
		return;

	bool adding = true;
	size_t argIdx = 0;
	std::string applied = "";
	std::string appliedArgs = "";
	bool anyValid = false;

	for (size_t i = 0; i < modeStr.size(); ++i)
	{
		char c = modeStr[i];

		if (c == '+')
		{
			adding = true;
			if (applied.empty()
				|| applied[applied.size() - 1] != '+')
				applied += '+';
			continue;
		}
		else if (c == '-')
		{
			adding = false;
			if (applied.empty()
				|| applied[applied.size() - 1] != '-')
				applied += '-';
			continue;
		}

		switch (c)
		{
			case 'i':
				room->toggleRestricted(adding);
				applied += 'i';
				anyValid = true;
				break;

			case 't':
				room->toggleLockedSubject(adding);
				applied += 't';
				anyValid = true;
				break;

			case 'k':
			{
				if (adding)
				{
					if (argIdx >= modeArgs.size())
					{
						replyNumeric(sess, "461",
							"MODE +k :Not enough parameters");
						continue;
					}
					std::string key = modeArgs[argIdx++];
					room->changePassphrase(key);
					applied += 'k';
					appliedArgs += " " + key;
				}
				else
				{
					room->changePassphrase("");
					applied += 'k';
				}
				anyValid = true;
				break;
			}

			case 'o':
			{
				if (argIdx >= modeArgs.size())
				{
					replyNumeric(sess, "461",
						"MODE +o :Not enough parameters");
					continue;
				}

				std::string who = modeArgs[argIdx++];
				Session* tgt = locateByNick(who);

				if (!tgt)
				{
					replyNumeric(sess, "401",
						who + " :No such nick/channel");
					continue;
				}
				if (!room->hasUser(tgt))
				{
					replyNumeric(sess, "441",
						who + " " + target
						+ " :They aren't on that channel");
					continue;
				}

				if (adding)
					room->promoteAdmin(tgt);
				else
					room->demoteAdmin(tgt);

				applied += 'o';
				appliedArgs += " " + who;
				anyValid = true;
				break;
			}

			case 'l':
			{
				if (adding)
				{
					if (argIdx >= modeArgs.size())
					{
						replyNumeric(sess, "461",
							"MODE +l :Not enough parameters");
						continue;
					}

					std::string limStr = modeArgs[argIdx++];
					int lim = std::atoi(limStr.c_str());

					if (lim <= 0)
					{
						replyNumeric(sess, "461",
							"MODE +l :Invalid user limit");
						continue;
					}

					room->setMaxUsers(lim);
					applied += 'l';
					appliedArgs += " " + limStr;
				}
				else
				{
					room->setMaxUsers(0);
					applied += 'l';
				}
				anyValid = true;
				break;
			}

			default:
				replyNumeric(sess, "472",
					std::string(1, c) + " :is unknown mode char to me");
				continue;
			}
	}

	if (anyValid && !applied.empty())
	{
		std::string modeLine = buildPrefix(sess) + " MODE " + target
			+ " " + applied + appliedArgs + "\r\n";
		room->relayAll(modeLine);
		std::cout << "[MODE] " << sess.getNick() << " set mode "
			<< applied << appliedArgs << " on " << target << std::endl;
	}
}

void IRCCore::cmdMode(Session& sess, const std::string& args)
{
	std::istringstream iss(args);
	std::string target;
	std::string modeStr;

	if (!(iss >> target))
	{
		replyNumeric(sess, "461", "MODE :Not enough parameters");
		return;
	}

	if (target[0] != '#')
	{
		return;
	}

	if (!(iss >> modeStr))
	{
		showChannelModes(sess, target);
		return;
	}

	std::vector<std::string> modeArgs;
	std::string tok;
	while (iss >> tok)
		modeArgs.push_back(tok);

	applyChannelModes(sess, target, modeStr, modeArgs);
}
