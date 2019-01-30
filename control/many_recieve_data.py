import socket
import struct
import sys
import threading

from manager.base import CONNECTION_TYPE_DATA, PACKAGE_TYPE_DATA, get_connection


class DataThread(threading.Thread):
    data_max_freq = 100  # See explaination in recieve_data.py. Use None to disable.
    meta_info_size = 8

    def __init__(self, connection, client_id, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.client_id = client_id
        self.last_time_stamp = {}
        if self.data_max_freq is not None:
            self.data_min_time_delta = 1000 / self.data_max_freq
        self.first_time_stamp = {}  # per cannel
        self.measure_count = {}  # per channel

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

            if package_type == PACKAGE_TYPE_DATA:
                self.input(buff[0: package_len])
            else:
                print('Wrong package recieved: {}'.format(package_type))

            # remove full packages from buffer:
            buff = buff[package_len:]

    def input(self, buff):
        ref, = struct.unpack('<Q', buff[0:self.meta_info_size])

        data_len = len(buff)-self.meta_info_size
        for i in range(data_len // 7):
            id, value, timestamp_delta = struct.unpack(
                '<BiH',
                buff[i*7 + self.meta_info_size: (i+1)*7 + self.meta_info_size])
            timestamp = (timestamp_delta + ref)/100  # in ms.

            if id not in self.first_time_stamp:
                self.first_time_stamp[id] = timestamp
                self.measure_count[id] = 0

            self.measure_count[id] += 1

            if self.data_max_freq is None:
                print(self.client_id, id, value/100000000.0, timestamp)
            else:
                if (id not in self.last_time_stamp
                        or timestamp > (self.last_time_stamp[id] + self.data_min_time_delta)):
                    print(self.client_id, id, value/100000000.0, timestamp)
                    self.last_time_stamp[id] = timestamp
                    self.print_effective_samplerate(id, timestamp)

    def print_effective_samplerate(self, id, current_timestamp):
        if (current_timestamp <= self.first_time_stamp[id]):
            return

        eff = (self.measure_count[id]-1)/(current_timestamp - self.first_time_stamp[id])
        print(self.client_id, id, 'eff: ', eff * 1000)


def main(num_clients):
    connections = []
    threads = []
    for i in range(num_clients):
        c = get_connection(CONNECTION_TYPE_DATA)
        threads.append(DataThread(c, i))
        connections.append(c)

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()

    for connection in connections:
        connection.close()


def usage():
    print('usage: python many_recieve_data.py <clients from 1 to 10>')
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
