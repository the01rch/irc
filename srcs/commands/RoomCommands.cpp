#include "IRCCore.hpp"
#include <iostream>
#include <sstream>

void IRCCore::joinOneChannel(Session& sess, const std::string& roomLabel,
								const std::string& passphrase)
{
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

void IRCCore::cmdJoin(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "461", "JOIN :Not enough parameters");
		return;
	}

	std::istringstream iss(args);
	std::string chanGroup, keyGroup;
	iss >> chanGroup >> keyGroup;

	std::vector<std::string> chans = splitComma(chanGroup);
	std::vector<std::string> keys = splitComma(keyGroup);

	for (size_t i = 0; i < chans.size(); ++i)
	{
		std::string key = (i < keys.size()) ? keys[i] : "";
		joinOneChannel(sess, chans[i], key);
	}
}

void IRCCore::cmdPart(Session& sess, const std::string& args)
{
	if (args.empty())
	{
		replyNumeric(sess, "461", "PART :Not enough parameters");
		return;
	}

	std::string chanGroup = args;
	std::string reason = "";
	size_t sp = args.find(' ');
	if (sp != std::string::npos)
	{
		chanGroup = args.substr(0, sp);
		reason = args.substr(sp + 1);
		if (!reason.empty() && reason[0] == ':')
			reason = reason.substr(1);
	}

	std::vector<std::string> chans = splitComma(chanGroup);

	for (size_t i = 0; i < chans.size(); ++i)
	{
		Room* room = requireRoom(sess, chans[i], false);
		if (!room)
			continue;

		std::string partLine = buildPrefix(sess) + " PART " + chans[i];
		if (!reason.empty())
			partLine += " :" + reason;
		partLine += "\r\n";

		room->relayAll(partLine);
		room->eraseUser(&sess);

		if (room->getUserList().empty())
		{
			_rooms.erase(chans[i]);
			delete room;
			std::cout << "[CHANNEL] Deleted: " << chans[i] << std::endl;
		}

		std::cout << "[PART] " << sess.getNick() << " left "
			<< chans[i] << std::endl;
	}
}

void IRCCore::cmdNames(Session& sess, const std::string& args)
{
    std::string roomLabel = args;
    
    if (!roomLabel.empty() && roomLabel[0] == ':')
        roomLabel = roomLabel.substr(1);

    if (roomLabel.empty())
    {
        replyNumeric(sess, "461", "NAMES :Not enough parameters");
        return;
    }

    Room* room = requireRoom(sess, roomLabel, false);
    if (!room)
        return;

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
}

void IRCCore::cmdList(Session& sess, const std::string& args)
{
    (void)args;

    replyNumeric(sess, "321", "Channel :Users  Name");

    for (std::map<std::string, Room*>::iterator it = _rooms.begin();
        it != _rooms.end(); ++it)
    {
        Room* room = it->second;
        
        std::ostringstream oss;
        oss << room->getUserList().size();
        
        std::string info = it->first + " " + oss.str();
        
        if (!room->getSubject().empty())
            info += " :" + room->getSubject();
        else
            info += " :";

        replyNumeric(sess, "322", info);
    }

    replyNumeric(sess, "323", ":End of /LIST");
}