#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>

#include "Channel.h"
#include "User.h"
#include "mplexserver.h"

/**
 * @brief Class to manage channels, users and their capabilities
*/
class SrvMgr : public MPlexServer::EventHandler {
public:
    SrvMgr() = delete;
    SrvMgr(MPlexServer::Server&, const std::string& server_password, const std::string& server_name);
    ~SrvMgr() = default;

    void    onConnect(MPlexServer::Client client) override;
    void    onDisconnect(MPlexServer::Client client) override;
    void    onMessage(MPlexServer::Message msg) override;

    void    process_password(const std::string&, const MPlexServer::Client&, User&) const;
    void    process_cap(const std::string&, const MPlexServer::Client&, User&) const;
    void    process_nick(const std::string&, const MPlexServer::Client&, User&);
    void    process_user(std::string, const MPlexServer::Client&, User&) const;
    void    process_join(std::string, User&);
    void    process_part(std::string, const MPlexServer::Client&, User&);
    void    process_privmsg(std::string, const MPlexServer::Client&, User&);
    void    process_topic(std::string, const MPlexServer::Client&, User&);
    void    process_mode(std::string, const MPlexServer::Client&, User&);
    void    process_invite(std::string, const MPlexServer::Client&, User&);
    void    process_kick(std::string, const MPlexServer::Client&, User&);
    void    process_quit(std::string, const MPlexServer::Client&, User&);
    void    pong(const std::string &, const MPlexServer::Client &, const User&);

private:
    void    try_to_log_in(User& user, const MPlexServer::Client& client) const;

    void    change_nick(const std::string &new_nick, const std::string& old_nick, User& user);
    void    change_nick_in_channel(const std::string &new_nick, const std::string& old_nick, Channel &channel);

    void    remove_user_from_channel(Channel& channel, std::string& nick);
    void    remove_op_from_channel(Channel& channel, std::string& op);
    void    remove_nick_from_channel(Channel& channel, std::string& nick);

    void    mode_i(char plusminus, std::string& mode_arguments, Channel &channel, User& user);
    void    mode_t(char plusminus, std::string& mode_arguments, Channel &channel, User& user);
    void    mode_k(char plusminus, std::string& mode_arguments, Channel &channel, User& user);
    void    mode_o(char plusminus, std::string& mode_arguments, Channel &channel, User& user);
    void    mode_l(char plusminus, std::string& mode_arguments, Channel &channel, User& user);

    void    join_channel(std::string& chan_name, std::string& key, User& user);

    bool    nick_exists(std::string& nick);
    bool    chan_exists(std::string& chan_name);

    // convenience functions, no "\r\n" needed
    void    send_to_one(const std::string& nick, const std::string& msg);
    void    send_to_one(const User& user, const std::string& msg);
    void    send_to_chan_all_but_one(const Channel& channel, const std::string& msg, const std::string& origin_nick) const;
    void    send_to_chan_all_but_one(const std::string& chan_name, const std::string& msg, const std::string& origin_nick) const;
    void    send_to_chan_all(const Channel& channel, const std::string& msg) const;

    void    send_channel_command_ack(Channel&, const User&);
    void    send_channel_greetings(Channel&, const User&);

    std::vector<MPlexServer::Client>    create_client_vector(const std::unordered_set<std::string>& set_of_nicks) const;

    MPlexServer::Server&                        srv_instance_;
    const std::string                           server_password_;
    const std::string                           server_name_;
    std::unordered_map<int, User>               server_users_;
    std::unordered_map<std::string, int>        server_nicks_;
    std::unordered_map<std::string, Channel>    server_channels_;
};


enum    cmdType {
    PASS,
    CAP,
    NICK,
    USER,
    JOIN,
    PART,
    PRIVMSG,
    TOPIC,
    MODE,
    INVITE,
    KICK,
    QUIT,
    PING,
    NO_TYPE_FOUND
};