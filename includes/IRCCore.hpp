#ifndef IRCCORE_HPP
#define IRCCORE_HPP

#include <map>
#include <vector>
#include <string>
#include <poll.h>
#include "Session.hpp"
#include "Room.hpp"

class IRCCore {
    private:
        int                             _listenSock;
        int                             _portNum;
        std::string                     _secret;
        std::string                     _hostname;
        std::map<int, Session*>         _sessions;
        std::map<std::string, Room*>    _rooms;
        std::vector<struct pollfd>      _watchers;
        bool                            _active;

        // Non-copyable
        IRCCore(const IRCCore&);
        IRCCore& operator=(const IRCCore&);

        // Socket setup
        void initSocket();

        // Connection lifecycle
        void onIncomingConnection();
        void onDataAvailable(int idx);
        void onReadyToSend(int idx);
        void dropConnection(int idx);

        // Output helpers
        void enqueueReply(Session& sess, const std::string& data);
        void replyNumeric(Session& sess, const std::string& code, const std::string& body);
        std::string buildPrefix(Session& sess);
        void refreshPollFlags(int fd);
        void flushRoomBuffers(Room* room, Session* except);

        // Parsing
        std::string parseVerb(const std::string& raw);
        std::string parseArgs(const std::string& raw);

        // Lookup
        Session* locateByNick(const std::string& nick);
        void purgeFromRooms(Session* sess);
        Room* requireRoom(Session& sess, const std::string& label, bool needOp);

        // Command dispatcher
        void dispatch(Session& sess, const std::string& line);

        // Registration commands
        void cmdPass(Session& sess, const std::string& args);
        void cmdNick(Session& sess, const std::string& args);
        void cmdUser(Session& sess, const std::string& args);
        void tryFinalize(Session& sess);
        void cmdQuit(Session& sess, const std::string& args);

        // Room commands
        void joinOneChannel(Session& sess, const std::string& roomLabel,
                           const std::string& passphrase);
        void cmdJoin(Session& sess, const std::string& args);
        void cmdPart(Session& sess, const std::string& args);
        void cmdNames(Session& sess, const std::string& args);
        void cmdList(Session& sess, const std::string& args);

        // Messaging
        void cmdPrivmsg(Session& sess, const std::string& args);

        // Operator commands
        void cmdKick(Session& sess, const std::string& args);
        void cmdInvite(Session& sess, const std::string& args);
        void cmdTopic(Session& sess, const std::string& args);
        void cmdMode(Session& sess, const std::string& args);
        void showChannelModes(Session& sess, const std::string& target);
        void applyChannelModes(Session& sess, const std::string& target,
                              const std::string& modeStr,
                              const std::vector<std::string>& modeArgs);

        // Utility
        void cmdPing(Session& sess, const std::string& args);

    public:
        IRCCore(int port, const std::string& password);
        ~IRCCore();
        void loop();
        void shutdown();
};

#endif
