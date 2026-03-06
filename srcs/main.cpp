#include "IRCCore.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

volatile sig_atomic_t g_caught_sig = 0;

static void sig_catch(int sig)
{
	(void)sig;
	g_caught_sig = 1;
}

static bool checkPort(const char* str)
{
	int val = std::atoi(str);
	return (val > 0 && val <= 65535);
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	if (!checkPort(argv[1]))
	{
		std::cerr << "Error: port must be between 1 and 65535" << std::endl;
		return 1;
	}

	std::string secret = argv[2];
	if (secret.empty())
	{
		std::cerr << "Error: password must not be empty" << std::endl;
		return 1;
	}

	signal(SIGINT, sig_catch);
	signal(SIGTERM, sig_catch);
	signal(SIGQUIT, sig_catch);

	try
	{
		IRCCore core(std::atoi(argv[1]), secret);
		core.loop();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
