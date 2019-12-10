import os
from time import sleep
# os.system("../build/node 1337 &")
for i in range(1339, 1428):
    command = "../build/peerpaste --debug -j 127.0.0.1 %s -p %s   &" % (( i-1 ), i)
    print(command)
    os.system(command)
    sleep(0.8)
