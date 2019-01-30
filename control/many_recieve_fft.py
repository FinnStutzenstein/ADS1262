import socket
import struct
import sys
import threading

from manager.base import CONNECTION_TYPE_FFT, PACKAGE_TYPE_FFT, get_connection


class DataThread(threading.Thread):
    def __init__(self, connection, client_id, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.client_id = client_id
        self.counter = 100

    def run(self):
        try:
            self.recieve()
        except socket.error as err:
            print('Socketerror: {}'.format(err))
            exit(3)

    def recieve(self):
        buff = b''
        while True:
            if len(buff) < 3:
                buff += self.connection.recv(3)

            package_type, package_len = struct.unpack('<BH', buff[0:3])
            buff = buff[3:]

            while len(buff) < package_len:
                buff += self.connection.recv(package_len)

            if package_type == PACKAGE_TYPE_FFT:
                self.input(buff[0: package_len])
            else:
                print("Wrong package recieved: {}".format(package_type))

            # remove full packages from buffer:
            buff = buff[package_len:]

    def input(self, buff):
        status = ''
        if (len(buff)-21) % 8 != 0:
            status = 'Size does not fit'
        else:
            status = 'FFT size: {}'.format(int((len(buff)-21)/8))

        print("{} {} got package with len {}. {}".format(
            self.client_id,
            self.counternumber(),
            len(buff),
            status))

    def counternumber(self):
        self.counter += 1
        if self.counter > 999:
            self.counter = 100
        return self.counter


def main(num_clients):
    connections = []
    threads = []
    for i in range(num_clients):
        c = get_connection(CONNECTION_TYPE_FFT)
        threads.append(DataThread(c, i))
        connections.append(c)

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()

    for connection in connections:
        connection.close()


def usage():
    print("usage: python many_recieve_fft.py <clients from 1 to 10>")
    exit(1)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        usage()

    try:
        clients = int(sys.argv[1])
    except (ValueError, KeyError):
        usage()

    if clients > 10 or clients < 1:
        usage()

    main(clients)
