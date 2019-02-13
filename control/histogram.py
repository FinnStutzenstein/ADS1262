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
    Synchronizes data between threads.
    """
    def __init__(self):
        self._reset_data()
        self.lock = threading.Lock()

    def _reset_data(self):
        self.xdata = np.array([])

    def add_data(self, x):
        """ Adds the data (a single value or array) to the buffer """
        self.lock.acquire()
        self.xdata = np.append(self.xdata, x)
        self.lock.release()

    def get_and_flush_data(self):
        """ Returns all the data """
        self.lock.acquire()
        xdata = np.array(self.xdata, copy=True)
        self._reset_data()
        self.lock.release()
        return xdata


class PlotUpdateThread(threading.Thread):
    """ Take care about updating the plot as new data comes in. """

    def __init__(self, data_buffer, plot, fig, ax, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.data_buffer = data_buffer
        self.plot = plot
        self.fig = fig
        self.ax = ax

        # All data every seen.
        self.data = np.array([])

    def run(self):
        while True:
            xdata = self.data_buffer.get_and_flush_data()  # get new data.
            if len(xdata) > 0:  # there is some data
                # Add to our databuffer
                self.data = np.append(self.data, xdata)

                # Calc statistics.
                N = len(self.data)
                mean = np.mean(self.data)
                std = np.std(self.data)

                # Format stats and fancy-print them
                format_string = "{:5.6f}"
                mean = format_string.format(mean).ljust(14)
                std = format_string.format(std).ljust(14)
                N = str(N).ljust(10)
                text = "N: {} Mittelwert: {} Standardabweichung: {}".format(N, mean, std).strip()
                print('\r'+ text, end='')

                # Update the plot. Hist has no nice functionality to update, so we need to
                # clear the whole graph and redraw it.
                self.ax.cla()
                self.ax.hist(self.data, bins=100)
                self.ax.set_xlabel('$\mu V$')
                self.ax.set_ylabel('$N$')
                self.ax.grid()

                # trigger an update for the graph
                self.fig.canvas.draw()
                self.fig.canvas.flush_events()
            time.sleep(1)


class DataThread(threading.Thread):
    """
    The Thread recieveing data. Passes the data to a PlotThread instance
    """

    meta_info_size = 8  # Size of the metainfo of each data packet.

    def __init__(self, connection, plot, fig, ax, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.data_buffer = DataBuffer()  # This is used to synchronize the data between this
        # and the plot thread
        self.plot_thread = PlotUpdateThread(self.data_buffer, plot, fig, ax, daemon=True)

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

            value /= 100.0  # in mikrovolt
            # value /= 1000.0  # in millivolt
            self.data_buffer.add_data(value)


def main(connection):
    # Create a plot with one figure.
    fig, ax = plt.subplots()
    plot = plt.plot()
    ax.grid()

    # Start the thread to recieve data. This thread will also control
    # the plot thread to display the data.
    dataThread = DataThread(connection, plot, fig, ax, daemon=True)
    dataThread.start()

    plt.show()
    print('')

    connection.close()


if __name__ == '__main__':
    base(main, connection_type=CONNECTION_TYPE_DATA)   # get a data connection
