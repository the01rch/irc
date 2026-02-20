CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -Iinclude -g #-Werror
AR       := ar
ARFLAGS  := rcs
RM       := rm -f

SRCDIR   := src
INCDIR   := include
TARGET   := libserver.a

SOURCES  := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.cpp=$(SRCDIR)/%.o)

# ANSI colors for pretty output
COLOR_RESET := \033[0m
COLOR_RED   := \033[31m
COLOR_GREEN := \033[32m
COLOR_YELLOW:= \033[33m
COLOR_BLUE  := \033[34m
COLOR_CYAN  := \033[36m
BOLD        := \033[1m

.PHONY: all clean fclean re message

all: message $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	@printf "$(COLOR_GREEN)[mplexserver]$(COLOR_RESET) built $(BOLD)%s$(COLOR_RESET)\n" "$@"

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	@printf "$(COLOR_BLUE)[mplexserver]$(COLOR_RESET) Compiling $(COLOR_YELLOW)%s$(COLOR_RESET)\n" "$@"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
	@printf "$(COLOR_CYAN)[mplexserver]$(COLOR_RESET) cleaned object files\n"

fclean: clean
	$(RM) $(TARGET)
	@printf "$(COLOR_RED)[mplexserver]$(COLOR_RESET) removed $(BOLD)%s$(COLOR_RESET)\n" "$(TARGET)"

re: fclean all

message:
	@printf "$(COLOR_CYAN)Building MPlexServer sources (src/).$(COLOR_RESET)\n"