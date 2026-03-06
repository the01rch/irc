#include "IRCCore.hpp"
#include <iostream>

void IRCCore::cmdJoin(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "461", "JOIN :Not enough parameters");
		return;
	}

	std::string roomLabel = args;
	std::string passphrase = "";
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		roomLabel = args.substr(0, sp);
		passphrase = args.substr(sp + 1);
	}

	if (roomLabel.empty() || roomLabel[0] != '#')
	{
		replyNumeric(sess, "476", roomLabel + " :Bad Channel Mask");
		return;
	}

	Room* room = NULL;
	std::map<std::string, Room*>::iterator it = _rooms.find(roomLabel);

	if (it != _rooms.end())
	{
		room = it->second;

		if (room->hasUser(&sess))
			return;

		if (room->isRestricted() && !room->isGuest(&sess))
		{
			replyNumeric(sess, "473", roomLabel + " :Cannot join channel (+i)");
			return;
		}

		if (!room->getPassphrase().empty()
			&& passphrase != room->getPassphrase())
		{
			replyNumeric(sess, "475", roomLabel + " :Cannot join channel (+k)");
			return;
		}

		if (room->getMaxUsers() > 0
			&& (int)room->getUserList().size() >= room->getMaxUsers())
		{
			replyNumeric(sess, "471", roomLabel + " :Cannot join channel (+l)");
			return;
		}
	}
	else
	{
		room = new Room(roomLabel);
		_rooms[roomLabel] = room;
		room->promoteAdmin(&sess);
		std::cout << "[CHANNEL] Created: " << roomLabel << std::endl;
	}

	room->insertUser(&sess);
	room->removeGuest(&sess);

	std::string joinLine = buildPrefix(sess) + " JOIN " + roomLabel + "\r\n";
	room->relayAll(joinLine);

	if (!room->getSubject().empty())
		replyNumeric(sess, "332", roomLabel + " :" + room->getSubject());

	std::string namesList = "";
	std::vector<Session*>& users = room->getUserList();
	for (size_t i = 0; i < users.size(); ++i)
	{
		if (i > 0)
			namesList += " ";
		if (room->isAdmin(users[i]))
			namesList += "@";
		namesList += users[i]->getNick();
	}

	replyNumeric(sess, "353", "= " + roomLabel + " :" + namesList);
	replyNumeric(sess, "366", roomLabel + " :End of /NAMES list");

	std::cout << "[JOIN] " << sess.getNick() << " joined "
		<< roomLabel << std::endl;
}

void IRCCore::cmdPart(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "461", "PART :Not enough parameters");
		return;
	}

	std::string roomLabel = args;
	std::string reason = "";
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		roomLabel = args.substr(0, sp);
		reason = args.substr(sp + 1);
		if (!reason.empty() && reason[0] == ':')
			reason = reason.substr(1);
	}

	std::map<std::string, Room*>::iterator it = _rooms.find(roomLabel);
	if (it == _rooms.end())
	{
		replyNumeric(sess, "403", roomLabel + " :No such channel");
		return;
	}

	Room* room = it->second;

	if (!room->hasUser(&sess))
	{
		replyNumeric(sess, "442", roomLabel + " :You're not on that channel");
		return;
	}

	std::string partLine = buildPrefix(sess) + " PART " + roomLabel;
	if (!reason.empty())
		partLine += " :" + reason;
	partLine += "\r\n";

	room->relayAll(partLine);
	room->eraseUser(&sess);

	if (room->getUserList().empty())
	{
		delete room;
		_rooms.erase(it);
		std::cout << "[CHANNEL] Deleted: " << roomLabel << std::endl;
	}

	std::cout << "[PART] " << sess.getNick() << " left "
		<< roomLabel << std::endl;
}
