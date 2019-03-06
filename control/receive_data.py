import socket
import struct
import threading
import time

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D

from manager.base import CONNECTION_TYPE_DATA, PACKAGE_TYPE_DATA, base


class DataBuffer():
    """
    Represents a synchronized buffer where one can write data
    and the other part can read it. After reading this buffer is flushed.
    """
    def __init__(self):
        self._reset_data()
        self.lock = threading.Lock()

    def _reset_data(self):
        # holds data per measurement.
        self.xdata = {}
        self.ydata = {}

    def add_data(self, id, x, y):
        self.lock.acquire()
        if id not in self.xdata:
            self.xdata[id] = np.array([])
            self.ydata[id] = np.array([])
        # append data to the right measurement.
        self.xdata[id] = np.append(self.xdata[id], x)
        self.ydata[id] = np.append(self.ydata[id], y)
        self.lock.release()

    def get_and_flush_data(self):
        self.lock.acquire()
        xdata = {}  # used for returning the values
        ydata = {}

        # Copy the data
        for key, val in self.xdata.items():
            xdata[key] = np.array(val, copy=True)
        for key, val in self.ydata.items():
            ydata[key] = np.array(val, copy=True)
        if len(xdata.keys()) > 0:
            self._reset_data()
        self.lock.release()
        return xdata, ydata


