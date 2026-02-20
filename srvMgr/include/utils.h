#pragma once

#include <string>
#include <vector>

std::vector<std::string>    process_message(std::string);
void                        strip_trailing_rn(std::string& s);
std::string                 split_off_before_del(std::string& s, char del);
int                         get_msg_type(std::string& s);