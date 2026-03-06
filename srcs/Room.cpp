#include "Room.hpp"
#include "Session.hpp"
#include <algorithm>

Room::Room(const std::string& label)
	: _label(label), _restricted(false), _lockedSubject(false), _maxUsers(0)
{
}

Room::~Room()
{
}

std::string Room::getLabel() const { return _label; }
std::string Room::getSubject() const { return _subject; }
std::string Room::getPassphrase() const { return _passphrase; }
bool Room::isRestricted() const { return _restricted; }
bool Room::hasLockedSubject() const { return _lockedSubject; }
int Room::getMaxUsers() const { return _maxUsers; }
std::vector<Session*>& Room::getUserList() { return _users; }

void Room::changeSubject(const std::string& subject) { _subject = subject; }
void Room::changePassphrase(const std::string& pass) { _passphrase = pass; }
void Room::toggleRestricted(bool on) { _restricted = on; }
void Room::toggleLockedSubject(bool on) { _lockedSubject = on; }
void Room::setMaxUsers(int cap) { _maxUsers = cap; }

void Room::insertUser(Session* s)
{
	if (!hasUser(s))
		_users.push_back(s);
}

void Room::eraseUser(Session* s)
{
	std::vector<Session*>::iterator it = std::find(_users.begin(), _users.end(), s);
	if (it != _users.end())
		_users.erase(it);
	demoteAdmin(s);
	removeGuest(s);
}

bool Room::hasUser(Session* s) const
{
	return std::find(_users.begin(), _users.end(), s) != _users.end();
}

void Room::promoteAdmin(Session* s)
{
	if (!isAdmin(s))
		_admins.push_back(s);
}

void Room::demoteAdmin(Session* s)
{
	std::vector<Session*>::iterator it = std::find(_admins.begin(), _admins.end(), s);
	if (it != _admins.end())
		_admins.erase(it);
}

bool Room::isAdmin(Session* s) const
{
	return std::find(_admins.begin(), _admins.end(), s) != _admins.end();
}

void Room::addGuest(Session* s)
{
	if (!isGuest(s))
		_guestList.push_back(s);
}

bool Room::isGuest(Session* s) const
{
	return std::find(_guestList.begin(), _guestList.end(), s) != _guestList.end();
}

void Room::removeGuest(Session* s)
{
	std::vector<Session*>::iterator it = std::find(_guestList.begin(), _guestList.end(), s);
	if (it != _guestList.end())
		_guestList.erase(it);
}

void Room::relay(const std::string& msg, Session* except)
{
	for (size_t i = 0; i < _users.size(); ++i)
	{
		if (_users[i] != except)
			_users[i]->pushToOutBuf(msg);
	}
}

void Room::relayAll(const std::string& msg)
{
	for (size_t i = 0; i < _users.size(); ++i)
		_users[i]->pushToOutBuf(msg);
}
