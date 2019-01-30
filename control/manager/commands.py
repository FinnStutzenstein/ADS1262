import struct

from .base import STATUSCODES
from .state import State
from .utils import parse_number
from .base import connection_timeout


class BaseCommand:
    """
    Represents one command. A command needs a prefix and `command` (meaning the second byte
    to identify the command). The `command_dict` is the configuration object from the json
    describing hwo the command behaves. `main` is for printing responses.

    The interesting function is `get_command_bytes`, which, given some verbose arguments,
    creates the actual bytes to send to the server.

    `get_timeout` specifies the timeout needed for this command. Subclasses can overwrite
    this, if they know, that the command may take some time.

    `format_for_help` returnes a printable representation of the command giving a help
    text and explainations for the arguments.
    """
    def __init__(self, prefix, command, command_dict, main):
        self.main = main

        # Parse the prefix and command to numbers.
        prefix = parse_number(prefix)
        command = parse_number(command)

        # Make them the first two bytes.
        self.command_bytes = self.value_to_bytes(prefix, 'u8')
        self.command_bytes += self.value_to_bytes(command, 'u8')

        self.command_name = command_dict['command']
        # get the arguments for the command.
        self.args = command_dict.get('args', [])

    def parse_args(self, args):
        """
        Parses the args given and return the corresponding bytes.

        The arguments are splitted via `.split()`, meaning, that they are divided by
        ay whitespace (refer to the python-docs for more information).

        Every argument is tried to match to the argument description in `self.args`.
        If the argumetn fits the description and could be parsed, the resulting
        bytes for the command to send is build.

        All in all, all bytes from all arguments are concatinated and returned.
        """
        args = [a.strip() for a in args.split()]

        # Check, if the right mount of arguments are given.
        if len(args) != len(self.args):
            raise ValueError('Expected {} arguments, {} given.\n{}'.format(
                len(self.args), len(args), self.format_for_help()))

        # Build up the arugment bytes
        argument_bytes = b''

        # iterate over all args. `i` is the index, `arg` the decription and `arg_input` the string from the user.
        for i, (arg, arg_input) in enumerate(zip(self.args, args)):
            # Some information about the arugment.
            arg_value_limit, arg_type = self.get_value_limit_and_type(arg)

            # check for different argument types: range, in, ...
            if arg_value_limit == 'range':
                # We must have a number. Parse it and check, if it lays in the specified range.
                value = parse_number(arg_input)
                if value < arg['range']['from'] or value > arg['range']['to']:
                    raise ValueError('Argument {} (value {}) is out of range.\n{}'.format(
                        i+1, value, self.format_for_help()))

            elif arg_value_limit == 'in':
                # The argument must be one of `arg['in']`
                if arg_input not in arg['in']:
                    raise ValueError('Argument {} (value {}) is not in the given set.\n{}'.format(
                        i+1, arg_input, self.format_for_help()))
                value = arg['in'][arg_input]

            elif arg_value_limit == 'number':
                # A simple number. Parse the value.
                value = parse_number(arg_input)

            # `value` is a number. Check for overflows, by the argument type.
            signed, bits = self.get_signed_and_bits(arg_type)
            if ((signed and (value < -pow(2, bits-1) or value > pow(2, bits-1)+1))
                    or not signed and (value < 0 or value > pow(2, bits)-1)):
                raise ValueError('Argument {} (value {}) is too large (or small) for the range of {}.\n{}'.format(
                    i+1, value, arg_type, self.format_for_help()))

            # Convert the given munber to bytes respecting the argument type.
            argument_bytes += self.value_to_bytes(value, arg_type)
        return argument_bytes

    def format_for_help(self):
        """ Returns a printable representation of this command """
        if len(self.args) == 0:
            return 'Usage: {}'.format(self.command_name)

        # Build up strings for each argument.
        args_formatted = []
        for arg in self.args:
            arg_value_limit_str = ''  # For some limits of the argument
            arg_value_limit, arg_type = self.get_value_limit_and_type(arg)

            # Range: we have limits. Format them.
            if arg_value_limit == 'range':
                arg_value_limit_str = 'Range from {} to {} (all incl.)'.format(
                    arg['range']['from'], arg['range']['to'])

            # In: The "limits" are all values, the argument can be.
            elif arg_value_limit == 'in':
                arg_value_limit_str = 'Value in {' + ', '.join(arg['in'].keys()) + '}'

            # For a number, we need to observe the argument type. Either the number is signed or not,
            # and giving the amount of bytes.
            elif arg_value_limit == 'number':
                signed, bits = self.get_signed_and_bits(arg_type)
                if signed:
                    arg_value_limit_str = 'signed number with size of {} byte'.format(int(bits/8))
                else:
                    arg_value_limit_str = 'unsigned number with size of {} byte'.format(int(bits/8))

            # Put everything together and add ahelp text.
            args_formatted.append('  <{}: {}>'.format(arg.get('help', 'not documented'), arg_value_limit_str))

        return 'Usage: {}\n{}'.format(self.command_name, '\n'.join(args_formatted))

    def get_signed_and_bits(self, type):
        """ Returns the amout of bits and the signess of the argument type """
        signed = (type[0] == 's')
        bits = int(type[1:])
        return (signed, bits)

    def get_value_limit_and_type(self, arg):
        """ Returns a tuple of the type (range, in, number, ...) and size in bits: """
        limits = ['range', 'in']
        keys = arg.keys()
        for limit in limits:
            if limit in keys:
                return (limit, arg.get('type', 'u8'))
        return ('number', arg.get('type', 'u8'))

    def value_to_bytes(self, val, type):
        """ Converts the given `val` (type number) to bytes. The type is respected. """
        # https://docs.python.org/3/library/struct.html
        mapping = {
            's8': 'b',
            'u8': 'B',
            's16': 'h',
            'u16': 'H',
            's32': 'i',
            'u32': 'I',
            's64': 'q',
            'u64': 'Q'
        }

        if type not in mapping:
            raise NotImplementedError('The type {} is not known.'.format(type))
        return struct.pack('<' + mapping[type], val)

    def get_command_bytes(self, args):
        """ Returnes the byte needed to send to the server given `args` """
        return self.command_bytes + self.parse_args(args)

    def get_timeout(self):
        """ Returnes the timeout. Default: connection_timeout specified in the settings.py """
        return connection_timeout


