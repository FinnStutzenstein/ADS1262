import curses
import socket
import struct
import threading

from manager.base import (
    CONNECT_MAGIC,
    CONNECTION_TYPE_STATUS,
    PACKAGE_TYPE_RESPONSE,
    PACKAGE_TYPE_STATUS,
    connection_timeout,
    host,
    port,
)
from manager.command import CommandManager
from manager.commands import BashCommand
from manager.ui import UI
from manager.state import State
from manager.responseholder import ResponseHolder


class RecvThread(threading.Thread):
    """
    This thread continuously listens on the provided connection.
    If a status update comes in, the main will be notified. If a response somes in, it
    will be put into the response holder and the response recieve event will be fired.
    """
    def __init__(self, connection, response_holder, recv_event, main, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.response_holder = response_holder
        self.recv_event = recv_event
        self.main = main

    def run(self):
        self.main.ui.print('Ready to recieve status updates')
        try:
            self.recieve()
        except socket.error as err:
            self.main.ui.print('Socketerror: {}'.format(err))
            exit(3)

    def recieve(self):
        buff = b''
        while True:
            if len(buff) < 3:
                buff += self.connection.recv(3)

            # Package metadata: type and length
            package_type, package_len = struct.unpack('<BH', buff[0:3])
            buff = buff[3:]

            # recieve until the package is complete.
            while len(buff) < package_len:
                buff += self.connection.recv(package_len)

            try:
                if package_type == PACKAGE_TYPE_STATUS:
                    self.main.update_state(State(bytes(buff)))
                elif package_type == PACKAGE_TYPE_RESPONSE:
                    self.response_holder.set(bytes(buff))
                    self.recv_event.set()
                else:
                    self.main.ui.print('Wrong package recieved: 0x{:02x}'.format(package_type))
            except Exception as e:
                self.main.ui.print(repr(e))

            # remove full packages from buffer:
            buff = buff[package_len:]


class Main:
    """
    This main class holds the UI instance (self.ui), and manages the
    connection establishement, sending commands and retrieving responses.
    """
    def __init__(self, stdscr):
        self.stdscr = stdscr
        stdscr.clear()

        # Initiate all commands.
        self.command_manager = CommandManager(self)

        # The UI
        self.ui = UI(stdscr, self.command_manager)

        self.current_status = None

    def run(self):
        """ The main entry point. Connects and run the main loop. """
        # Greet the user. Be friendly :)
        self.ui.print(
            'Welcome to the ADS1262 management interface!\n' +
            'For help type "help".\nTry to connect to {}:{}...'.format(host, port))
        self.ui.redraw_ui()

        # Try to connect to the server
        try:
            c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            c.settimeout(connection_timeout)
            c.connect((host, port))
        except (ConnectionRefusedError, OSError):
            self.error_handler('Cannot connect to {}:{}'.format(host, port))
            return
        self.ui.print('Connected to {}:{}'.format(host, port))

        # We've got a connection. Send the magic bytes.
        connect_payload = CONNECT_MAGIC + CONNECTION_TYPE_STATUS
        c.send(connect_payload)
        recv = c.recv(4)
        if recv != b'\x00\x01\x00\x00':
            self.error_handler('Error during connecting to the server.')
            return

        # issue request for initial status update
        self.query_initial_status(c)
        c.settimeout(None)

        # The thread for handling incomming messages.
        recv_response = ResponseHolder()
        recv_event = threading.Event()
        recv_thread = RecvThread(c, recv_response, recv_event, self, daemon=True)
        recv_thread.start()

        # Refresh the ui. somehw this is necessary..
        self.ui.redraw_ui()

        # Mainloop: get the users input and precess it within the command system.
        inp = ''
        while inp not in ('q', 'quit'):
            inp = self.ui.wait_input('> ')
            self.ui.print('> ' + inp)
            if not inp:
                continue

            # serach for a command.
            command = self.command_manager.get_command(inp)
            if command is None:
                self.ui.print('The command "{}" was not found.'.format(inp))
                continue

            # Split the arguments from the command.
            args = inp[len(command.command_name):].strip()
            if isinstance(command, BashCommand):
                command.handle(main)
            else:
                # This case is network related.
                try:
                    command_bytes = command.get_command_bytes(args)
                    c.send(command_bytes)

                    # Command send, wait for a response.
                    if recv_event.wait(timeout=command.get_timeout()):
                        recv_event.clear()
                        resp = recv_response.get()
                        if resp is None:
                            self.ui.print("No response from the server")
                        command.handle_response(resp)
                    else:
                        self.ui.print('Response timed out.')
                except ValueError as e:
                    # Something is not right.
                    self.ui.print(str(e))
                except IndexError as e:
                    self.ui.print('The server did not send enough bytes')

    def error_handler(self, msg):
        """ Generic error handler during the connection phase. """
        self.ui.print(msg)
        self.ui.print('An error occurred: Enter q or quit to exit this application')
        self.ui.redraw_ui()
        # Wait for the user to quit.
        inp = ''
        while inp not in ('q', 'quit'):
            inp = self.ui.wait_input('> ').strip()
            self.ui.print(inp)
        return

    def query_initial_status(self, connection):
        """ Sends an initial status update to retrieve the current state. """
        command_bytes = self.command_manager.get_command('adc update state').get_command_bytes('')
        connection.send(command_bytes)

        # Metadata: type and length
        package_descriptor = connection.recv(3)
        package_type, package_len = struct.unpack('<BH', package_descriptor)

        # Get content of the package.
        buff = b''
        while len(buff) < package_len:
            buff += connection.recv(package_len - len(buff))

        if package_type == PACKAGE_TYPE_RESPONSE:
            status, = struct.unpack('<B', buff[0:1])
            if status != 0:
                self.ui.print("Couldn't get a first status update")
            else:
                self.update_state(State(buff[1:]))
        else:
            self.ui.print("Couldn't get a first status update")

    def update_state(self, state):
        self.current_state = state
        self.ui.update_state(state)

    def get_current_state(self):
        return self.current_state


def main(stdscr):
    """ Instantiates the one main class and run it. """
    m = Main(stdscr)
    m.run()


if __name__ == '__main__':
    try:
        curses.wrapper(main)
    except Exception as e:
        curses.echo()
        curses.nocbreak()
        curses.endwin()
        raise e
