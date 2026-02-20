#include <utility>
#include <ctime>

#include "SrvMgr.h"

Channel::Channel(std::string  chan_name, std::string chan_creator) : chan_name_(std::move(chan_name)) {
    chan_nicks_.insert(chan_creator);
    chan_ops_.insert(chan_creator);
    creation_time_ = std::time(nullptr);
}

std::string Channel::get_channel_name() {
    return chan_name_;
}
std::string Channel::get_channel_topic() {
    return topic_;
}
void    Channel::set_channel_topic(std::string& topic) {
    topic_ = topic;
}
std::string Channel::get_user_nicks_str() {
    std::string all_nicks;
    for (const auto& op : chan_ops_) {
        all_nicks += "@" + op + " ";
    }
    for (const auto& nick : chan_nicks_) {
        if (chan_ops_.find(nick) == chan_ops_.end()) {
            all_nicks += nick + " ";
        }
    }
    return all_nicks;
}

std::unordered_set<std::string> Channel::get_chan_nicks() const {
    return chan_nicks_;
}


void    Channel::add_operator(std::string op) {
    chan_ops_.emplace(op);
}

int    Channel::remove_operator(std::string op) {
    return chan_ops_.erase(op);
}

void    Channel::add_nick(std::string nick) {
    chan_nicks_.insert(nick);
    member_count_++;

}

int Channel::remove_nick(std::string nick) {
    if (chan_nicks_.erase(nick) == 1) {
        member_count_--;
        return 1;
    }
    return 0;
}

bool Channel::has_chan_member(const std::string &nick) {
    for (const std::string& member : chan_nicks_) {
        if (member == nick) {
            return true;
        }
    }
    return false;
}

bool Channel::has_chan_op(const std::string &op) {
    for (const std::string& member : chan_ops_) {
        if (member == op) {
            return true;
        }
    }
    return false;
}

void Channel::set_key(const std::string &key) {
    key_ = key;
}
bool    Channel::does_key_fit(const std::string &key) {
    return key_ == key || key_.empty();
}

int Channel::get_member_count() const {
    return member_count_;
}

int Channel::get_member_limit() const {
    return member_limit_;
}

void Channel::set_member_limit(int member_limit) {
    member_limit_ = member_limit;
}

bool Channel::topic_protected() const {
    return topic_protected_;
}

void Channel::set_topic_protected(bool topic_protected) {
    topic_protected_ = topic_protected;
}

bool Channel::needs_invite() const {
    return needs_invite_;
}

void Channel::set_needs_invite(bool needs_invite) {
    needs_invite_ = needs_invite;
}

std::string Channel::get_creation_time() const {
    std::stringstream time;
    time << creation_time_;
    return time.str();
}

std::string Channel::get_topic_setter() const {
    return topic_setter_;
}

void Channel::set_topic_setter(std::string setter_nick) {
    topic_setter_ = setter_nick;
}

std::string Channel::get_topic_set_time() const {
    std::stringstream time;
    time << topic_set_time_;
    return time.str();
}

void Channel::set_topic_set_time() {
    topic_set_time_ = std::time(nullptr);
}

std::string Channel::get_modes() const {
    std::string modes = "+";
    if (needs_invite_) modes += "i";
    if (topic_protected_) modes += "t";
    if (!key_.empty()) modes += "k";
    if (member_limit_ != 0) modes += "l";
    if (!key_.empty()) modes += " secretKey";
    if (member_limit_ != 0) modes += " " + std::to_string(member_limit_);

    return modes;
}
