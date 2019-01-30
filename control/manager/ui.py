import curses
from threading import Lock


class UI:
    def __init__(self, stdscr, command_manager):
        self.command_manager = command_manager
        curses.use_default_colors()
        for i in range(0, curses.COLORS):
            curses.init_pair(i, i, -1)
        self.stdscr = stdscr
        self.inputbuffer = ''
        self.inputhistory = []
        self.linebuffer = []
        self.logbuffer = []
        self.cmd_addons = []
        self.status = None

        self.sigint_flag = False

        self.statusside_width = 32
        self.cmdline_height = 1

        height_without_cmdline = curses.LINES - 4 - self.cmdline_height
        statusside_hwyx = (height_without_cmdline, self.statusside_width, 0, 0)
        log_hwyx = (height_without_cmdline, curses.COLS - self.statusside_width - 3,
                    0, self.statusside_width + 3)
        cmdline_hwyx = (self.cmdline_height, curses.COLS - 2, curses.LINES - 1 - self.cmdline_height, 1)

        self.win_statusside = curses.newwin(*statusside_hwyx)
        self.win_cmdline = curses.newwin(*cmdline_hwyx)
        self.win_cmdline.timeout(1)
        self.win_log = curses.newwin(*log_hwyx)

        self.lock = Lock()

    def resize(self, lock=True):
        """Handles a change in terminal size"""
        if lock:
            self.lock.acquire()

        h, w = self.stdscr.getmaxyx()

        if w < self.statusside_width + 7:
            return

        height_without_cmdline = h - 4 - self.cmdline_height
        statusside_hwyx = (height_without_cmdline, self.statusside_width, 0, 0)
        log_hwyx = (height_without_cmdline, w - self.statusside_width - 3,
                    0, self.statusside_width + 3)
        cmdline_hwyx = (self.cmdline_height, w - 2, h - 1 - self.cmdline_height, 1)

        if self.win_statusside:
            del self.win_statusside
        self.win_statusside = curses.newwin(*statusside_hwyx)

        if self.win_cmdline:
            del self.win_cmdline
        self.win_cmdline = curses.newwin(*cmdline_hwyx)
        self.win_cmdline.timeout(1)

        if self.win_log:
            del self.win_log
        self.win_log = curses.newwin(*log_hwyx)

        self.linebuffer = []
        for msg in self.logbuffer:
            self._linebuffer_add(msg)

        self.redraw_ui(lock=False)

        if lock:
            self.lock.release()

    def redraw_ui(self, lock=True):
        """Redraws the entire UI"""
        if lock:
            self.lock.acquire()

        h, w = self.stdscr.getmaxyx()
        self.stdscr.clear()
        self.stdscr.vline(0, self.statusside_width + 1, "|", h - 3 - self.cmdline_height)
        self.stdscr.hline(h - 3 - self.cmdline_height, 0, "-", w)
        self.stdscr.refresh()

        self.redraw_statusside(lock=False)
        self.redraw_log(lock=False)
        self.redraw_cmdline(lock=False)

        if lock:
            self.lock.release()

    def redraw_cmdline(self, lock=True):
        """Redraw the user input textbox"""
        if lock:
            self.lock.acquire()
        h, w = self.win_cmdline.getmaxyx()
        self.win_cmdline.clear()
        for i, text in enumerate(self.cmd_addons):
            self.win_cmdline.addstr(i + 1, 0, text)
        start = len(self.inputbuffer) - w + 1
        if start < 0:
            start = 0
        self.win_cmdline.addstr(0, 0, self.inputbuffer[start:])
        self.win_cmdline.refresh()
        if lock:
            self.lock.release()

    def redraw_statusside(self, lock=True):
        """Redraw the statusside"""
        if lock:
            self.lock.acquire()
        self.win_statusside.clear()
        if self.status:
            for i, text in enumerate(self.status.split('\n')):
                self.win_statusside.addstr(i, 0, text)
        self.win_statusside.refresh()
        if lock:
            self.lock.release()

    def redraw_log(self, lock=True):
        """Redraw the chat message buffer"""
        if lock:
            self.lock.acquire()
        self.win_log.clear()
        h, w = self.win_log.getmaxyx()
        j = len(self.linebuffer) - h
        if j < 0:
            j = 0
        for i in range(min(h, len(self.linebuffer))):
            self.win_log.addstr(i, 0, self.linebuffer[j])
            j += 1
        self.win_log.refresh()
        if lock:
            self.lock.release()

    def update_state(self, state):
        self.status = 'State\n\n' + str(state)
        self.redraw_ui()
        self.win_cmdline.cursyncup()

    def print(self, msg):
        """
        Add a message to the chat buffer, automatically slicing it to
        fit the width of the buffer
        """
        lines = msg.split('\n')
        for line in lines:
            self.logbuffer.append(line)
            self._linebuffer_add(line)
        self.redraw_log()
        self.redraw_cmdline()
        self.win_cmdline.cursyncup()

    def _linebuffer_add(self, msg):
        h, w = self.stdscr.getmaxyx()
        s_h, s_w = self.win_statusside.getmaxyx()
        w = w - s_w - 2
        if len(msg) == 0:
            self.linebuffer.append('')
            return

        while len(msg) >= w:
            self.linebuffer.append(msg[:w])
            msg = msg[w:]
        if msg:
            self.linebuffer.append(msg)

    def _set_cmd_addons(self, addons):
        self.cmd_addons = addons
        self.cmdline_height = len(addons) + 1
        self.resize()

    def wait_input(self, prompt=''):
        """
        Wait for the user to input a message and hit enter.
        Returns the message
        """
        input = ''
        history_index = len(self.inputhistory)
        history_user_input_save = None

        self.inputbuffer = prompt
        self.redraw_cmdline()
        self.win_cmdline.cursyncup()

        cursor_pos = 0

        self.sigint_flag = False

        char_input = -1
        while char_input != ord('\n'):
            # idk why the win_cmdline.move does not work..
            self.stdscr.move(curses.LINES - 1 - self.cmdline_height, 1 + len(prompt) + cursor_pos)
            char_input = self.stdscr.getch()

            if self.sigint_flag:
                self.sigint_flag = False
                return ''

            if not char_input:
                continue

            if len(self.cmd_addons) > 0:
                self._set_cmd_addons([])

            if char_input == ord('\n'):
                self.inputbuffer = ''
                self.redraw_cmdline()
                self.win_cmdline.cursyncup()
                if len(input) > 0:
                    self.inputhistory.append(input)
                return input

            elif char_input == curses.KEY_BACKSPACE or char_input == 127:
                if len(input) > 0 and cursor_pos > 0:
                    input = input[0:cursor_pos-1] + input[cursor_pos:]
                    cursor_pos -= 1

            elif char_input == curses.KEY_DC:  # entf-button
                if len(input) > 0 and cursor_pos < len(input):
                    input = input[0:cursor_pos] + input[cursor_pos+1:]

            elif char_input == curses.KEY_RESIZE:
                self.resize()

            elif char_input == curses.KEY_UP:
                if history_index == len(self.inputhistory):
                    history_user_input_save = input

                if history_index > 0:
                    history_index -= 1
                    input = self.inputhistory[history_index]
                    cursor_pos = len(input)

            elif char_input == curses.KEY_DOWN:
                if history_index < (len(self.inputhistory)-1):
                    history_index += 1
                    input = self.inputhistory[history_index]
                    cursor_pos = len(input)
                elif history_user_input_save is not None:
                    history_index += 1
                    input = history_user_input_save
                    history_user_input_save = None
                    cursor_pos = len(input)

            elif char_input == curses.KEY_LEFT:
                if cursor_pos > 0:
                    cursor_pos -= 1

            elif char_input == curses.KEY_RIGHT:
                if cursor_pos < len(input):
                    cursor_pos += 1

            elif char_input == curses.KEY_HOME:  # pos1-button
                cursor_pos = 0

            elif char_input == curses.KEY_END:  # ende-button
                cursor_pos = len(input)

            elif char_input == ord('\t'):
                commands, common_part = self.command_manager.get_commands_for_autocompletion(input)
                if len(commands) > 1:
                    suggestions = []
                    for c in commands:
                        suggestions.append(c.command_name)
                    self._set_cmd_addons(suggestions)
                input += common_part
                if len(commands) == 1:
                    input += ' '
                cursor_pos = len(input)

            elif 32 <= char_input <= 126:
                input = input[0:cursor_pos] + chr(char_input) + input[cursor_pos:]
                cursor_pos += 1

            else:
                self.print("unknown keystroke: {} {}".format(int(char_input), chr(char_input)))

            self.inputbuffer = prompt + input
            self.redraw_cmdline()
            self.win_cmdline.cursyncup()
            self.win_cmdline.move(0, 1)
            curses.doupdate()
            self.win_cmdline.refresh()

    def handle_sigint(self):
        self.sigint_flag = True
