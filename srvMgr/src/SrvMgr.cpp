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

SrvMgr::SrvMgr(MPlexServer::Server& srv, const string& server_password, const string& server_name) : srv_instance_(srv), server_password_(server_password), server_name_(server_name) {
}

void    SrvMgr::onConnect(MPlexServer::Client client) {
    cout << "[CONNECT] New client: " << client.getIpv4() << ":" << client.getPort() << endl;
    server_users_.emplace(client.getFd(), User(client));
}

void    SrvMgr::onDisconnect(MPlexServer::Client client) {
    User&       user = server_users_[client.getFd()];
    std::string nick = user.get_nickname();
    std::string signature = ":" + user.get_signature();

    cout << "[DISCONNECT] " << nick << " (" << client.getIpv4() << ":" << client.getPort() << ") left" << endl;


    if (user.is_logged_in()) {
        std::vector<string> keys;
        for (const auto& pair : server_channels_) {
            keys.push_back(pair.first);
        }
        for (const auto& key : keys) {
            const auto& it = server_channels_.find(key);
            if (it == server_channels_.end()) continue;
            Channel& channel = it->second;

            if (channel.has_chan_member(nick)) {
                remove_user_from_channel(channel, nick);
                string  msg = ":" + user.get_signature() + " QUIT :Quit: User disconnected";
                send_to_chan_all_but_one(channel, msg, nick);
            }
        }
    }
    server_nicks_.erase(nick);
    server_users_.erase(client.getFd());
}

void    SrvMgr::onMessage(const MPlexServer::Message msg) {
    const MPlexServer::Client&  client = msg.getClient();
    User&                       user = server_users_[client.getFd()];
    
    cout << "[MSG] Received: '" << msg.getMessage() << "'" << endl;
    
    std::vector<string>         msg_parts = process_message(msg.getMessage());
    const int                   command = get_msg_type(msg_parts[0]);

    cout << "[MSG] Command: " << msg_parts[0] << " (type: " << command << ")" << endl;
    if (msg_parts.size() > 1 && !msg_parts[1].empty()) {
        cout << "[MSG] Args: '" << msg_parts[1] << "'" << endl;
    }

    // some commands are only allowed after the user registered successfully
    if (command > cmdType::USER && !user.is_logged_in()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NOTREGISTERED + " * " + ":You have not registered";
        send_to_one(user, err_msg);
        return ;
    }

    switch (command) {
        case cmdType::PASS:
            process_password(msg_parts[1], client, user);
            break;
        case cmdType::CAP:
            process_cap(msg_parts[1], client, user);
            break;
        case cmdType::NICK:
            process_nick(msg_parts[1], client, user);
            break;
        case cmdType::USER:
            process_user(msg_parts[1], client, user);
            break;
        case cmdType::JOIN:
            process_join(msg_parts[1], user);
            break;
        case cmdType::PART:
            process_part(msg_parts[1], client, user);
            break;
        case cmdType::PRIVMSG:
            process_privmsg(msg_parts[1], client, user);
            break;
        case cmdType::TOPIC:
            process_topic(msg_parts[1], client, user);
            break;
        case cmdType::MODE:
            process_mode(msg_parts[1], client, user);
            break;
        case cmdType::INVITE:
            process_invite(msg_parts[1], client, user);
            break;
        case cmdType::KICK:
            process_kick(msg_parts[1], client, user);
            break;
        case cmdType::QUIT:
            process_quit(msg_parts[1], client, user);
            break;
        case cmdType::PING:
            pong(msg_parts[1], client, user);
            break;
        default:
            cout << "no cmd_type found.\n";
            string  nick = user.get_nickname().empty()? "*" : user.get_nickname();
            string  err_msg = ":" + server_name_ + " " + ERR_UNKNOWNERROR + " " + nick + " " + ":Could not parse command or parameters";
            send_to_one(user, err_msg);
            return ;
    }

}

