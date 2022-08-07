import socket

HOST = "10.0.0.154"  # The server's hostname or IP address
PORT = 8008  # The port used by the server

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    while(1):
        data = s.recv(1024).decode('utf-8')
        print(data,end='')