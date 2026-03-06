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

### Testing

Open one or more terminals and connect with netcat:

```
nc 127.0.0.1 6667
PASS mypass
NICK john
USER john 0 * :John
JOIN #general
PRIVMSG #general :hello everyone
```

The server is also compatible with irssi and other standard IRC clients.

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
| QUIT | Disconnect from the server |

## Resources

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [poll(2) man page](https://man7.org/linux/man-pages/man2/poll.2.html)