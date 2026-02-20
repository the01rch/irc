#pragma once

#include <unordered_set>

#include "mplexserver.h"

class User
{
public:
    User() = default;
    User(const User& other) = default;
    User(MPlexServer::Client&);
    ~User() = default;

    User& operator=(const User& other) = default;

    MPlexServer::Client     get_client() const;
    void                    set_nickname(std::string);
    std::string             get_nickname() const;
    void                    set_username(std::string);
    std::string             get_username();
    void                    set_hostname(std::string);
    std::string             get_hostname();
    std::string             get_signature() const;

    std::string             get_farewell_message() const;
    void                    set_farewell_message(const std::string &farewell_message);
    bool                    is_logged_in() const;
    void                    set_as_logged_in(bool is_logged_in);
    bool                    password_provided() const;
    void                    set_password_provided(bool password_provided);
    bool                    cap_negotiation_ended() const;
    void                    set_cap_negotiation_ended(bool cap_negotiation_ended);
    bool                    cap_negotiation_started() const;
    void                    set_cap_negotiation_started(bool cap_negotiation_started);
    void                    add_invitation(std::string& chan_name);
    void                    remove_invitation(std::string& chan_name);
    bool                    has_invitation(std::string& chan_name);

private:
    MPlexServer::Client             client_{};
    bool                            is_logged_in_ = false;
    bool                            password_provided_ = false;
    bool                            cap_negotiation_started_ = false;
    bool                            cap_negotiation_ended_ = false;
    std::string                     nickname_{};
    std::string                     username_{};
    std::string                     hostname_{};
    std::unordered_set<std::string> channel_invites_{};
    std::string                     farewell_message_{};
};