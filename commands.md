# IRC Server Commands Reference

## Connection & Authentication

### NICK - Set/Change Nickname
```
/nick <nickname>
```
**Example:**
```
/nick mana
```

### QUIT - Disconnect from Server
```
/quit [reason]
```
**Example:**
```
/quit See you later!
```

---

## Channel Management

### JOIN - Join a Channel
```
/join <#channel1>,<#channel2> key1
```
**Example:**
```
/join #general
/join #test
```
**Note:** Channel names must start with `#` or `&`

### PART - Leave a Channel
```
/part [#channel] [reason/bye]
```
**Example:**
```
/part #general Goodbye!
/part Ciao!                     # when inside a channel
/part
```

### TOPIC - View/Set Channel Topic
```
/topic [#channel] [new topic]
/topic [new topic]              # when inside a channel
```
**Examples:**
```
/topic                          # View current channel topic
/topic #general                 # View #general's topic
/topic #general :New topic here # Set new topic (requires permission)
```

### INVITE - Invite User to Channel
```
/invite <nick> <#channel>
```
**Example:**
```
/invite alice #general
```

### KICK - Remove User from Channel
```
/kick <#channel> <nick> [reason]
```
**Example:**
```
/kick #general bob Spamming
/kick #general alice
/kick alice                     # when inside a channel
```
**Note:** Requires channel operator privileges

---

## Channel Modes

### MODE - Set/View Channel Modes
```
/mode <#channel> [+/-flags] [parameters]
```

**Available Modes:**
- `i` - Invite-only: Only invited users can join
- `t` - Topic protection: Only operators can change topic
- `k` - Channel key: Requires password to join
- `o` - Operator: Grant/revoke operator privileges
- `l` - User limit: Set maximum number of users

**Examples:**
```
/mode #general                          # View current modes
/mode #general +i                       # Enable invite-only
/mode #general -i                       # Disable invite-only
/mode #general +t                       # Protect topic (ops only)
/mode #general +k secret123             # Set channel password
/mode #general -k secret123             # Remove password
/mode #general +o alice                 # Give alice operator status
/mode #general -o alice                 # Remove alice's operator status
/mode #general +l 50                    # Set user limit to 50
/mode #general -l                       # Remove user limit
/mode #general +itko secretKey nick     # Enable multiple modes at once
/mode +itko secretKey nick              # when inside a channel, enable multiple modes at once
```

---

## Messaging

### PRIVMSG (via /msg or /query) - Send Direct Message
```
/msg <nick> <message>
/query <nick> [message]
```
**Examples:**
```
/msg alice Hello there!
/query bob                      # Open private chat window
/query bob How are you?         # Open window and send message
```

### Channel Messages
Just type your message in the channel buffer:
```
Hello everyone!
```

### CLOSE - Close Private Message Window
```
/close
```
**Note:** Closes current buffer (query/channel window)

---

## Server

### PING - Test Server Connection
```
/ping
```
Server will respond with PONG to confirm connection is alive.

---

## Quick Reference

| Command | Usage | Operator Required |
|---------|-------|-------------------|
| `/nick` | Change nickname | No |
| `/join` | Join channel | No |
| `/part` | Leave channel | No |
| `/msg` | Private message | No |
| `/query` | Open PM window | No |
| `/topic` | View topic | No |
| `/topic` | Set topic | Sometimes (if +t) |
| `/mode` | View modes | No |
| `/mode` | Change modes | Yes |
| `/invite` | Invite to channel | Yes |
| `/kick` | Remove user | Yes |
| `/quit` | Disconnect | No |
| `/close` | Close buffer | No |
| `/ping` | Test connection | No |

---

## Server Info

- **Server:** irc.LeMaDa.hn
- **IP:** tba
- **Port:** 6666
- **Password:** ask project team

**Connect in WeeChat:**
```
/server add 42irc tba/6666 -password=ask_project_team
/connect 42irc
```
