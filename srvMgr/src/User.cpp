#include "mplexserver.h"
#include "SrvMgr.h"

std::string User::get_farewell_message() const {
    return farewell_message_;
}

void User::set_farewell_message(const std::string& farewell_message) {
    farewell_message_ = farewell_message;
}

User::User(MPlexServer::Client& client) : client_(client)
{
}

[[nodiscard]] bool User::is_logged_in() const {
    return is_logged_in_;
}

void User::set_as_logged_in(bool is_logged_in) {
    is_logged_in_ = is_logged_in;
}

[[nodiscard]] bool User::password_provided() const {
    return password_provided_;
}

void User::set_password_provided(bool password_provided) {
    password_provided_ = password_provided;
}

[[nodiscard]] bool User::cap_negotiation_ended() const {
    return cap_negotiation_ended_;
}

void User::set_cap_negotiation_ended(bool cap_negotiation_ended) {
    cap_negotiation_ended_ = cap_negotiation_ended;
}

[[nodiscard]] bool User::cap_negotiation_started() const {
    return cap_negotiation_started_;
}

void User::set_cap_negotiation_started(bool cap_negotiation_started) {
    cap_negotiation_started_ = cap_negotiation_started;
}

MPlexServer::Client User::get_client() const {
    return client_;
}

void        User::set_nickname(std::string nickname) {
    nickname_ = nickname;
}
std::string User::get_nickname() const {
    return nickname_;
}
void        User::set_username(std::string username) {
    username_ = username;
}
std::string User::get_username() {
    return username_;
}
void        User::set_hostname(std::string hostname) {
    hostname_ = hostname;
}
std::string User::get_hostname() {
    return hostname_;
}

std::string User::get_signature() const {
    return nickname_ + "!" + username_ + "@" + hostname_;
}

void    User::add_invitation(std::string &chan_name) {
    channel_invites_.insert(chan_name);
}

void User::remove_invitation(std::string &chan_name) {
    channel_invites_.erase(chan_name);
}

bool User::has_invitation(std::string &chan_name) {
    if (channel_invites_.find(chan_name) == channel_invites_.end()) {
        return false;
    }
    return true;
}
