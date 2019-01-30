import socket
import struct

from manager.base import CONNECTION_TYPE_DEBUG, PACKAGE_TYPE_DEBUG, base


def main(connection):
    try:
        buff = b''
        # Read debug messages until the user aorts this process
        while True:
            # Get a package header
            if len(buff) < 3:
                buff += connection.recv(3)

            # Extract package type and length
            package_type, package_len = struct.unpack('<BH', buff[0:3])
            buff = buff[3:]

            # Recieve as long the buffer too few bytes for the package
            while len(buff) < package_len:
                buff += connection.recv(2048)

            if package_type == PACKAGE_TYPE_DEBUG:
                data = buff[0:package_len]
                try:
                    # Try to print the data. Should be ascii..
                    print(data.decode('ascii'), end='', flush=True)
                except UnicodeDecodeError:
                    # Something went wrong.. May not be ascii? Something not transmitted right?
                    print('Decode error for this message:')
                    print(repr(data))
            else:
                print('wrong package type')

            buff = buff[package_len:]  # package done!
    except socket.error as err:
        print('Socketerror: {}'.format(err))
        exit(2)
    connection.close()


if __name__ == '__main__':
    base(main, connection_type=CONNECTION_TYPE_DEBUG)  # get a debug connection
