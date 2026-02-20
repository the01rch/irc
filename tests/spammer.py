import socket
clients = []
for i in range(0,50):
    sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY,1)
    sock.connect(("157.90.114.127",6666))
    sock.sendall(b"PASS abc\r\nNICK superspammer_%d\r\nUSER spamgod 0 * :hugy\r\n" % i)
    sock.sendall(b"JOIN #lobby\r\n")
    clients.append(sock)
print(sock.recv(1024))

for c in clients:
    for i in range(50):
        c.sendall(b"PRIVMSG #lobby Super spammer spamming hard\r\n")