class BashCommand(BaseCommand):
    """
    A special type of command, that does not interact with the server.
    Let every non-server command inherit from this base class.
    """
    def __init__(self, command):
        # Some dummy values. No arguments. Ties does not mean, that this command can take arguments,
        # but they are not validated with `parse_args`.
        super().__init__(0, 0, {'command': command}, None)

    def get_command_bytes(self, *args, **kwargs):
        raise NotImplementedError('This is not allowed for a dummy command.')

    def handle(self, args):
        raise NotImplementedError('A child class should implement handle!')


class QuitCommand(BashCommand):
    """ Dummy. Does nothing. """
    def handle(self, args):
        pass


class ListCommand(BashCommand):
    """ Lists all available commands. """
    def handle(self, args):
        command_names = []
        for command in self.main.command_manager.get_all_commands():
            command_names.append('  ' + command.command_name)
        self.main.ui.print('All available commands. Type help <command> for more info.\n{}'.format(
            '\n'.join(command_names)))


class HelpCommand(BashCommand):
    """
    Prints help. If no argument is given, the command will print general help instructions.
    If some arguments are specified, the command will print help for the specified
    command, if found.
    """
    def handle(self, args):
        if len(args) == 0:
            self.main.ui.print(
                'Help\n- Type q or quit to exit\n' +
                '- type list to list all available commands\n' +
                '- use help <command> to see the command structure')
        else:
            command = self.main.command_manager.get_command(args)
            if command is None:
                self.main.ui.print('Help: Command "{}" not found.'.format(args))
            else:
                self.main.ui.print(command.format_for_help())


class RemoteCommand(BaseCommand):
    """
    Handles all commands, that have to deal with the server.
    This is the class, a command will be instantiated with, if there is
    no special command class. The response statuscode will be checked, and
    any error will be printed to the ui.
    """
    def print_error(self, status):
        """ Prints an error message to the ui with the given status code. """
        self.main.ui.print('Error: {}'.format(STATUSCODES.get(
            status, 'Unknown status code {}'.format(status))))

    def handle_response(self, response):
        # get statuscode
        status = response[0]
        if status != 0:
            self.print_error(status)
        else:
            self.main.ui.print('OK')


