import struct


samplerate_reverse_lookup = [2.5, 5, 10, 16.6, 20, 50, 60, 100, 400,
                             1200, 2400, 4800, 7200, 14400, 19200, 38400]
filter_reverse_lookup = ['sinc1', 'sinc2', 'sinc3', 'sinc4', 'FIR']
pga_reverse_lookup = ['1', '2', '4', '8', '16', '32']
window_reverse_lookup = {
    0: 'Hann',
    1: 'Berlett',
    2: 'Welch',
    255: 'Rectangular',
}
started_reverse_lookup = ['Idle', 'Running', 'Oneshot', 'Calibrating']

adc_state_size = 29
measurement_state_size = 9


class StateError(Exception):
    """ Represents an error during the creation of a state or measurement state """
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return self.msg


class MeasurementState:
    """
    Represents a measurement state with all needed information for one specific measurement.
    We need `measurement_state_size` bytes to parse the state of the measurement.

    The string representation can be used to display the measurements state.
    """
    def __init__(self, measurement_bytes):
        if len(measurement_bytes) < measurement_state_size:
            raise StateError("The server didn't send enough data")

        # Get all needed information from the data
        (self.id, input_mux, self.enabled, self.averaging, self.fft_enabled,
         self.fft_length, self.fft_window_index) = struct.unpack(
            '<BBBHBHB', measurement_bytes)

        self.neg = int(input_mux & 0x0F)
        self.pos = int((input_mux & 0xF0) >> 4)

    def get_id(self):
        return self.id

    def get_averaging(self):
        return self.averaging

    def __str__(self):
        """ Returns a printable presentation of the measurement state """
        if self.enabled:
            enabled = 'enabled'
        else:
            enabled = 'disabled'

        if self.fft_enabled:
            fft_enabled = 'enabled'
        else:
            fft_enabled = 'disabled'

        if self.averaging == 0:
            averaging = 'disabled'
        else:
            averaging = self.averaging

        try:
            fft_window = window_reverse_lookup[self.fft_window_index]
        except KeyError:
            fft_window = 'Unkown window'

        return ('{}: {}\n  input_mux: {} {}\n  averaging: {}\n  FFT: {}, ' +
                'length: {}\n  FFT window: {}\n').format(
                    self.id, enabled, self.pos, self.neg, averaging,
                    fft_enabled, self.fft_length, fft_window)


class State:
    """
    Represents the state of the ADC. Next to all values, this class also manages the measurements.
    """
    def __init__(self, state_bytes):
        if len(state_bytes) < adc_state_size:
            raise StateError("The server didn't send enough data")

        # get all main and non optional data from the state.
        (flags, sr_filter, self.pga, self.v_ref, v_ref_inputs, self.calibration_offset,
         self.calibration_scale, self.measurement_count) = struct.unpack('<BBBQBqQB', state_bytes[0:adc_state_size])

        # parse the data.
        self.started = started_reverse_lookup[int(flags & 0x03)]
        self.internal_reference = bool(flags & 0x04)
        self.slow_connection = bool(flags & 0x08)
        self.ADC_reset = bool(flags & 0x10)
        try:
            self.samplerate = samplerate_reverse_lookup[sr_filter & 0x0F]
        except IndexError:
            self.filter = "unknown samplerate"
        try:
            self.filter = filter_reverse_lookup[(sr_filter & 0xF0) >> 4]
        except IndexError:
            self.filter = "unknown filter"
        self.v_ref_neg = int(v_ref_inputs & 0x0F)
        self.v_ref_pos = int((v_ref_inputs & 0xF0) >> 4)

        # Get the actual meaning of the offset and scale values.
        self.cal_offset_diff_nv = (self.v_ref * -self.calibration_offset) / pow(2, 24)
        self.cal_scale_diff = ((self.calibration_scale/0x400000)-1)

        # Holds all the measurements.
        self.measurements = {}

        measurement_bytes = state_bytes[adc_state_size:]
        if len(measurement_bytes) < (self.measurement_count * measurement_state_size):
            raise StateError("The server didn't send enough data")

        # Create measurements.
        for i in range(self.measurement_count):
            m = MeasurementState(measurement_bytes[i*measurement_state_size: (i+1)*measurement_state_size])
            self.measurements[m.get_id()] = m

    def get_measurement(self, id):
        """ returnes the measurement state for the given measurement id """
        return self.measurements.get(id, None)

    def __str__(self):
        """ Makes the state (incl. measurements) printable. """
        if self.slow_connection:
            slow_connection = 'Measurement stopped due\nto slow connection!\n\n'
        else:
            slow_connection = ''

        if self.ADC_reset:
            ADC_reset = ('Measurement stopped due to\nto ADC reset! Please make sure\n' +
                         'that the ADC is connected and\nrun "adc reset"\n\n')
        else:
            ADC_reset = ''

        if self.pga == 0xFF:
            pga = 'bypassed'
        else:
            pga = pga_reverse_lookup[self.pga]

        cal_offset_diff = '{0:.2f} nV'.format(self.cal_offset_diff_nv)

        if abs(self.cal_scale_diff) > 0.01:
            cal_scale_diff = '{0:.2f} %'.format(self.cal_scale_diff * 100)
        else:
            cal_scale_diff = '{0:.2f} ppm'.format(self.cal_scale_diff * 1000000)

        msg = ('state: {}\ninternal_reference: {}\n{}{}' +
               'samplerate: {} SPS\nfilter: {}\n' +
               'pga: {}\nv_ref: {}nV\nv_ref_inputs: {} {}\n' +
               'cal offset: {}\n  (diff: {})\ncal scale: {}\n  (diff: {})\n' +
               'measurements: {}').format(
                        self.started, self.internal_reference, slow_connection, ADC_reset,
                        self.samplerate, self.filter, pga, self.v_ref, self.v_ref_pos, self.v_ref_neg,
                        self.calibration_offset, cal_offset_diff, self.calibration_scale,
                        cal_scale_diff, self.measurement_count)

        # Add measurements
        if self.measurement_count > 0:
            measurements = self.measurements.values()
            measurements = sorted(measurements, key=lambda m: m.get_id())
            measurement_strings = []
            for m in measurements:
                measurement_strings.append(str(m))
            msg += '\n\n' + '\n'.join(measurement_strings)
        return msg
