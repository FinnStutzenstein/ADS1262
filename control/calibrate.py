import socket
import json
import struct
import threading

from manager.base import PACKAGE_TYPE_RESPONSE, connection_timeout, base, STATUSCODES
from manager.responseholder import ResponseHolder


class TimeoutException(Exception):
    """ Exception, if a timeout happens """
    pass


class RecvThread(threading.Thread):
    """ handles all incomming data. Waits for a package with type 'response'. """
    def __init__(self, connection, response_holder, recv_event, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.response_holder = response_holder
        self.recv_event = recv_event

    def run(self):
        try:
            self.recieve()
        except socket.error as err:
            print('Socketerror: {}'.format(err))

    def recieve(self):
        buff = b''
        while True:
            if len(buff) < 3:
                buff += self.connection.recv(3)

            package_type, package_len = struct.unpack('<BH', buff[0:3])
            buff = buff[3:]

            while len(buff) < package_len:
                buff += self.connection.recv(package_len)

            if package_type == PACKAGE_TYPE_RESPONSE:
                self.response_holder.set(bytes(buff))
                self.recv_event.set()

            # remove full packages from buffer:
            buff = buff[package_len:]


class Main():
    choices_samplerate = ['2.5', '5', '10', '16.6', '20', '50', '60', '100', '400',
                          '1200', '2400', '4800', '7200', '14400', '19200', '38400']
    choices_filter = ['sinc1', 'sinc2', 'sinc3', 'sinc4', 'FIR']
    choices_pga = ['1', '2', '4', '8', '16', '32', 'bypassed']
    choices_ref_pos_pin = ['0', '2', '4', 'AVDD']
    choices_ref_neg_pin = ['1', '3', '5', 'AVSS']

    def __init__(self, connection):
        self.connection = connection
        with open('protocol.json', 'r') as f:
            self.commands_dict = json.load(f)

        self.recv_response = ResponseHolder()
        self.recv_event = threading.Event()
        recv_thread = RecvThread(connection, self.recv_response, self.recv_event, daemon=True)
        recv_thread.start()

    def calibrate(self):
        print('Calibrating the ADS1262')

        # We cannot calibrate, if something else is using the ADC
        print('Check the current state...')
        if not self.check_idle_state():
            return

        # Query, Samplerate, filter, PGA, v_ref.
        # Confirm them.
        print('\nWe need to query some information to calibrate the ADC...')
        settings_confirmed = False
        while not settings_confirmed:
            ref_type = self.query('Internal/External referencevoltage', ['internal', 'external'], default='internal')
            if ref_type == 'internal':
                v_ref = 250000000
            else:
                print('\nExternal reference: Please specify the reference voltage and the reference input pins.')
                v_ref = self.query('Reference voltage (in 10nv)', {'from': 0, 'to': int(pow(2, 31))}, default=250000000)
                ref_pos_pin = self.query('Positive pin', self.choices_ref_pos_pin)
                ref_neg_pin = self.query('Negative pin', self.choices_ref_neg_pin)

            pga = self.query('PGA', self.choices_pga, default='1')
            samplerate = self.query('Samplerate', self.choices_samplerate, default='10')
            filter = self.query('Filter', self.choices_filter, default='sinc4')

            settings_str = ('\tReference voltage: {} nV\n\tPGA: {}\n\tsamplerate: {}\n\t' +
                            'filter: {}\n').format(v_ref*10, pga, samplerate, filter)
            settings_confirmed = self.query(
                '\nConfirm these settings?\n' + settings_str, ['y', 'n'], default='y').lower() in ('y', 'yes')

        # Set everything:
        if ref_type == 'internal':
            self.send_command('adc reference set internal')
        else:
            # Add +1 for the indices, becuase the arbuemtn to the server must be between 1 and 4.
            ref_pos_pin_index = self.choices_ref_pos_pin.index(ref_pos_pin) + 1
            ref_neg_pin_index = self.choices_ref_neg_pin.index(ref_neg_pin) + 1
            arguments = (
                      self.value_to_bytes(v_ref, 'u64') +
                      self.value_to_bytes(ref_pos_pin_index, 'u8') +
                      self.value_to_bytes(ref_neg_pin_index, 'u8'))
            self.send_command('adc reference set external', arguments)

        if pga == 'bypass':
            self.send_command('adc pga bypass')
        else:
            pga_index = self.choices_pga.index(pga)
            self.send_command('adc pga set gain', self.value_to_bytes(pga_index, 'u8'))

        samplerate_index = self.choices_samplerate.index(samplerate)
        self.send_command('adc set samplerate', self.value_to_bytes(samplerate_index, 'u8'))
        filter_index = self.choices_filter.index(filter)
        self.send_command('adc set filter', self.value_to_bytes(filter_index, 'u8'))

        # Guess timeout: 16 Samples, *2 as buffer.
        timeout = int(32 / float(samplerate)) + 1

        # Offset: get inputs and confirm
        print('\nOffset calibration: Please short two inputs!')
        pos = self.query('Positive pin', {'from': 0, 'to': 15})
        neg = self.query('Negative pin', {'from': 0, 'to': 15})
        if self.query('\nContinue?', ['y', 'n'], default='y').lower() not in ('y', 'yes'):
            return
        if not self.check_idle_state():
            return
        print('This may take up to {} seconds...'.format(timeout))

        # Do offset calibration
        inputs = self.value_to_bytes(pos, 'u8') + self.value_to_bytes(neg, 'u8')
        resp = self.send_command('calibrationsequence offset', inputs, timeout=timeout)
        offset = struct.unpack('<i', resp[1:5])[0]
        offset_diff_nv = (v_ref * 10 * -offset) / pow(2, 24)

        # Scale: get inputs and confirm
        print('\nScale calibration: Please apply a full scale voltage (v_ref) to two inputs!')
        pos = self.query('Positive pin', {'from': 0, 'to': 15})
        neg = self.query('Negative pin', {'from': 0, 'to': 15})
        if self.query('\nContinue?', ['y', 'n'], default='y').lower() not in ('y', 'yes'):
            return
        if not self.check_idle_state():
            return
        print('This may take up to {} seconds...'.format(timeout))

        # Do sclae calibration
        inputs = self.value_to_bytes(pos, 'u8') + self.value_to_bytes(neg, 'u8')
        resp = self.send_command('calibrationsequence scale', inputs, timeout=timeout)
        scale = struct.unpack('<I', resp[1:5])[0]
        scale_diff = ((scale/0x400000)-1)
        if abs(scale_diff) > 0.01:
            scale_diff = '{0:.2f} %'.format(scale_diff * 100)
        else:
            scale_diff = '{0:.2f} ppm'.format(scale_diff * 1000000)

        # Print both cal-values.
        print('\nDone!')
        print('Offset: {0} (diff: {1:.2f} nV)'.format(offset, offset_diff_nv))
        print('Scale: {} (diff: {})'.format(scale, scale_diff))

    def value_to_bytes(self, val, type):
        """ Converts the value to bytes by the given format. """
        # https://docs.python.org/3/library/struct.html
        # Just all needed..
        mapping = {
            'u8': 'B',
            'u64': 'Q'
        }

        if type not in mapping:
            raise NotImplementedError('The type {} is not known.'.format(type))
        return struct.pack('<' + mapping[type], val)

    def check_idle_state(self):
        """ Returnes true, if the ADC is in Idle state. """
        state = self.get_current_state()
        if state != 'Idle':
            print('The state is {}, not Idle. Aborting.'.format(state))
            return False
        return True

    def get_current_state(self):
        """ Returnes the verbose state of the ADC. """
        resp = self.send_command('adc update state')
        if len(resp) < 2:
            raise Exception('The server did not return enough data.')
        return ['Idle', 'Running', 'Oneshot', 'Calibrating'][resp[1] & 0x03]

    def send_command(self, command_name, argument_bytes=b'', timeout=connection_timeout):
        """
        Sends the given command with the arguemt_bytes as payload.
        Checks for the response status code. If it is not OK, an exception
        will be raised. Returns the complete payload, even with the status code.
        """

        # find command:
        command_bytes = None
        for prefix, command_group in self.commands_dict.items():
            for command, command_dict in command_group.items():
                if command_dict['command'] == command_name:
                    prefix = int(prefix.split('x')[1], 16)
                    command = int(command.split('x')[1], 16)
                    command_bytes = bytes([prefix, command])

        if command_bytes is None:
            raise Exception('Cound not find command "{}"'.format(command_name))

        # Send command
        command_bytes += argument_bytes
        self.connection.send(command_bytes)

        # Wait for response
        if self.recv_event.wait(timeout=timeout):
            self.recv_event.clear()
            resp = self.recv_response.get()
            if len(resp) < 1:
                raise Exception('The server did not send any data.')
            if resp[0] != 0:
                raise Exception('Status code not ok: {}'.format(STATUSCODES.get(
                    resp[0], 'Unknown status code {}'.format(int(resp[0])))))
            return resp
        else:
            raise TimeoutException()

    def query(self, msg, expected_input, default=None):
        """
        Giving the message, query the input from the user. The expected input can be:
        {from: <number>, to: <number>} or ['choiceA', 'choiceB', ...].
        If default is given, the user can hit enter to take the default value.
        """
        prompt = msg
        if default is not None:
            prompt += ' [{}]'.format(default)
        prompt += ': '

        value = None
        value_ok = False

        while not value_ok:
            value = input(prompt)
            if value == '' and default is not None:
                value = default
            if isinstance(expected_input, dict):
                try:
                    value = int(value)
                    if value < expected_input['from'] or value > expected_input['to']:
                        raise ValueError()
                    value_ok = True
                except ValueError:
                    print('Error: Value needs to be an integer between {} and {}'.format(
                        expected_input['from'], expected_input['to']))
            else:
                if value in expected_input:
                    value_ok = True
                else:
                    print('Error: Value needs to be one of: {}'.format(
                        ', '.join(expected_input)))

        return value


def main(connection):
    m = Main(connection)
    m.calibrate()
    connection.close()


if __name__ == '__main__':
    base(main)  # get a connection
