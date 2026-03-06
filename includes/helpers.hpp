#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <string>

bool enable_nonblock(int fd);
void fatal(const std::string& msg);

#endif
