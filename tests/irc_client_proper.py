#!/usr/bin/env python3
"""
IRC Protocol Compliant Client with Color Support
Properly handles IRC message framing with CRLF line endings
Adds colors and formatting for better readability
"""

import socket
import sys
import select
import threading
import hashlib

class IRCFormatter:
    """Handle IRC message formatting with colors"""
    
    # ANSI color codes
    RESET = '\033[0m'
    BOLD = '\033[1m'
    
    # User colors (avoid dark colors for readability)
    USER_COLORS = [
        '\033[91m',  # Light Red
        '\033[92m',  # Light Green
        '\033[93m',  # Light Yellow
        '\033[94m',  # Light Blue
        '\033[95m',  # Light Magenta
        '\033[96m',  # Light Cyan
        '\033[33m',  # Yellow
        '\033[35m',  # Magenta
        '\033[36m',  # Cyan
    ]
    
    SERVER_COLOR = '\033[1;37m'  # Bold White
    
    def __init__(self):
        self.nickname_colors = {}
    
    def get_nick_color(self, nickname):
        """Get consistent color for a nickname"""
        if nickname not in self.nickname_colors:
            # Hash nickname to get consistent color
            hash_val = int(hashlib.md5(nickname.encode()).hexdigest(), 16)
            color_idx = hash_val % len(self.USER_COLORS)
            self.nickname_colors[nickname] = self.USER_COLORS[color_idx]
        return self.nickname_colors[nickname]
    
    def format_message(self, raw_message):
        """Format IRC message with colors"""
        # Server numeric replies (001-999)
        if raw_message.startswith(':server ') and len(raw_message) > 8:
            parts = raw_message.split(' ', 3)
            if len(parts) >= 3 and parts[1].isdigit():
                # Server message - make it bold
                return f"{self.SERVER_COLOR}{raw_message}{self.RESET}"
        
        # Parse IRC message: :prefix COMMAND params :trailing
        if raw_message.startswith(':'):
            parts = raw_message.split(' ', 2)
            if len(parts) >= 2:
                prefix = parts[0][1:]  # Remove leading :
                command = parts[1]
                rest = parts[2] if len(parts) > 2 else ""
                
                # Extract nickname from prefix (nick!user@host or just nick)
                nick = prefix.split('!')[0]
                
                if command == 'PRIVMSG':
                    # Format: :nick PRIVMSG #channel :message
                    msg_parts = rest.split(' :', 1)
                    if len(msg_parts) == 2:
                        channel = msg_parts[0]
                        message = msg_parts[1]
                        nick_color = self.get_nick_color(nick)
                        return f"{nick_color}<{nick}>{self.RESET} {message}"
                
                elif command == 'NICK':
                    # Format: :oldnick NICK :newnick
                    if ':' in rest:
                        new_nick = rest.split(':', 1)[1]
                        return f"{self.SERVER_COLOR}* {nick} is now known as {new_nick}{self.RESET}"
                
                elif command == 'QUIT':
                    # Format: :nick QUIT :reason
                    reason = rest.split(':', 1)[1] if ':' in rest else "Quit"
                    return f"{self.SERVER_COLOR}* {nick} has quit ({reason}){self.RESET}"
                
                elif command == 'JOIN':
                    return f"{self.SERVER_COLOR}* {nick} has joined{self.RESET}"
                
                elif command == 'PART':
                    return f"{self.SERVER_COLOR}* {nick} has left{self.RESET}"
        
        # Default: return as-is with server color for unhandled messages
        if raw_message.startswith(':server') or raw_message.startswith('Available commands'):
            return f"{self.SERVER_COLOR}{raw_message}{self.RESET}"
        
        return raw_message

