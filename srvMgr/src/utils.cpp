#include <string>
#include <vector>

#include "utils.h"
#include "SrvMgr.h"

void    strip_trailing_rn(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
}

std::string split_off_before_del(std::string& s, char del) {
    size_t  idx = s.find_first_of(del);
    std::string  split_off = s.substr(0, idx);
    if (idx == std::string::npos) {
        s = "";
    } else {
        s = s.substr(idx + 1, s.length() - idx);
    }
    return split_off;
}

std::vector<std::string>    process_message(std::string s) {
    std::vector<std::string>    msg_parts;
    size_t                      idx;

    strip_trailing_rn(s);

    idx = s.find_first_of(' ');
    msg_parts.push_back(s.substr(0, idx));
    msg_parts.push_back(s.substr(idx + 1, s.length() - idx));

    return msg_parts;
}

int     get_msg_type(std::string& s) {
    if (s == "PASS") {
        return cmdType::PASS;
    } else if (s == "CAP") {
        return cmdType::CAP;
    } else if (s == "NICK") {
        return cmdType::NICK;
    } else if (s == "USER") {
        return cmdType::USER;
    } else if (s == "JOIN") {
        return cmdType::JOIN;
    } else if (s == "PART") {
        return cmdType::PART;
    } else if (s == "PRIVMSG") {
        return cmdType::PRIVMSG;
    } else if (s == "TOPIC") {
        return cmdType::TOPIC;
    } else if (s == "MODE") {
        return cmdType::MODE;
    } else if (s == "INVITE") {
        return cmdType::INVITE;
    } else if (s == "KICK") {
        return cmdType::KICK;
    } else if (s == "QUIT") {
        return cmdType::QUIT;
    } else if (s == "PING") {
        return cmdType::PING;
    }
    else {
        return cmdType::NO_TYPE_FOUND;
    }
}