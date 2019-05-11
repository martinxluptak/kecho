#!/usr/bin/env python3

import telnetlib
import threading
import time


HOST="localhost"
PORT="12345"
ROUNDS = 10000
MSG_LEN = 200
THREADS = 20

class myThread (threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name

    def run(self):
        test_client(self.name)
        
def test_client(thr):
    tn = telnetlib.Telnet(HOST, PORT)

    for x in range(ROUNDS):
        sent = 'x' * MSG_LEN
        tn.write(sent.encode('ascii'))
        l = tn.read_some()

threads = []
start = time.time()

for i in range(THREADS):
    threads.append(myThread(i, "Thread%d" % i ))

for thread in threads:
    thread.start()

for thread in threads:
    thread.join()


elapsed = time.time() - start

print(elapsed)