class IRCClient:
    def __init__(self, host='localhost', port=6667, password='DaLeMa26'):
        self.host = host
        self.port = port
        self.password = password
        self.sock = None
        self.recv_buffer = ""
        self.running = False
        self.formatter = IRCFormatter()
        
    def connect(self):
        """Connect to the IRC server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.running = True
            print(f"Connected to {self.host}:{self.port}")

            # Send PASS command (standard IRC authentication)
            self.sock.sendall(f'PASS {self.password}\r\n'.encode('utf-8'))
            print("Connected to server. Type messages and press Enter. Type '/quit' to exit.")
            try:
                while True:
                    sockets = [self.sock, sys.stdin]
                    readable, _, _ = select.select(sockets, [], [])

                    for s in readable:
                        if s == self.sock:
                            # Receive message from server
                            message = self.sock.recv(1024).decode()
                            if not message:
                                print("Disconnected from server.")
                                return
                            print(message.strip())
                        else:
                            # Send user input to server
                            user_input = input()
                            if user_input.strip().lower() == '/quit':
                                print("Exiting client.")
                                return
                            self.send_message(user_input)
            except KeyboardInterrupt:
                print("\nExiting client.")
                return
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def send_message(self, message):
        """Send a message to the server with proper formatting"""
        try:
            # Convert user-friendly /commands to IRC protocol commands
            if message.startswith("/"):
                cmd_parts = message[1:].split(' ', 1)
                cmd = cmd_parts[0].upper()
                args = cmd_parts[1] if len(cmd_parts) > 1 else ""
                
                # Map common commands
                if cmd == "NICK":
                    formatted_message = f"NICK {args}"
                elif cmd == "QUIT":
                    formatted_message = f"QUIT :{args}" if args else "QUIT"
                elif cmd == "WHO" or cmd == "USERS":
                    formatted_message = "WHO"
                elif cmd == "MSG" and args:
                    # /msg target message -> PRIVMSG target :message
                    msg_parts = args.split(' ', 1)
                    if len(msg_parts) == 2:
                        formatted_message = f"PRIVMSG {msg_parts[0]} :{msg_parts[1]}"
                    else:
                        print("Usage: /msg <target> <message>")
                        return
                elif cmd == "JOIN" and args:
                    formatted_message = f"JOIN {args}"
                elif cmd == "PART" and args:
                    formatted_message = f"PART {args}"
                else:
                    # Unknown command, send as-is without /
                    formatted_message = message[1:]
            else:
                # Regular text - wrap in PRIVMSG for channel
                formatted_message = f"PRIVMSG #general :{message}"

            # Send the formatted message
            self.sock.sendall((formatted_message.rstrip('\r\n') + '\r\n').encode('utf-8'))
        except Exception as e:
            print(f"Send error: {e}")
            self.running = False
    
    def receive_loop(self):
        """Continuously receive and display messages from server"""
        while self.running:
            try:
                # Use select to avoid blocking
                ready = select.select([self.sock], [], [], 0.1)
                
                if self.sock in ready[0]:
                    data = self.sock.recv(4096)
                    
                    if not data:
                        print("\n[Server closed connection]")
                        self.running = False
                        break
                    
                    # Append to buffer
                    self.recv_buffer += data.decode('utf-8', errors='ignore')
                    
                    # Process complete messages (ending with \r\n)
                    while '\r\n' in self.recv_buffer:
                        line, self.recv_buffer = self.recv_buffer.split('\r\n', 1)
                        if line:
                            self.display_message(line)
                            
            except Exception as e:
                print(f"\n[Receive error: {e}]")
                self.running = False
                break
    
    def display_message(self, message):
        """Display IRC message in readable format with colors"""
        formatted = self.formatter.format_message(message)
        print(formatted)
    
    def input_loop(self):
        """Handle user input"""
        print("\nType messages and press Enter. Commands: /nick <name>, /who, /quit")
        print("---")
        
        while self.running:
            try:
                # Check if stdin has data
                ready = select.select([sys.stdin], [], [], 0.1)
                
                if sys.stdin in ready[0]:
                    line = sys.stdin.readline().rstrip('\n')
                    
                    if line:
                        if line == '/quit':
                            self.send_message('/quit')
                            self.running = False
                            break
                        else:
                            self.send_message(line)
                            
            except KeyboardInterrupt:
                print("\n[Disconnecting...]")
                self.running = False
                break
            except Exception as e:
                print(f"\n[Input error: {e}]")
                break
    
    def run(self):
        """Run the client"""
        if not self.connect():
            return
        
        # Start receive thread
        recv_thread = threading.Thread(target=self.receive_loop, daemon=True)
        recv_thread.start()
        
        # Run input loop in main thread
        try:
            self.input_loop()
        finally:
            self.running = False
            if self.sock:
                self.sock.close()
            print("[Disconnected]")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 irc_client_proper.py <password> [host] [port]")
        sys.exit(1)

    password = sys.argv[1]
    host = sys.argv[2] if len(sys.argv) > 2 else 'localhost'
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 6667

    client = IRCClient(host, port, password)
    client.connect()
