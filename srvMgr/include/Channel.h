//
// Created by daspring on 2/9/26.
//

#pragma once

#include <ctime>

#include "User.h"

class Channel
{
public:
    Channel() = default;
    Channel(std::string  chan_name, std::string chan_creator);
    Channel(const Channel& other) = default;
    ~Channel() = default;

    Channel&    operator=(const Channel& other) = default;

    std::string                     get_channel_name();
    std::string                     get_channel_topic();
    void                            set_channel_topic(std::string&);
    std::string                     get_user_nicks_str();
    std::unordered_set<std::string> get_chan_nicks() const;
    void                            add_operator(std::string);
    int                             remove_operator(std::string);
    void                            add_nick(std::string);
    int                             remove_nick(std::string);
    bool                            has_chan_member(const std::string &nick);
    bool                            has_chan_op(const std::string &nick);

    std::string                     get_modes() const;

    void                            set_key(const std::string &key);
    bool                            does_key_fit(const std::string &key);
    int                             get_member_count() const;
    int                             get_member_limit() const;
    void                            set_member_limit(int member_limit);
    bool                            topic_protected() const;
    void                            set_topic_protected(bool topic_protected);
    bool                            needs_invite() const;
    void                            set_needs_invite(bool needs_invite);
    std::string                     get_creation_time() const;
    std::string                     get_topic_setter() const;
    void                            set_topic_setter(std::string setter_nick);
    std::string                     get_topic_set_time() const;
    void                            set_topic_set_time();

private:
    std::string                     chan_name_;
    std::time_t                     creation_time_;
    std::string                     key_{};
    std::string                     topic_{":"};
    std::string                     topic_setter_{};
    std::time_t                     topic_set_time_{};
    int                             member_count_{0};
    int                             member_limit_{0};
    bool                            topic_protected_{false};
    bool                            needs_invite_{false};

    std::unordered_set<std::string> chan_nicks_;
    std::unordered_set<std::string> chan_ops_;
    std::unordered_set<std::string> invites_;
};