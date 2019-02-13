# Base file with all defined values used in the communication with
# the ADC. Also has the connection routines to build up an initial connection.
import socket

from settings import connection_timeout, host, port


CONNECTION_TYPE_NONE = b'\x00'
CONNECTION_TYPE_DEBUG = b'\x01'
CONNECTION_TYPE_STATUS = b'\x02'
CONNECTION_TYPE_DATA = b'\x04'
CONNECTION_TYPE_FFT = b'\x08'
PACKAGE_TYPE_RESPONSE = 0
PACKAGE_TYPE_DEBUG = 1
PACKAGE_TYPE_STATUS = 2
PACKAGE_TYPE_DATA = 4
PACKAGE_TYPE_FFT = 8
CONNECT_MAGIC = b'\x10\x00'

STATUSCODES = {
    0x00: 'RESPONSE_OK',
    0x01: 'RESPONSE_MESSAGE_TOO_SHORT',
    0x02: 'RESPONSE_INVALID_PREFIX',
    0x03: 'RESPONSE_INVALID_COMMAND',
    0x04: 'RESPONSE_TOO_FEW_ARGUMENTS',
    0x05: 'RESPONSE_NO_MEMORY',
    0x06: 'RESPONSE_NOT_ENABLED',
    0x07: 'RESPONSE_NO_MEASUREMENTS',
    0x08: 'RESPONSE_TOO_MUCH_MEASUREMENTS',
    0x09: 'RESPONSE_MEASUREMENT_ACTIVE',
    0x0A: 'RESPONSE_NO_SUCH_MEASUREMENT',
    0x0B: 'RESPONSE_NO_ENABLED_MEASUREMENT',
    0x0c: 'RESPONSE_WRONG_ARGUMENT',
    0x0D: 'RESPONSE_FFT_NO_MEMORY',
    0x0E: 'RESPONSE_FFT_INVALID_LENGTH',
    0x0F: 'RESPONSE_FFT_INVALID_WINDOW',
    0x10: 'RESPONSE_ADC_RESET',
    0x11: 'RESPONSE_CALIBRATION_TIMEOUT',
    0x12: 'RESPONSE_SOMETHING_IS_NOT_GOOD',
    0x13: 'RESPONSE_WRONG_REFERENCE_PINS',
    0x14: 'RESPONSE_MESSAGE_TOO_LONG',
    0x15: 'RESPONSE_MESSAGE_TYPE_NOT_SUPPORTED',
}


def base(cb, *args, connection_type=CONNECTION_TYPE_NONE, skip_connection_magic=False,
         skip_check=False, **kwargs):
    """
    Wrapper for the main. Calles the given callback (cb) with the active connection as
    the first parameter and then every extra parameter given.
    """
    cb(get_connection(connection_type, skip_connection_magic, skip_check), *args, **kwargs)


def get_connection(connection_type=CONNECTION_TYPE_NONE, skip_connection_magic=False,
                   skip_check=False):
    """
    Establish a connection to the ADC. Uses host and port given in the settings.py.
    Exits the program with an error, if the connection could not be established.
    After the TCP connection is up, the connection magic and the connection type is
    send to the server. Checks, if the response is valid.
    """
    try:
        c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        c.settimeout(connection_timeout)
        c.connect((host, port))
    except (ConnectionRefusedError, OSError):
        print('Cannot connect to {}:{}'.format(host, port))
        exit(1)

    if not skip_connection_magic:
        connect_payload = CONNECT_MAGIC + connection_type
        c.send(connect_payload)
        if not skip_check:
            recv = c.recv(4)
            if recv != b'\x00\x01\x00\x00':
                print(repr(recv))
                print('error during connecting')
                exit(1)
    c.settimeout(None)
    return c