void    SrvMgr::process_password(const std::string& provided_password, const MPlexServer::Client& client, User& user) const {
    if (user.is_logged_in()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_ALREADYREGISTERED + " " + user.get_nickname() + " " + ":You may not reregister\r\n");
        return ;
    }
    if (provided_password == server_password_) {
        user.set_password_provided(true);
    }
    else {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_PASSWDMISMATCH + " * " + ":Password incorrect\r\n");
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NOTREGISTERED + " * " + ":You have not registered\r\n");
        srv_instance_.sendTo(client, "ERROR :Closing Link: " + client.getIpv4() + " (Password incorrect)\r\n");
        srv_instance_.disconnectClient(client);
        return ;
    }
    if (!user.is_logged_in()) {
        try_to_log_in(user ,client);
    }
}

void    SrvMgr::process_cap(const string& s, const MPlexServer::Client& client, User& user) const {
    user.set_cap_negotiation_started(true);
    if (s == "END") {
        user.set_cap_negotiation_ended(true);
    }
    else {
        srv_instance_.sendTo(client, "CAP * LS :\r\n");
    }
    if (!user.is_logged_in()) {
        try_to_log_in(user ,client);
    }
}

void    SrvMgr::process_nick(const string& s, const MPlexServer::Client& client, User& user) {
    string	new_nick;
	string	old_nick = user.get_nickname();
	string	old_signature = user.get_signature();

	if (!user.is_logged_in()) {
		old_nick = "*";
	}
    if (s.empty()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NONICKNAMEGIVEN + " " + old_nick + " :No nickname given\r\n");
        return ;
    }
    if (s.find_first_of("#&:; ") != s.npos) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_ERRONEUSNICKNAME + " " + old_nick + " " + s + " :Erroneous nickname, it may not contain \"#:; \"\r\n");
        return ;
	} else {
		new_nick = s;
 }
    if (server_nicks_.find(new_nick) != server_nicks_.end()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NICKNAMEINUSE + " " + old_nick + " " + new_nick + " :Nickname is already in use\r\n");
    } else {
        change_nick(new_nick, old_nick, user);
        if (user.is_logged_in()) {
            string msg = ":" + old_signature + " NICK :" + new_nick;
            cout << msg << endl;
            srv_instance_.sendTo(client, msg + "\r\n");
        }
    }
    if (!user.is_logged_in()) {
        try_to_log_in(user ,client);
    }
}

void    SrvMgr::process_user(string s, const MPlexServer::Client& client, User& user) const {
    cout << "[USER] Processing USER command with params: '" << s << "'" << endl;
    
    if (user.is_logged_in()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_ALREADYREGISTERED + " " + user.get_nickname() + " " + ":You may not reregister\r\n");
        return ;
    }

    string username = split_off_before_del(s, ' ');
    string hostname = split_off_before_del(s, ' ');

    if (username.empty() || hostname.empty()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " * " + ":Not enough parameters for user registration\r\n");
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NOTREGISTERED + " * " + ":You have not registered\r\n");
        return ;
    }

    user.set_username(username);
    user.set_hostname(hostname);

    cout << "process_user: username: " << username
        << ", hostname: " << hostname << endl;
    if (!user.is_logged_in()) {
        try_to_log_in(user ,client);
    }

    // if no password was provided after CAP, NICK and USER registration fails and we terminate
    if (!user.password_provided()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_PASSWDMISMATCH + " * " + ":Password incorrect\r\n");
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NOTREGISTERED + " * " + ":You have not registered\r\n");
        srv_instance_.sendTo(client, "ERROR :Closing Link: " + client.getIpv4() + " (Password incorrect)\r\n");
        srv_instance_.disconnectClient(client);
    }
}