class MeasurementCreateCommand(RemoteCommand):
    """ Retrievs the create measurement id. """
    def handle_response(self, response):
        # get status:
        status, = struct.unpack('<B', response[0:1])
        if status != 0:
            self.print_error(status)
        else:
            id, = struct.unpack('<B', response[1:2])
            self.main.ui.print('Created measurement with id {}.'.format(id))


class AdcUpdateStateCommand(RemoteCommand):
    """ Retrievs the current status and prints it to the ui. """
    def handle_response(self, response):
        # get status:
        status, = struct.unpack('<B', response[0:1])
        if status != 0:
            self.print_error(status)
        else:
            state = State(response[1:])
            self.main.update_state(state)
            self.main.ui.print(str(state))


class Base4BytesInReturnCommand(RemoteCommand):
    """
    This base class accepts next to the status byte 4 additional bytes.
    `handle_data` is called, so a inheriting class can take care about
    the interpretation of the 4 bytes.
    """
    def handle_response(self, response):
        # get status:
        status, = struct.unpack('<B', response[0:1])
        if status != 0:
            self.print_error(status)
            return

        if len(response) < 5:
            self.ui.print("The server didn't returned enough data")
            return

        self.handle_data(response[1:5])

    def handle_data(self, data):
        raise NotImplementedError('A child needs to implement this function')


class MeasurementOneshotCommand(Base4BytesInReturnCommand):
    """ Does a oneshot measurement. """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.timeout = connection_timeout

    def get_command_bytes(self, args):
        """
        Override to get the measurement id. Then use it to get the averaging
        and calculate the timeout. This command may take longer then `connection_timeout`
        """
        command_bytes = super().get_command_bytes(args)
        id = int(command_bytes[2])  # the id is the first argument.

        # get the current state.
        state = self.main.get_current_state()
        if state is None:
            self.timeout = connection_timeout
            return command_bytes

        # get the mesurmement state.
        m = state.get_measurement(id)
        if m is None:
            self.timeout = connection_timeout
            return command_bytes

        # A measurement takes 1/f_s * averaging time.
        self.timeout = m.get_averaging()/state.samplerate
        self.timeout = int(self.timeout*2)+1  # *2 as buffer and round up

        # Let the timeout not be less then `connection_timeout`.
        if self.timeout < connection_timeout:
            self.timeout = connection_timeout

        # Inform the user, that this may take some time.
        self.main.ui.print('The measurement may take up to {} seconds.'.format(self.timeout))
        return command_bytes

    def handle_data(self, data):
        """ Interprets the 4 bytes as an signed int32. """
        value = struct.unpack('<i', data[0:4])[0]
        value = value / 100000000.0
        self.main.ui.print('Result: {}V'.format(value))

    def get_timeout(self):
        return self.timeout


class CalibrationsequenceOffsetCommand(Base4BytesInReturnCommand):
    """ Interprets the data as the new offset """
    def handle_data(self, data):
        offset = struct.unpack('<i', data[0:4])[0]
        v_ref = self.main.get_current_state().v_ref
        offset_diff_nv = (v_ref * -offset) / pow(2, 24)
        self.main.ui.print('Offset: {0} (diff: {1:.2f} nV)'.format(offset, offset_diff_nv))


class CalibrationsequenceScaleCommand(Base4BytesInReturnCommand):
    """ Interprets the data as the new scale """
    def handle_data(self, data):
        scale = struct.unpack('<I', data[0:4])[0]
        scale_diff = ((scale/0x400000)-1)
        if abs(scale_diff) > 0.01:
            scale_diff = '{0:.2f} %'.format(scale_diff * 100)
        else:
            scale_diff = '{0:.2f} ppm'.format(scale_diff * 1000000)
        self.main.ui.print('Scale: {} (diff: {})'.format(scale, scale_diff))


class PrintCommand(BaseCommand):
    """
    This base class handels all commands, that recievs ascii data
    and prints it to the ui.
    """
    def handle_response(self, response):
        self.main.ui.print(response.decode('ascii'))


class PrintNetworkstatsCommand(PrintCommand):
    pass


class PrintOsstatsCommand(PrintCommand):
    pass


class PrintConnectionsCommand(PrintCommand):
    pass
