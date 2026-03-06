#ifndef ROOM_HPP
#define ROOM_HPP

#include <string>
#include <vector>

class Session;

class Room {
    private:
        std::string             _label;
        std::string             _subject;
        std::string             _passphrase;
        std::vector<Session*>   _users;
        std::vector<Session*>   _admins;
        std::vector<Session*>   _guestList;
        bool                    _restricted;
        bool                    _lockedSubject;
        int                     _maxUsers;

        // Non-copyable
        Room(const Room&);
        Room& operator=(const Room&);

    public:
        Room(const std::string& label);
        ~Room();

        // Accessors
        std::string getLabel() const;
        std::string getSubject() const;
        std::string getPassphrase() const;
        bool isRestricted() const;
        bool hasLockedSubject() const;
        int getMaxUsers() const;
        std::vector<Session*>& getUserList();

        // Mutators
        void changeSubject(const std::string& subject);
        void changePassphrase(const std::string& pass);
        void toggleRestricted(bool on);
        void toggleLockedSubject(bool on);
        void setMaxUsers(int cap);

        // User management
        void insertUser(Session* s);
        void eraseUser(Session* s);
        bool hasUser(Session* s) const;

        // Admin management
        void promoteAdmin(Session* s);
        void demoteAdmin(Session* s);
        bool isAdmin(Session* s) const;

        // Guest list
        void addGuest(Session* s);
        bool isGuest(Session* s) const;
        void removeGuest(Session* s);

        // Messaging
        void relay(const std::string& msg, Session* except);
        void relayAll(const std::string& msg);
};

#endif
