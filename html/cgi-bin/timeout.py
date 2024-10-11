import socket
import time

host = 'localhost'
PORT = 8080
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((host, PORT))
    s.sendall(b'GET / HTTP/1.1\r\n')
    s.sendall(b'Host: localhost\r\n')
    time.sleep(2)
    s.sendall(b'\r\n')
    data = s.recv(1024)
    print(repr(data))
