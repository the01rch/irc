NAME     := ircserv
CXX      := g++
CXXFLAGS := -Wall -Wextra -Werror -std=c++17 -Iserver/include -IsrvMgr/include  -g
LDFLAGS  :=
RM       := rm -f
RMDIR    := rm -rf

SRCS     := main.cpp \
			./srvMgr/src/SrvMgr.cpp \
			./srvMgr/src/SrvMgrUtils.cpp \
			./srvMgr/src/User.cpp \
			./srvMgr/src/Channel.cpp \
			./srvMgr/src/utils.cpp
OBJS     := $(SRCS:.cpp=.o)

SERVER_DIR := server
SERVER_LIB := $(SERVER_DIR)/libserver.a

.PHONY: all clean fclean re server

all: $(NAME)

$(NAME): server $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(SERVER_LIB) $(LDFLAGS)
	@echo "[ircserv] built $(NAME)"

server:
	$(MAKE) -C $(SERVER_DIR)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS)
	@$(MAKE) -C $(SERVER_DIR) clean
	@echo "[ircserv] cleaned object files"

fclean: clean
	$(RM) $(NAME)
	@$(MAKE) -C $(SERVER_DIR) fclean
	@echo "[ircserv] removed $(NAME)"

re: fclean all