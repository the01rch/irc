#include "helpers.hpp"
#include <fcntl.h>
#include <iostream>
#include <cstdlib>

bool enable_nonblock(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl: failed to set non-blocking" << std::endl;
		return false;
	}
	return true;
}

void fatal(const std::string& msg)
{
	std::cerr << "Error: " << msg << std::endl;
	exit(1);
}
