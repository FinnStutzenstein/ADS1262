import socket
import struct
import threading
import time

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D

from manager.base import CONNECTION_TYPE_FFT, PACKAGE_TYPE_FFT, base


class DataBuffer():
    def __init__(self):
        self._reset_data()
        self.lock = threading.Lock()

    def _reset_data(self):
        self.flushed_data = np.array([])
        self.frame_data = np.array([])
        self.length = 0
        self.resolution = 0
        self.wss = 0

    def reset_frame_data(self):
        self.lock.acquire()
        self.frame_data = np.array([])
        self.lock.release()

    def add_frame_data(self, data):
        self.lock.acquire()
        self.frame_data = np.append(self.frame_data, data)
        self.lock.release()

    def flush(self, length, resolution, wss):
        self.lock.acquire()
        self.flushed_data = np.array(self.frame_data, copy=True)
        self.frame_data = np.array([])
        self.length = length
        self.resolution = resolution
        self.wss = wss
        self.lock.release()

    def get_data(self):
        self.lock.acquire()
        data = self.flushed_data
        self.flushed_data = np.array([])
        self.lock.release()
        return self.length, self.resolution, self.wss, data


class PlotUpdateThread(threading.Thread):
    def __init__(self, data_buffer, plot, fig, axes, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.data_buffer = data_buffer
        self.plot = plot
        self.fig = fig
        self.axes = axes
        self.fft_line = Line2D([], [])
        self.axes[0].add_line(self.fft_line)
        self.axes[0].set_xscale('log')
        self.axes[0].set_title('FFT')
        self.axes[0].set_ylabel('$A$')
        self.axes[0].set_xlabel('Frequenz')
        self.psd_line = Line2D([], [])
        self.axes[1].add_line(self.psd_line)
        self.axes[1].set_xscale('log')
        self.axes[1].set_title('PSE')
        self.axes[1].set_ylabel('$V^2s$ (in $dB$)')
        self.axes[1].set_xlabel('Frequenz')

        # Use both to check, if some configuration has changed.
        self.last_wss = None
        self.last_N = None

    def run(self):
        while True:
            length, resolution, wss, data = self.data_buffer.get_data()
            if len(data) > 0:
                self.update(length, resolution, wss, data)

                for ax in self.axes:
                    ax.relim()
                    ax.autoscale_view()
                self.fig.canvas.draw()
                self.fig.canvas.flush_events()
            time.sleep(0.5)

    def update(self, N, resolution, wss, fft):
        if self.last_wss != wss or self.last_N != N:
            self.reset_averaging(N)
            self.last_wss = wss
            self.last_N = N

        self.recieved_ffts += 1

        N_half = N//2
        fft = np.append(fft, np.imag(fft[0]))
        fft[0] = np.real(fft[0])
        fft = np.absolute(fft)
        self.fft = self.fft + fft
        fft = self.fft / self.recieved_ffts

        # PSD
        psd = 2.0*np.square(fft)/(resolution*N*wss)
        psd[0] = psd[0]/2.0
        psd[N_half] = psd[N_half]/2.0
        self.psd = self.psd + psd
        psd = self.psd / self.recieved_ffts

        # x axis
        x_fft = np.linspace(0, N_half*resolution, num=N_half+1)

        # Plot it
        self.fft_line.set_xdata(x_fft[1:])
        self.fft_line.set_ydata(fft[1:])
        self.psd_line.set_xdata(x_fft)
        self.psd_line.set_ydata(10*np.log10(psd))

        # Print samplerate, N and resolution
        self.axes[0].set_xlabel('Frequenz ($f_s={0:.2f}$, $N={1}$, $res={2:.2f}$)'.format(
            N*resolution, N, resolution))

    def reset_averaging(self, N):
        self.recieved_ffts = 0
        self.fft = np.zeros(N//2 + 1)
        self.psd = np.zeros(N//2 + 1)


class DataThread(threading.Thread):
    metadata_size = 21

    def __init__(self, connection, plot, fig, ax, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connection = connection
        self.data_buffer = DataBuffer()
        self.plot_thread = PlotUpdateThread(self.data_buffer, plot, fig, ax, daemon=True)
        self.last_frame_number = -1
        self.last_bytes = b''

    def run(self):
        self.plot_thread.start()
        try:
            self.recieve()
        except socket.error as err:
            print('Socketerror: {}'.format(err))
            exit(3)

    def recieve(self):
        buff = b''
        while True:
            if len(buff) < 3:
                buff += self.connection.recv(3)

            package_type, package_len = struct.unpack('<BH', buff[0:3])
            buff = buff[3:]

            while len(buff) < package_len:
                buff += self.connection.recv(package_len)

            if package_type == PACKAGE_TYPE_FFT:
                self.input(buff[0: package_len])
            else:
                print("Wrong package recieved: {}".format(package_type))

            # remove full packages from buffer:
            buff = buff[package_len:]

    def input(self, buff):
        id, frame_count, frame_number, length, timestamp, resolution, wss = struct.unpack('<BBBHQff', buff[0:self.metadata_size])

        print("got data: frame {}/{}".format(frame_number+1, frame_count))
        if self.last_frame_number+1 != frame_number:
            print("Drop")
            self.reset_frame_data()
            if frame_number != 0:
                return
                # if we somehow got into a transmission of frames (e.g. starting
                # by the second one), skip this.
        self.last_frame_number = frame_number

        # add last bytes to the buffer
        buff = self.last_bytes + buff[self.metadata_size:]
        data_len = len(buff)
        data = np.array([])
        for i in range(data_len // 8):
            re, im = struct.unpack(
                '<ff',
                buff[i*8: (i+1)*8])
            value = re + 1j*im
            data = np.append(data, value)

        # If more frames are send, the bytes are splitted, not fully complex numbers.
        # Store the bytes for the next frame. There should be no remainder on the
        # last frame (checkd below)
        remainder = data_len % 8
        if remainder != 0:
            self.last_bytes = buff[-remainder:]
        else:
            self.last_bytes = b''

        self.data_buffer.add_frame_data(data)
        if frame_number+1 == frame_count:
            if remainder != 0:
                print('Error! Some bytes left after all frames')
                self.reset_frame_data()
            else:
                self.data_buffer.flush(length, resolution, wss)
                self.last_frame_number = -1

    def reset_frame_data(self):
        self.data_buffer.reset_frame_data()
        self.last_frame_number = -1
        self.last_bytes = b''


def main(connection):
    fig, axes = plt.subplots(nrows=2)
    plot = plt.plot()
    for ax in axes:
        ax.set_autoscaley_on(True)
        ax.set_autoscalex_on(True)
        ax.grid()
    dataThread = DataThread(connection, plot, fig, axes, daemon=True)
    dataThread.start()

    plt.show()
    connection.close()


if __name__ == '__main__':
    base(main, connection_type=CONNECTION_TYPE_FFT)