// JOIN <channel>{,<channel>} [<key>{,<key>}]
// To do: support multiple channels and keys???
void    SrvMgr::process_join(string s, User& user) {
    if (s.empty()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " JOIN :Not enough parameters";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    string  chan_names = split_off_before_del(s, ' ');
    string  keys = split_off_before_del(s, ' ');

    while (!chan_names.empty()) {
        string  chan_name = split_off_before_del(chan_names,',');
        string  key = split_off_before_del(keys,',');
        join_channel(chan_name, key, user);
    }
}

// KICK <channel> <client> :[<message>]
// Only channel operators may kick
void    SrvMgr::process_kick(std::string s, const MPlexServer::Client& client, User& user) {
    string chan_name = split_off_before_del(s, ' ');
    string target_nick = split_off_before_del(s, ' ');
    string message = s;
    if (!message.empty() && message[0] == ':') message = message.substr(1);

    // Check channel exists
    auto chan_it = server_channels_.find(chan_name);
    if (chan_it == server_channels_.end()) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_NOSUCHCHANNEL + " " + user.get_nickname() + " " + chan_name + " :No such channel\r\n");
        return;
    }
    Channel& channel = chan_it->second;

    // Check user is channel operator
    if (!channel.has_chan_op(user.get_nickname())) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_CHANOPRIVSNEEDED + " " + user.get_nickname() + " " + chan_name + " :You're not channel operator\r\n");
        return;
    }

    // Check target is in channel
    if (!channel.has_chan_member(target_nick)) {
        srv_instance_.sendTo(client, ":" + server_name_ + " " + ERR_USERNOTINCHANNEL + " " + user.get_nickname() + " " + target_nick + " " + chan_name + " :They aren't on that channel\r\n");
        return;
    }

    // Compose KICK message
    string kick_msg = ":" + user.get_signature() + " KICK " + chan_name + " " + target_nick + " :" + (message.empty() ? user.get_nickname() : message) + "\r\n";
    send_to_chan_all(channel, kick_msg);

    // Remove user from channel
    remove_user_from_channel(channel, target_nick);
}

void    SrvMgr::process_part(string s, const MPlexServer::Client& client, User& user) {
    (void)  client;
    string  nick = user.get_nickname();

    if (s.empty()) {
        string msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " PART :Not enough parameters";
        send_to_one(nick, msg);
        return ;
    }

    string chan_name = split_off_before_del(s, ' ');
    string reason = s;

    if (server_channels_.find(chan_name) == server_channels_.end()) {
        string msg = ":" + server_name_ + " " + ERR_NOSUCHCHANNEL + " " + user.get_nickname() + " " + chan_name + " :No such channel";
        send_to_one(nick, msg);
        return ;
    }
    Channel& channel = server_channels_[chan_name];
    if (channel.get_chan_nicks().find(user.get_nickname()) == channel.get_chan_nicks().end()) {
        string msg = ":" + server_name_ + " " + ERR_NOTONCHANNEL + " " + user.get_nickname() + " " + chan_name + " :You're not on that channel";
        send_to_one(nick, msg);
        return ;
    }

    string  message = ":" + user.get_signature() + " PART " + chan_name + " " + reason;
    send_to_chan_all(channel, message);
    remove_user_from_channel(channel, nick);
}