class PlotUpdateThread(threading.Thread):
    """ Take care about updating the plot as new data comes in. """
    colors = ['blue', 'green', 'red', 'cyan', 'magenta', 'yellow', 'black', 'white']

    def __init__(self, data_buffer, plot, fig, ax, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.data_buffer = data_buffer
        self.plot = plot
        self.fig = fig
        self.ax = ax
        self.first_timestamp = None  # saves the first timestamp ever seen. Used to
        # set all measurements relativ to this timestamp, so the first datapoint is
        # on t=0.
        self.id_index_map = {}  # maps the measurement id to an line index.
        # Each dataset gets an own "line" in matplotlib. These lines lay in
        # in the array self.ax.lines. Each measurement corresponds to one line.

    def run(self):
        while True:
            xdata, ydata = self.data_buffer.get_and_flush_data()  # get new data.
            if len(xdata.keys()) > 0:  # there is some data
                if self.first_timestamp is None:
                    # Find the minimal x value of all measurements
                    self.first_timestamp = xdata[list(xdata.keys())[0]][0]
                    for id in xdata.keys():
                        if xdata[id][0] < self.first_timestamp:
                            self.first_timestamp = xdata[id][0]

                # update each measurement
                for id in xdata.keys():
                    if len(xdata[id]) > 0:
                        self.update_for_id(id, xdata[id], ydata[id])

                # trigger an update for the graph
                self.ax.relim()
                self.ax.autoscale_view()
                self.fig.canvas.draw()
                self.fig.canvas.flush_events()
            time.sleep(1)

    def update_for_id(self, id, xdata, ydata):
        """ Takes data for one measurement to add to the graph """
        # convert tennanovolt (10^-8) to volt
        ydata = [y/100000000.0 for y in ydata]
        # offset the timestamps and scale from ms to s
        xdata = [(x-self.first_timestamp)/1000.0 for x in xdata]

        if id not in self.id_index_map:
            # This measurement does not have a line. Create on.
            line_index = len(self.ax.lines)
            line = Line2D(xdata, ydata, color=self.colors[line_index])
            self.ax.add_line(line)
            self.id_index_map[id] = line_index
            return

        # Update xdata
        index = self.id_index_map[id]
        line = self.ax.lines[index]
        line.set_xdata(np.append(line.get_xdata(), xdata))
        line.set_ydata(np.append(line.get_ydata(), ydata))


class DataThread(threading.Thread):
    """
    The Thread recieveing data. Passes the data to a PlotThread instance
    """

    data_max_freq = 100
    """
    If you use a high samplerate, there might be too much data send to handle
    by pyplot. Set this parameter to a frequency in which datapoints are taken.
    E.g.: If you sample with 2000 Hz and set this param to 100 Hz, every 20th sample
    is used, or all other 19 are dropped. Note that this may falsify your graph!
    If this is an issue, collect this data into a file and visualize the results later on.
    Set this to None to disable this feature.
    """

    meta_info_size = 8  # Size of the metainfo of each data packet.

    def __init__(self, connection, plot, fig, ax, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.data_buffer = DataBuffer()  # This is used to synchronize the data between this
        # and the plot thread
        self.plot_thread = PlotUpdateThread(self.data_buffer, plot, fig, ax, daemon=True)
        self.last_time_stamp = {}  # Save the last timestamp seen per channel. Used for the
        # data_max_freq feature
        self.last_status = {}  # Saves the last status per channel.

        if self.data_max_freq is not None:
            self.data_min_time_delta = 1000 / self.data_max_freq  # This delta is given in ms.

    def run(self):
        self.plot_thread.start()
        try:
            self.recieve()
        except socket.error as err:
            print('Socketerror: {}'.format(err))
            exit(3)

    def recieve(self):
        """ Collects packets with data. Passes full packets to input() """
        buff = b''
        while True:
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

    def input(self, buff):
        """ Takes the content of one package and process it. """
        ref, = struct.unpack('<Q', buff[0:self.meta_info_size])  # get the timereference

        data_len = len(buff)-self.meta_info_size
        # iterate over all values given
        for i in range(data_len // 7):  # Each measurements needs 7 bytes
            # extract the measurement_id, the actual value and the time difference to the timereference
            id_and_status, value, timestamp_delta = struct.unpack(
                '<BiH',
                buff[i*7 + self.meta_info_size: (i+1)*7 + self.meta_info_size])
            timestamp = (timestamp_delta + ref)/100  # in ms.

            id = id_and_status & 0x07
            status = (id_and_status & 0xF8) >> 3
            self.handle_status(id, status)

            if self.data_max_freq is None:
                self.data_buffer.add_data(id, timestamp, value)  # add this to the value buffer
            else:
                if (id not in self.last_time_stamp
                        or timestamp > (self.last_time_stamp[id] + self.data_min_time_delta)):
                    # print(id, value/100000000.0, timestamp)
                    self.last_time_stamp[id] = timestamp
                    self.data_buffer.add_data(id, timestamp, value)  # Finally, add this to the value buffer

    def handle_status(self, id, status):
        if id not in self.last_status:
            self.last_status[id] = status
            self.print_status(id, status)
        elif status != self.last_status[id]:
                self.last_status[id] = status
                self.print_status(id, status)

    def print_status(self, id, status):
        alarms_set = status & 0x0F
        msg = ''
        if alarms_set == 0:
            msg += 'Measurement {} no alarms set.'.format(id)
        else:
            alarms = ''
            alarm_count = 0
            if status & 0x01:
                alarms += 'differential, '
                alarm_count += 1
            if status & 0x02:
                alarms += 'high, '
                alarm_count += 1
            if status & 0x04:
                alarms += 'low, '
                alarm_count += 1
            if status & 0x08:
                alarms += 'reference, '
                alarm_count += 1
            alarms = alarms[:-2]
            msg += 'Measurement {} '.format(id)
            if alarm_count == 1:
                msg += 'alarm: '
            else:
                msg += 'alarms: '
            msg += alarms + '. Clock: '
        if (status & 0x10) == 0:
            msg += 'Intern'
        else:
            msg += 'Extern'
        print(msg)


def main(connection):
    # Create a plot with one figure.
    fig, ax = plt.subplots()
    plot = plt.plot()
    ax.set_autoscaley_on(True)
    ax.set_autoscalex_on(True)
    ax.grid()

    # Start the thread to recieve data. This thread will also control
    # the plot thread to display the data.
    dataThread = DataThread(connection, plot, fig, ax, daemon=True)
    dataThread.start()

    plt.title('Data')
    plt.xlabel('t in s')
    plt.ylabel('U in V')
    plt.show()

    connection.close()


if __name__ == '__main__':
    base(main, connection_type=CONNECTION_TYPE_DATA)   # get a data connection
