*This project has been created as part of the 42 curriculum by kpires, redrouic.*

# ft_irc

## Description

ft_irc is an IRC server written in C++98. It handles multiple client connections simultaneously using a single `poll()` loop with non-blocking sockets. All outgoing data is buffered per client and flushed through `POLLOUT`, ensuring the server never blocks on `send()`.

The first user to join a channel automatically becomes its operator. Operators can manage channels using dedicated commands (kick, invite, topic, mode).

## Instructions

### Compilation

```
make
```

Other available targets: `make clean`, `make fclean`, `make re`.

### Execution

```
./ircserv <port> <password>
```

- `port` — TCP port the server listens on
- `password` — connection password required by clients

### Testing with netcat

Open one or more terminals and connect with netcat:

```bash
nc 127.0.0.1 6667
PASS mypass
NICK john
USER john 0 * :John
JOIN #general
PRIVMSG #general :hello everyone
```

### Testing with irssi (recommended)

Install irssi if needed:
```bash
sudo apt-get install irssi
```

Connect to the server:
```bash
irssi
/connect localhost 6667 mypass
/join #general
```

irssi provides a more complete IRC client experience with window management, user lists, and full protocol support.

## Supported commands

| Command | Description |
|---------|-------------|
| PASS | Authenticate with the server password |
| NICK | Set or change nickname |
| USER | Register username |
| JOIN | Join a channel (creates it if it does not exist) |
| PART | Leave a channel |
| PRIVMSG | Send a message to a channel or a user |
| KICK | Eject a user from a channel (operator) |
| INVITE | Invite a user to a channel (operator) |
| TOPIC | View or change the channel topic |
| MODE | Change channel modes: i, t, k, o, l |
| LIST | List all available channels on the server |
| NAMES | List users in a specific channel |
| QUIT | Disconnect from the server |

## Resources

### Documentation
- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [poll(2) man page](https://man7.org/linux/man-pages/man2/poll.2.html)
- [Les sockets](https://webusers.i3s.unice.fr/~tettaman/Classes/L2I/ProgSys/11_IntroSockets.pdf)

### Use of AI
AI (GitHub Copilot) was used in this project for:
- Debugging assistance and error interpretation
- Explaining RFC specifications and IRC protocol details