import socket
import struct
import threading
import signal
import sys

from manager.base import CONNECTION_TYPE_DATA, PACKAGE_TYPE_DATA, base


DEFAULT_FILENAME = 'samples.txt'


class DataThread(threading.Thread):
    """
    Recieves data and writes to the given filehandle
    """
    meta_info_size = 8

    def __init__(self, connection, stop_event, N, filehandle, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.stop_event = stop_event
        self.filehandle = filehandle
        self.N = N

        self.collected_samples = 0
        self.first_timestamp_seen = None

        self.first_status_print = True

    def run(self):
        try:
            self.recieve()
        except socket.error as err:
            print('Socketerror: {}'.format(err))
            exit(3)

    def collect_more_samples(self):
        """ returns True, if more samples should be collected """
        # If the thread should stop, do not collect more samples.
        if self.stop_event.is_set():
            return False

        # If N is given, we need at least N samples.
        if self.N is not None:
            return self.collected_samples < self.N

        return True

    def recieve(self):
        buff = b''
        # Collect, while we need samples.
        while self.collect_more_samples():
            # Get one package
            if len(buff) < 3:
                buff += self.connection.recv(3)

            package_type, package_len = struct.unpack('<BH', buff[0:3])
            buff = buff[3:]

            # Recieve as long there are too few bytes for the package.
            while len(buff) < package_len:
                buff += self.connection.recv(package_len)

            if package_type == PACKAGE_TYPE_DATA:
                self.input(buff[0: package_len])
            else:
                print("Wrong package recieved: {}".format(package_type))

            # remove full packages from buffer
            buff = buff[package_len:]

        print('\nDone.')
        self.connection.close()

    def input(self, buff):
        """ Takes the content of one package and process it. """
        ref, = struct.unpack('<Q', buff[0:self.meta_info_size])  # get the timereference

        data_len = len(buff) - self.meta_info_size
        # iterate over all values given
        for i in range(data_len // 7):

            # Do not collect more samples as given
            if self.N is not None and self.collected_samples >= self.N:
                continue

            # extract the measurement_id, the actual value and the time difference to the timereference
            id_and_status, value, timestamp_delta = struct.unpack(
                '<BiH',
                buff[i*7 + self.meta_info_size: (i+1)*7 + self.meta_info_size])
            timestamp = ref + timestamp_delta
            id = id_and_status & 0x0F

            # Get the first ever tmestamp.
            if self.first_timestamp_seen is None:
                self.first_timestamp_seen = timestamp

            timestamp -= self.first_timestamp_seen  # offset the time axis to 0.

            # Write to file.
            self.filehandle.write('{}, {}, {}\n'.format(id, timestamp, value))
            self.collected_samples += 1

        # Some status update for the user.
        # Nice, replace the formet text instead of printing always a new line.
        cool_effect = '\r'
        if self.first_status_print:
            self.first_status_print = False
            cool_effect = ''

        if self.N is not None:
            percent = self.collected_samples/self.N * 100
            print('{0}collected {1}/{2} ({3:.2f} %) samples'.format(
                cool_effect, self.collected_samples, self.N, percent), end='')
        else:
            print('{}collected {} samples'.format(cool_effect, self.collected_samples), end='')


class Main:
    """ The main class for collecting data. """

    def __init__(self, connection, filename, N, *args, **kwargs):
        self.connection = connection
        self.filename = filename
        self.N = N
        self.stop_event = threading.Event()

    def run(self):
        with open(self.filename, 'w') as f:
            dataThread = DataThread(self.connection, self.stop_event, self.N, f, daemon=True)
            dataThread.start()
            dataThread.join()

    def signal_handler(self):
        self.stop_event.set()

def main(connection, filename, N):
    global m
    m = Main(connection, filename, N)
    signal.signal(signal.SIGINT, signal_handler)
    m.run()

def signal_handler(*args, **kwargs):
    global m
    m.signal_handler()

if __name__ == '__main__':
    filename = DEFAULT_FILENAME
    N = None
    if len(sys.argv) < 2:
        print('Saving samples, until Ctrl+C to default file {}'.format(filename))
        print('Note: You can provide a file as the first argument and a number of samples as the second one.')
    elif len(sys.argv) == 2:
        filename = sys.argv[1]
        print('Saving samples, until Ctrl+C to the file {}'.format(filename))
    else:
        filename = sys.argv[1]
        try:
            N = int(sys.argv[2])
        except ValueError:
            print('Argument 2 ("{}") is not a number!'.format(sys.argv[2]))
            sys.exit(1)
        if N < 1:
            print('The samples to save must be greater then 0!')
            sys.exit(1)
        print('Saving {} smaples to file {}'.format(N, filename))

    base(main, filename, N, connection_type=CONNECTION_TYPE_DATA)
