from base import base


url = b'/main.js'


def main(connection):
    # Put together a request.
    connect_payload = b'GET ' + url + b' HTTP/1.1\r\n\r\n'
    connection.send(connect_payload)
    # recieve response, does not check for a correct content length,
    # instead prints just everything send from the server.
    sum_bytes = 0
    while True:
        recv = connection.recv(8192)
        sum_bytes += len(recv)
        print("recv {} bytes ({} insg.)".format(len(recv), sum_bytes))
        try:
            recv.decode('utf-8')
        except UnicodeDecodeError:
            print(repr(recv))

    connection.close()


if __name__ == '__main__':
    base(main, skip_connection_magic=True)  # We just want a raw connection
