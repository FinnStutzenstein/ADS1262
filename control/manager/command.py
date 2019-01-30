import json
import sys

from .commands import *  # noqa


class CommandManager:
    """ Manages all commands. """

    def __init__(self, main, filename='protocol.json'):
        """ Reads the given json file and parses the commands. """
        self.main = main
        with open(filename, 'r') as f:
            self.commands = self.init_commands(json.load(f))

    def init_commands(self, commands_dict):
        """ Init commands form the json dict. Returns an array of commands. """
        commands_module = sys.modules['manager.commands']

        commands = []  # create a list of all commands
        # iterate through the double-dict structure giving the prefix- and
        # command bytes.
        for prefix, command_group in commands_dict.items():
            for command, command_dict in command_group.items():
                # Just register a command, if it is shown.
                if command_dict.get('show', True):

                    # Try to get the commands class. If the class is not found, use
                    # the RemoteCommand class as default.
                    cls_name = self.command_name_to_class_name(command_dict['command'])
                    command_cls = getattr(commands_module, cls_name, None)
                    if command_cls is None:
                        command_cls = getattr(commands_module, 'RemoteCommand', None)
                        if command_cls is None:
                            raise ValueError('Class {} was not found.'.format(cls_name))

                    # Instantiate the command.
                    commands.append(command_cls(prefix, command, command_dict, self.main))

        # add dummy commands, so the autocompletion works there.
        commands.append(QuitCommand('quit'))  # noqa
        commands.append(HelpCommand('help'))  # noqa
        commands.append(ListCommand('list'))  # noqa
        return commands

    def command_name_to_class_name(self, command_name):
        """
        Converts the command to a class name by making it CamelCase and
        appending 'Command'
        """
        name_parts = command_name.split()
        c = ''.join([p[0].upper() + p[1:] for p in name_parts])
        return c + 'Command'

    def get_command(self, command_name):
        """ Takes a string and checks if it's a command. Even with args given, the command is returned. """
        for command in self.commands:
            if command_name.startswith(command.command_name):
                return command
        return None

    def get_all_commands(self):
        """ Returns all commands sorted by their name. """
        return sorted(self.commands, key=lambda c: c.command_name)

    def get_commands_for_autocompletion(self, name_prefix):
        """
        Returns a tuple: first a list (may be empty) of commands, that have the name_prefix as
        prefix. The second entry is the common part of all found commands after the prefix.
        """
        found_commands = []
        for command in self.commands:
            if command.command_name.startswith(name_prefix):
                found_commands.append(command)

        if len(found_commands) == 0:
            return ([], '')

        common_name_part = found_commands[0].command_name[len(name_prefix):]
        for command in found_commands:
            name = command.command_name[len(name_prefix):]
            # check every character.
            for i in range(len(common_name_part)):
                if common_name_part[i] != name[i]:
                    common_name_part = common_name_part[0:i]
                    break

        return (found_commands, common_name_part)