void    SrvMgr::process_privmsg(std::string s, const MPlexServer::Client& client, User& user) {
    (void)  client;
    string  target = split_off_before_del(s, ' ');
    string  message = s;
    string  nick = user.get_nickname();

    if (message.empty()) {
        string err_msg = ":" + server_name_ + " " + ERR_NOTEXTTOSEND + " " + nick + " :No text to send";
        send_to_one(user, err_msg);
        return ;
    }

    if (target[0] != '#' && target[0] != '&') {
        if (server_nicks_.find(target) == server_nicks_.end()) {
            string err_msg = ":" + server_name_ + " " + ERR_NOSUCHNICK + " " + nick + " " + target + " :No such nick";
            send_to_one(user, err_msg);
            return ;
        } else {
            message = ":" + user.get_signature() + " PRIVMSG " + target + " " + message;
            send_to_one(target, message);
        }
    } else {
        auto    chan_it = server_channels_.find(target);
        if (chan_it == server_channels_.end()) {
            string err_msg = ":" + server_name_ + " " + ERR_NOSUCHCHANNEL + " " + nick + " " + target + " :No such channel";
            send_to_one(user, err_msg);
            return ;
        } else {
            Channel& channel = chan_it->second;
            if (channel.get_chan_nicks().find(nick) == channel.get_chan_nicks().end()) {
                string err_msg = ":" + server_name_ + " " + ERR_NOTONCHANNEL + " " + nick + " " + target + " :You're not on that channel";
                send_to_one(user, err_msg);
                return ;
            }
            message = ":" + user.get_signature() + " PRIVMSG " + target + " " + message;
            send_to_chan_all_but_one(channel, message, nick);
        }
    }
}
// TOPIC <channel> [<topic>]
// If topic is not given, return current topic. 
// Only channel operators can set topic if topic_protected mode is enabled.
void    SrvMgr::process_topic(std::string s, const MPlexServer::Client& client, User& user) {
    (void) client;
    if (s.empty()) {
        string msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " TOPIC :Not enough parameters";
        send_to_one(user, msg);
        return;
    }

    string chan_name = split_off_before_del(s, ' ');
    string new_topic = s;
    if (!new_topic.empty() && new_topic[0] == ':') new_topic = new_topic.substr(1);

    auto chan_it = server_channels_.find(chan_name);
    if (chan_it == server_channels_.end()) {
        string msg = ":" + server_name_ + " " + ERR_NOSUCHCHANNEL + " " + user.get_nickname() + " " + chan_name + " :No such channel";
        send_to_one(user, msg);
        return;
    }
    Channel& channel = chan_it->second;

    if (!channel.has_chan_member(user.get_nickname())) {
        string err_msg = ":" + server_name_ + " " + ERR_NOTONCHANNEL + " " + user.get_nickname() + " " + chan_name + " :You're not on that channel";
        send_to_one(user, err_msg);
        return;
    }

    // If no topic provided, reply with current topic
    if (new_topic.empty()) {
        string  topic_msg;
        if (channel.get_channel_topic() == ":") {
            topic_msg = ":" + server_name_ + " " + RPL_NOTOPIC + " " + user.get_nickname() + " " + chan_name + " :No topic is set";
            send_to_one(user.get_nickname(), topic_msg);
        } else {
            topic_msg = ":" + server_name_ + " " + RPL_TOPIC + " " + user.get_nickname() + " " + chan_name + " " + channel.get_channel_topic();
            send_to_one(user.get_nickname(), topic_msg);
            string whotime_msg = ":" + server_name_ + " " + RPL_TOPICWHOTIME + " " + user.get_nickname() + " " + chan_name + " " + channel.get_topic_setter() + " " + channel.get_topic_set_time();
            send_to_one(user.get_nickname(), whotime_msg);
        }
        return;
    }

    if (channel.topic_protected() && !channel.has_chan_op(user.get_nickname())) {
        string err_msg = ":" + server_name_ + " " + ERR_CHANOPRIVSNEEDED + " " + user.get_nickname() + " " + chan_name + " :You're not channel operator";
        send_to_one(user, err_msg);
        return;
    }

    // Set new topic and notify all users in the channel
    channel.set_channel_topic(new_topic);
    channel.set_topic_setter(user.get_username());
    string topic_set_msg = ":" + user.get_signature() + " TOPIC " + chan_name + " :" + new_topic;
    send_to_chan_all(channel, topic_set_msg);
}

