import time
import socket
from threading import Thread

home_ip = "192.168.1.201"
home_port = 5000
target_ip = "192.168.1.176"
target_port = 8888
# home_ip = "127.0.0.1"
# target_ip = "127.0.0.1"
# target_port = 5000

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
s.bind((home_ip, home_port))


def udp_receive(skt):
    while True:
        (data, address) = skt.recvfrom(1024)
        print(f"Received {data} from {address[0]}:{address[1]}")


t = Thread(target=udp_receive, args=(s,))
t.start()

commands = []
f = open("zoe.dump", 'r')
while True:
    line = f.readline()
    if line == '':
        break
    line = line[0:line.__len__() - 1]
    commands.append(line)


def test_all_commands():
    for c in commands:
        print(f"Sending {c}")
        s.sendto(bytes(c, "utf-8"), 0, (target_ip, target_port))
        time.sleep(1)


def test_speed():
    s.sendto(bytes("5D7#3363036BF150FE", "utf-8"), 0, (target_ip, target_port))


def test_ready():
    test_command = bytearray(commands[51], "utf-8")
    for i in range(4, test_command.__len__()):
        for j in range(0, 8):
            test_command[i] ^= (1 << j)
            print(f"Flipped bit {j} in byte {i}")
            s.sendto(test_command, 0, (target_ip, target_port))
            time.sleep(1)

# test_all_commands()
# test_speed()
# test_ready()