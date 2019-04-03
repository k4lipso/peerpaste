import os
from time import sleep
# os.system("../build/node 1337 &")
for i in range(1338, 1348):
    command = "../build/peerpaste -j localhost %s -p %s --debug &" % (( i-1 ), i)
    print(command)
    os.system(command)
    sleep(0.4)