void    SrvMgr::process_mode(std::string s, const MPlexServer::Client& client, User& user) {
    (void)  client;
    (void)  user;

    string  target = split_off_before_del(s, ' ');          // must be a channel (as per the subject file)
    string  modestring = split_off_before_del(s, ' ');      // +-itkol
    string  mode_arguments = s;                                 // only for +kol-o
    char    plusminus;

    auto it = server_channels_.find(target);
    if (it == server_channels_.end()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NOSUCHCHANNEL + " " + user.get_nickname() + " " + target + " :No such channel";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    Channel&    channel = it->second;

    if (modestring.empty()) {
        string  msg = ":" + server_name_ + " " + RPL_CHANNELMODEIS + " " + user.get_nickname() + " " + channel.get_channel_name() + " " + channel.get_modes();
        send_to_one(user.get_nickname(), msg);
        msg = ":" + server_name_ + " " + RPL_CREATIONTIME + " " + user.get_nickname() + " " + channel.get_channel_name() + " " + channel.get_creation_time();
        send_to_one(user.get_nickname(), msg);
        return ;
    }

    if (!channel.has_chan_op(user.get_nickname())) {
        string  err_msg = ":" + server_name_ + " " + ERR_CHANOPRIVSNEEDED + " " + user.get_nickname() + " " + target + " :You're not a channel operator";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }

    if (modestring[0] != '-' && modestring[0] != '+') {
        string  err_msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " MODE :Not enough parameters";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    for (char m : modestring) {
        if (m == '-') plusminus = m;
        else if (m == '+') plusminus = m;
        else if (m == 'i') mode_i(plusminus, mode_arguments, channel, user);
        else if (m == 't') mode_t(plusminus, mode_arguments, channel, user);
        else if (m == 'k') mode_k(plusminus, mode_arguments, channel, user);
        else if (m == 'o') mode_o(plusminus, mode_arguments, channel, user);
        else if (m == 'l') mode_l(plusminus, mode_arguments, channel, user);
        else {
            string  err_msg = ":" + server_name_ + " " + ERR_UMODEUNKNOWNFLAG + " " + user.get_nickname() + " :Unknown MODE flag";
            send_to_one(user.get_nickname(), err_msg);
            return ;
        }
    }
}

void    SrvMgr::process_invite(std::string s, const MPlexServer::Client &client, User &user) {
    (void)  client;
    string  target_nick = split_off_before_del(s, ' ');
    string  target_chan = split_off_before_del(s, ' ');

    auto it = server_channels_.find(target_chan);
    if (it == server_channels_.end()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NOSUCHCHANNEL + " " + user.get_nickname() + " " + target_chan + " :No such channel";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    Channel&    channel = it->second;
    if (!nick_exists(target_nick)) {
        string  err_msg = ":" + server_name_ + " " + ERR_NOSUCHNICK + " " + user.get_nickname() + " " + target_nick + " :No such nick";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    if (!channel.has_chan_member(user.get_nickname())) {
        string  err_msg = ":" + server_name_ + " " + ERR_NOTONCHANNEL + " " + user.get_nickname() + " " + target_chan + " :You're not on that channel";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    if (!channel.has_chan_op(user.get_nickname()) && channel.needs_invite()) {
        string  err_msg = ":" + server_name_ + " " + ERR_CHANOPRIVSNEEDED + " " + user.get_nickname() + " " + target_chan + " :You're not channel operator";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    if (channel.has_chan_member(target_nick)) {
        string  err_msg = ":" + server_name_ + " " + ERR_USERONCHANNEL + " " + user.get_nickname() + " " + target_nick + " " + target_chan + " :is already on channel";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    User& target_user = server_users_.find(server_nicks_.find(target_nick)->second)->second;
    target_user.add_invitation(target_chan);
    string  msg = ":" + server_name_ + " " + RPL_INVITING + " " + user.get_nickname() + " " + target_nick + " " + target_chan;
    send_to_one(user.get_nickname(), msg);
    msg = ":" + user.get_signature() + " INVITE " + target_nick + " " + target_chan;
    send_to_one(target_nick, msg);
}

void    SrvMgr::process_quit(string s, const MPlexServer::Client &client, User& user) {
	user.set_farewell_message(s);
	srv_instance_.disconnectClient(client);
}

void    SrvMgr::pong(const string &s, const MPlexServer::Client &client, const User& user) {
    if (s.empty()) {
        string  err_msg = ":" + server_name_ + " " + ERR_NEEDMOREPARAMS + " " + user.get_nickname() + " PING :Not enough parameters";
        send_to_one(user.get_nickname(), err_msg);
        return ;
    }
    string nick = server_users_[client.getFd()].get_nickname();
    srv_instance_.sendTo(client, ":" + server_name_ + " PONG " + server_name_ + " " + s + "\r\n");
    cout << ":" + server_name_ + " PONG " + server_name_ + " :" + s << endl;
}