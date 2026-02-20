#!/usr/bin/env python3
import socket
import time

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)  # wichtig!
sock.connect(('157.90.114.127', 6666))

sock.sendall(b"PAS")
time.sleep(0.1)

sock.sendall(b"S a")
time.sleep(0.1)

sock.sendall(b"bc\r\n")

time.sleep(0.5)
sock.settimeout(1)
try:
    print(sock.recv(4096).decode())
except:
    pass

sock.close()
