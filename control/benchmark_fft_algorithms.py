import socket
import struct

from manager.base import STATUSCODES, base


def main(connection):
    try:

        command = b'\x11\x05'
        connection.send(command)

        response = connection.recv(12)
        print(response)
        print(len(response))
        if len(response) == 0:
            print("The server didn't send any data")
        elif response[3] != 0:
            print("Error: {}".format(STATUSCODES[response[3]]))
        elif len(response) != 12:
            print("The server send {} instead of expected 12 bytes".format(len(response)))
        else:  # OK
            packet_type, length, status, own, dsp_lib = struct.unpack('<BHBII', response)
            print("Own implementation:     {}".format(own))
            print("DSP-Lib implementation: {}".format(dsp_lib))
            print("unit: 10e-5 seconds")

    except socket.error as err:
        print('Socketerror: {}'.format(err))
        exit(2)
    connection.close()


if __name__ == '__main__':
    base(main)
