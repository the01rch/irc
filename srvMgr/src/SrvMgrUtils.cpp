#include <vector>

#include "mplexserver.h"
#include "Channel.h"
#include "IRC_macros.h"
#include "SrvMgr.h"
#include "User.h"
#include "utils.h"

using std::cout;
using std::endl;
using std::string;

void    SrvMgr::try_to_log_in(User &user, const MPlexServer::Client &client) const {
    cout << "[LOGIN] Checking login: nick='" << user.get_nickname() << "', user='" << user.get_username() 
         << "', pass=" << user.password_provided() << ", cap_started=" << user.cap_negotiation_started()
         << ", cap_ended=" << user.cap_negotiation_ended() << endl;
    
    if (user.get_nickname().empty() || user.get_username().empty() ||
        !user.password_provided() || 
        (user.cap_negotiation_started() && !user.cap_negotiation_ended())) {
        cout << "[LOGIN] Requirements not met, login blocked" << endl;
        return ;
        }
    cout << "[LOGIN] ✓ All requirements met, logging in user '" << user.get_nickname() << "'" << endl;
    user.set_as_logged_in(true);
    const string nick = user.get_nickname();
    srv_instance_.sendTo(client, ":" + server_name_ + " " + RPL_WELCOME + " " + nick + " :Welcome to our single-server IRC network, " + user.get_signature() + "\r\n");
    srv_instance_.sendTo(client, ":" + server_name_ + " " + RPL_YOURHOST + " " + nick + " :Your host is " + server_name_ + ", running version 1.0.\r\n");
    srv_instance_.sendTo(client, ":" + server_name_ + " " + RPL_CREATED + " " + nick + " :This server was created today.\r\n");
    srv_instance_.sendTo(client, ":" + server_name_ + " " + RPL_MYINFO + " " + nick + " :server 1.0 o o\r\n");
}

void SrvMgr::mode_i(char plusminus, std::string &mode_arguments, Channel &channel, User &user) {
    (void)  mode_arguments;
    if (plusminus == '-') {
        channel.set_needs_invite(false);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " -i";
        send_to_chan_all(channel, msg);
    } else if (plusminus == '+') {
        channel.set_needs_invite(true);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " +i";
        send_to_chan_all(channel, msg);
    }
}
void SrvMgr::mode_t(char plusminus, std::string &mode_arguments, Channel &channel, User &user) {
    (void)  mode_arguments;
    if (plusminus == '-') {
        channel.set_topic_protected(false);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " -t";
        send_to_chan_all(channel, msg);
    } else if (plusminus == '+') {
        channel.set_topic_protected(true);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " +t";
        send_to_chan_all(channel, msg);
    }
}
void SrvMgr::mode_k(char plusminus, std::string &mode_arguments, Channel &channel, User &user) {
    std::string key = split_off_before_del(mode_arguments,' ');
    if (key.empty()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " MODE :Not enough parameters";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    if (plusminus == '-') {
        channel.set_key("");
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " -k *";
        send_to_chan_all(channel, msg);
    } else if (plusminus == '+') {
        channel.set_key(key);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " +k " + key;
        send_to_chan_all(channel, msg);
    }
}
void SrvMgr::mode_o(char plusminus, std::string &mode_arguments, Channel &channel, User &user) {
    std::string target_nick = split_off_before_del(mode_arguments,' ');
    if (target_nick.empty()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " MODE :Not enough parameters";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    if (!channel.has_chan_member(target_nick)) {
        string  err_msg = ":" + server_name_ + " " + ERR_NOSUCHNICK + " " + user.get_nickname() + " MODE :No such nick";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    if (plusminus == '-') {
        channel.remove_operator(target_nick);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " -o " + target_nick;
        send_to_chan_all(channel, msg);
    } else if (plusminus == '+') {
        channel.add_operator(target_nick);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " +o " + target_nick;
        send_to_chan_all(channel, msg);
    }
}
void SrvMgr::mode_l(char plusminus, std::string &mode_arguments, Channel &channel, User &user) {
    if (plusminus == '-') {
        channel.set_member_limit(0);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " -l ";
        send_to_chan_all(channel, msg);
    } else if (plusminus == '+') {
        std::string limit_str = split_off_before_del(mode_arguments,' ');
        if (limit_str.empty()) {
            string  err_msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " MODE :Not enough parameters";
            send_to_one(user.get_nickname(), err_msg);
            return ;
        }
        int         limit = atoi(limit_str.c_str());
        channel.set_member_limit(limit);
        std::string msg = ":" + user.get_signature() + " MODE " + channel.get_channel_name() + " +l " + limit_str;
        send_to_chan_all(channel, msg);
    }
}

void    SrvMgr::join_channel(string& chan_name, string& key, User& user) {
    // Channel name must start with # or &
    if (chan_name.empty() || (chan_name[0] != '#' && chan_name[0] != '&')) {
        string  err_msg = ":" + server_name_ + " " + ERR_BADCHANMASK + " " + user.get_nickname() + " " + chan_name + " :Bad Channel Mask. Names must start with '#' or '&'";
        send_to_one(user, err_msg);
        return ;
    }
    if (server_channels_.find(chan_name) == server_channels_.end()) {
        server_channels_.emplace(chan_name, Channel(chan_name, user.get_nickname()));
    }
    Channel& channel = server_channels_[chan_name];
    if (!channel.does_key_fit(key)) {
        string  err_msg = ":" + server_name_ + " " + ERR_BADCHANNELKEY + " " + user.get_nickname() + " " + chan_name + " :Cannot join channel (+k)";
        send_to_one(user, err_msg);
        return ;
    }
    if (channel.get_member_count() >= channel.get_member_limit() && !channel.get_member_limit() == 0) {
        string  err_msg = ":" + server_name_ + " " + ERR_CHANNELISFULL + " " + user.get_nickname() + " " + chan_name + " :Cannot join channel (+l)";
        send_to_one(user, err_msg);
        return ;
    }
    if (channel.needs_invite()) {
        if (!user.has_invitation(chan_name)){
            string  err_msg = ":" + server_name_ + " " + ERR_INVITEONLYCHAN + " " + user.get_nickname() + " " + chan_name + " :Cannot join channel (+i)";
            send_to_one(user, err_msg);
            return ;
        } else {
            user.remove_invitation(chan_name);
        }
    }
    channel.add_nick(user.get_nickname());
    send_channel_command_ack(channel, user);
    send_channel_greetings(channel, user);
}

void    SrvMgr::send_to_one(const User& user, const std::string& msg) {
    srv_instance_.sendTo(user.get_client(), msg + "\r\n");
}
void    SrvMgr::send_to_one(const string& nick, const std::string& msg) {
    auto    nick_it = server_nicks_.find(nick);
    if (nick_it == server_nicks_.end()) {
        return ;
    }
    auto    user_it = server_users_.find(nick_it->second);
    if (user_it == server_users_.end()) {
        return ;
    }
    send_to_one(user_it->second, msg);
}
void    SrvMgr::send_to_chan_all(const Channel& channel, const std::string& msg) const {
    auto set_of_nicks = channel.get_chan_nicks();
    std::vector<MPlexServer::Client> clients = create_client_vector(set_of_nicks);
    srv_instance_.multisend(clients, msg + "\r\n");
}
void    SrvMgr::send_to_chan_all_but_one(const Channel& channel, const std::string& msg, const std::string& origin_nick) const {
    auto set_of_nicks = channel.get_chan_nicks();
    set_of_nicks.erase(origin_nick);
    std::vector<MPlexServer::Client> clients = create_client_vector(set_of_nicks);
    srv_instance_.multisend(clients, msg + "\r\n");
}
void    SrvMgr::send_to_chan_all_but_one(const std::string& chan_name, const std::string& msg, const std::string& origin_nick) const {
    auto    chan_it = server_channels_.find(chan_name);
    if (chan_it == server_channels_.end()) {
        return ;
    }
    const Channel& channel = chan_it->second;
    send_to_chan_all_but_one(channel, msg, origin_nick);
}

void    SrvMgr::change_nick(const string &new_nick, const std::string& old_nick, User& user) {
    for (auto& channel_it : server_channels_) {
        Channel& channel = channel_it.second;
        if (channel.has_chan_member(old_nick)) {
            change_nick_in_channel(new_nick, old_nick, channel);
        }
    }
    if (!old_nick.empty()) {
        server_nicks_.erase(old_nick);
    }
    server_nicks_.emplace(new_nick, user.get_client().getFd());
    user.set_nickname(new_nick);
}
void    SrvMgr::change_nick_in_channel(const std::string &new_nick, const std::string &old_nick, Channel &channel) {
    string old_signature = server_users_[server_nicks_[old_nick]].get_signature();
    if (channel.has_chan_op(old_nick)) {
        channel.remove_operator(old_nick);
        channel.add_operator(new_nick);
    }
    if (channel.has_chan_member(old_nick)) {
        channel.remove_nick(old_nick);
        channel.add_nick(new_nick);
    }
    string msg = ":" + old_signature + " NICK :" + new_nick;
    cout << msg << endl;
    send_to_chan_all_but_one(channel, msg, new_nick);
}

void    SrvMgr::remove_op_from_channel(Channel& channel, std::string &op) {
    channel.remove_operator(op);
}
void    SrvMgr::remove_nick_from_channel(Channel& channel, std::string &nick) {
    channel.remove_nick(nick);
    if (channel.get_chan_nicks().empty()) {
        server_channels_.erase(channel.get_channel_name());
    }
}
void    SrvMgr::remove_user_from_channel(Channel &channel, std::string &nick) {
    remove_op_from_channel(channel, nick);
    remove_nick_from_channel(channel, nick);
}

void    SrvMgr::send_channel_command_ack(Channel& channel, const User& user) {
    string  ack = ":" + user.get_nickname() + " JOIN :" + channel.get_channel_name();
    send_to_chan_all(channel, ack);
}
void    SrvMgr::send_channel_greetings(Channel& channel, const User& user) {
    if (channel.get_channel_topic() != ":") {
        string  topic = ":" + server_name_ + " " + RPL_TOPIC + " " + user.get_nickname() + " " + channel.get_channel_name() + " " + channel.get_channel_topic();
        string whotime_msg = ":" + server_name_ + " " + RPL_TOPICWHOTIME + " " + user.get_nickname() + " " + channel.get_channel_name() + " " + channel.get_topic_setter() + " " + channel.get_creation_time();
        send_to_one(user, topic);
        send_to_one(user.get_nickname(), whotime_msg);
    }
    string  name_reply = ":" + server_name_ + " " + RPL_NAMREPLY + " " + user.get_nickname() + " = " + channel.get_channel_name() + " :" + channel.get_user_nicks_str();
    send_to_one(user, name_reply);
    string  end_of_names = ":" + server_name_ + " " + RPL_ENDOFNAMES + " " + user.get_nickname() + " " + channel.get_channel_name() + " :End of /NAMES list.";
    send_to_one(user, end_of_names);
}

bool    SrvMgr::nick_exists(std::string &nick) {
     if (server_nicks_.find(nick) == server_nicks_.end()) {
         return false;
     }
    return true;
}
bool    SrvMgr::chan_exists(std::string &chan_name) {
    if (server_channels_.find(chan_name) == server_channels_.end()) {
        return false;
    }
    return true;
}

std::vector<MPlexServer::Client>    SrvMgr::create_client_vector(const std::unordered_set<std::string>& set_of_nicks) const {
    std::vector<MPlexServer::Client> clients;
    for (const string& nick : set_of_nicks) {
        auto nick_it = server_nicks_.find(nick);
        if (nick_it == server_nicks_.end()) {
            continue ;
        }

        int user_id = nick_it->second;

        auto user_it = server_users_.find(user_id);
        if (user_it == server_users_.end()) {
            continue ;
        }

        const User& user = user_it->second;
        MPlexServer::Client client = user.get_client();
        clients.push_back(client);
    }
    return clients;
}