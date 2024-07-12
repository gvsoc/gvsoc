"""Provides target class common to all targets which brings common methods and commands."""

#
# Copyright (C) 2022 GreenWaves Technologies
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
#



import traceback
import argparse
import inspect
import os
import importlib
import json
from collections import OrderedDict
import sys
import gapylib.flash
from prettytable import PrettyTable
import rich.table


def get_target(target: str) -> 'Target':
    """Returns the class implementing the support for the specified target.

    The target is specified as a python module which is imported from python path.
    It returns the class 'Target' from the imported module.

    Parameters
    ----------
    target : str
        Name of the target. The name must corresponds to a python module.

    Returns
    -------
    class
        The class 'Target' of the imported module.
    """
    try:
        module = importlib.import_module(target)


    except ModuleNotFoundError as exc:
        if exc.name == target:
            raise RuntimeError(f'Invalid target specified: {target}') from exc

        raise RuntimeError(f"Dependency '{exc.name}' of the target module '{target}' is"
            " missing (add --py-stack for more information).") from exc

    if 'Target' in dir(module):

        target_class = getattr(module, 'Target', None)

        # Check that the class comes from the module itself and not from an imported one
        if inspect.isclass(target_class) and target_class.__module__ == module.__name__:

            return target_class

    raise RuntimeError(f'Could not find any Gapy Target class in target: {target}')



class Property():
    """
    Placeholder for target properties.

    Attributes
    ----------
    name : str
        Name of the property.
    path : str
        Path of the property in the target hierarchy.
    value : any
        Value of the property.
    description : str
        Description of the property.
    path : str
        Path in the target of the property.
    cast : type
        When the property is overwritten from command-line, cast it to the specified type.
    dump_format : str
        When the property is dumped, dump it wth the specified format
    allowed_values : list
        List of allowed values. If set to None, anything is allowed.
    """

    def __init__(self, name: str, value: any, description: str, path: str=None,
            allowed_values: list=None, cast: type=None, dump_format: str=None):

        self.name = name
        self.path = path
        if path is not None:
            self.full_name = path + '/' + name
        else:
            self.full_name = name
        self.description = description
        self.value = value
        self.allowed_values = allowed_values
        self.cast = cast
        self.format = dump_format


class Target():
    """
    Parent class for all targets, which provides all common functions and commands.

    Attributes
    ----------
    parser : argparse.ArgumentParser
        The parser where to add arguments.
    options : List
        A list of options for the target.
    """

    gapy_description = "Generic Gapy target"
    is_gapy_target = True

    def __init__(self, parser, options: list=None):

        if parser is not None:
            parser.add_argument("--flash-property", dest="flash_properties", default=[],
                action="append", help="specify the value of a flash property")

            parser.add_argument("--flash-content", dest="flash_contents", default=[],
                action="append",
                help="specify the path to the JSON file describing the content of the flash")

            parser.add_argument("--multi-flash-content", dest="multi_flash_content", default=[],
                action="append",
                help="specify the path to the JSON file describing the content multiple flashes")

            parser.add_argument("--flash-layout-level", dest="layout_level", type=int, default=2,
                help="specify the level of the layout when dumping flash layout")

            parser.add_argument("--flash-no-auto", dest="flash_auto", action="store_false",
                help="Flash auto-mode, will force the flash content update only if needed")

            parser.add_argument("--binary", dest = "binary", default = None,
                help = "Binary to execute on the target")

            parser.add_argument("--debug-binary", dest = "debug_binary", action="append",
                default = None, help = "Additional binaries to be used for debug symbols in traces")

            parser.add_argument("--flash-property-override", dest = "flash_override", default = [],
                action="append",
                help = "Handle to override flash property")

            parser.add_argument("--ota-sign-key", dest = "pem_path", default = None,
                help = "Specify a key to sign OTA payload")

            parser.add_argument("--ota-sign-dgst", dest = "sign_dgst", default = 'sha256',
                help = "Specify digest for OTA signing (see openssl doc for details)")


        self.commands = [
            ['commands'    , 'Show the list of available commands'],
            ['targets'     , 'Show the list of available targets'],
            ['image'       , 'Generate the target images needed to run execution'],
            ['flash'       , 'Upload the flash contents to the target'],
            ['flash_layout', 'Dump the layout of the flashes'],
            ['flash_dump_sections', 'Dump each section of each flash memory'],
            ['flash_dump_app_sections', 'Dump each section of each flash memory'],
            ['flash_properties', 'Dump the value of all flash section properties'],
            ['target_properties', 'Dump the value of all target properties'],
            ['run'         , 'Start execution on the target'],
        ]

        [args, _] = parser.parse_known_args()
        self.target_dirs = []
        self.flashes = {}
        self.work_dir = args.work_dir
        self.options = options
        self.layout_level = 0
        self.pem_path = None
        self.sign_dgst = None
        self.args = None
        self.target_properties = {}
        self.args_properties = {}
        self.parser = parser
        self.target_properties_parsed = False
        self.command_handlers = []


    def get_abspath(self, relpath: str) -> str:
        """Return the absolute path depending on the working directory.

        If no working directory was specified, the relpath is appended to the current directory.
        Otherwise it is appended to the working directory.

        Parameters
        ----------
        relpath : str
            Relative path of the file.

        Returns
        -------
        str
            The absolute path.
        """
        if os.path.isabs(relpath):
            return relpath

        if self.work_dir is None:
            return os.path.abspath(relpath)

        return os.path.join(self.work_dir, relpath)



    def register_flash(self, flash: gapylib.flash.Flash):
        """Register a flash.

        The flash should inherit from gapylib.flash.Flash.
        This will allow gapy to produce images for this flash.

        Parameters
        ----------
        flash : gapylib.flash.Flash
            The flash to be registered.
        """
        self.flashes[flash.get_name()] = flash



    def get_args(self) -> argparse.ArgumentParser:
        """Return the command-line arguments.

        Returns
        -------
        argparse.ArgumentParser
            The arguments.
        """

        return self.args


    @staticmethod
    def get_file_path(relpath: str) -> str:
        """Return the absolute path of a file.

        Search into the python path for this file and returns its absolute path.
        This can be used to find configuration files next to python modules.

        Parameters
        ----------
        relpath: str
            The file relative path to look for in the python path

        Returns
        -------
        str
            The path.
        """
        for dirpath in sys.path:
            path = os.path.join(dirpath, relpath)
            if os.path.exists(path):
                return path

        return None

    def handle_command_image(self):
        """Handle the image command.

        This can be called if a class is overloading the image command, in order to still
        execute the generic part of this command.
        """
        for flash in self.flashes.values():
            if not flash.is_empty():
                sections = flash.get_sections()

                first_index = 0
                index_to_last= 0
                while sections[-1 - index_to_last].is_empty():
                    index_to_last+=1

                flash.dump_image(first_index, len(sections)-1-index_to_last)

    def handle_command(self, cmd: str):
        """Handle a command.

        This should be called only by gapy executable but can be overloaded by the real targets
        in order to add commands or to handle existing commands differently.

        Parameters
        ----------
        cmd : str
            Command name to be handled.
        """

        # First try to execute the command from registered handlers and leave if it handled
        for handler in self.command_handlers:
            if handler(cmd):
                return

        if cmd == 'commands':
            self.__print_available_commands()

        elif cmd == 'targets':
            self.__display_targets()

        elif cmd == 'flash_layout':
            for flash in self.flashes.values():
                flash.dump_layout(level=self.layout_level)

        elif cmd == 'flash_dump_sections':
            for flash in self.flashes.values():
                flash.dump_sections(pem_path = self.pem_path, sign_dgst=self.sign_dgst)

        elif cmd == 'flash_dump_app_sections':
            for flash in self.flashes.values():
                flash.dump_app_sections(pem_path = self.pem_path, sign_dgst=self.sign_dgst)

        elif cmd == 'flash_properties':
            for flash in self.flashes.values():
                flash.dump_section_properties()

        elif cmd == 'target_properties':
            self.dump_target_properties()

        elif cmd == 'image':
            self.handle_command_image()

        elif cmd == 'flash':
            pass

        else:
            raise RuntimeError('Invalid command: ' + cmd)

    def __print_available_commands(self):
        print('Available commands:')

        for command in self.commands:
            print(f'  {command[0]:16s} {command[1]}')


    def append_args(self, parser: argparse.ArgumentParser):
        """Append target specific arguments to gapy command-line.

        This should be called only by gapy executable to register some arguments handled by the
        default target but it can be overloaded in order to add arguments which should only be
        visible if the target is active.

        Parameters
        ----------
        parser : argparse.ArgumentParser
            The parser where to add arguments.
        """

    def declare_target_property(self, descriptor: Property):
        """Declare a target property.

        Target properties are used to configure the behavior of the target. They must be
        declared with this method before they can be overwritten from command-line target
        properties.

        Parameters
        ----------
        descriptor : Property
            The descriptor of the property
        """

        self.parse_target_properties()

        if self.target_properties.get(descriptor.full_name) is not None:
            traceback.print_stack()
            raise RuntimeError(f'Property {descriptor.full_name} already declared')

        self.target_properties[descriptor.full_name] = descriptor

        arg = self.args_properties.get(descriptor.full_name)
        if arg is not None:
            if descriptor.allowed_values is not None:
                if arg not in descriptor.allowed_values:
                    raise RuntimeError(f'Trying to set target property to invalid value '
                        f'(name: {descriptor.full_name}, value: {arg}, '
                        f'allowed_values: {", ".join(descriptor.allowed_values)})')

            if descriptor.cast is not None:
                if descriptor.cast == int:
                    if isinstance(arg, str):
                        arg = int(arg, 0)
                    else:
                        arg = int(arg)

            descriptor.value = arg

    def parse_target_properties(self):
        """Parse target properties.

        This can be called to parse the target properties given on the command line
        so that the property declaration can already use them.

        Parameters
        ----------
        properties : List
            The list of properties
        """
        if not self.target_properties_parsed:
            self.target_properties_parsed = True
            [args, _] = self.parser.parse_known_args()
            properties = args.target_properties

            for property_desc_list in properties:
                # Each property can actually be a list of form name=value,name=value
                for property_desc in property_desc_list.split(','):
                    # Property format is name=value
                    try:
                        name, value = property_desc.rsplit('=', 1)
                    except Exception as exc:
                        raise RuntimeError('Invalid target property (must be of form '
                            '<name>=<value>: ' + property_desc) from exc

                    self.args_properties[name] = value


    def check_args(self):
        """Check arguments.

        This can be called to make the target check some arguments which were not yet checked
        like target properties.
        """
        for name in self.args_properties:
            if self.target_properties.get(name) is None:
                raise RuntimeError(f'Trying to set undefined target property: {name}')



    def get_target_property(self, name: str, path: str=None) -> any:
        """Return the value of a target property.

        This can be called to get the value of a target property.

        Parameters
        ----------
        name : str
            Name of the property

        path : str
            Give the path of the component owning the property. If it is not None, the path is
            added as a prefix to the property name.

        Returns
        -------
        str
            The property value.
        """
        if path is not None:
            name = path + '/' + name
        if self.target_properties.get(name) is None:
            raise RuntimeError(f'Trying to get undefined property: {name}')

        prop = self.target_properties.get(name)
        value = prop.value

        return value

    def dump_target_properties(self):
        """Dump the properties of the target.
        """
        table = PrettyTable()
        table.field_names = ["Property name", "Value", "Allowed values", "Description"]
        for prop in self.target_properties.values():
            if prop.allowed_values is None:
                if prop.cast == int:
                    allowed_values = 'any integer'
                else:
                    allowed_values = 'any string'
            else:
                allowed_values = ', '.join(prop.allowed_values)
            value_str = prop.value
            if prop.format is not None:
                value_str = prop.format % prop.value
            table.add_row([prop.full_name, value_str, allowed_values, prop.description])

        table.align = 'l'
        print (table)


    def parse_args(self, args: any):
        """Handle arguments.

        This should be called only by gapy executable to handle arguments but it can be overloaded
        in order to handle arguments specifically to a target.

        Parameters
        ----------
        args :
            The arguments.
        """

        self.args = args
        self.layout_level = args.layout_level
        self.pem_path = args.pem_path
        self.sign_dgst = args.sign_dgst

        # Parse the flash properties so that we can propagate to each flash only its properties
        if len(args.flash_properties) != 0:
            self.__extract_flash_properties(args.flash_properties)

        if len(args.flash_contents) > 0:
            if len(args.multi_flash_content) > 0:
                # ERROR
                raise RuntimeError('Trying to set both a multi and single flash layout')

            # Now propagte the flash content to each flash
            for content in args.flash_contents:
                if content.find("@") == -1:
                    raise RuntimeError('Invalid flash content (must be of form '
                        '<content path>@<flash name>): ' + content)

                # Content is specified as content_file_path@flash_name
                content_path, flash_name = content.rsplit('@', 1)

                if self.flashes.get(flash_name) is None:
                    raise RuntimeError('Invalid flash content, flash is unknown: ' + content)

                try:
                    with open(content_path, "rb") as file_desc:
                        content_dict = json.load(file_desc, object_pairs_hook=OrderedDict)

                        self.flashes.get(flash_name).set_content(content_dict)
                except OSError as exc:
                    raise RuntimeError('Invalid flash content, got error while opening'
                        'content file: '
                        + str(exc)) from exc
        else:
            if len(args.multi_flash_content) > 0:
                for content in args.multi_flash_content:
                    try:
                        with open(content, "rb") as file_desc:
                            flash_dict = json.load(file_desc, object_pairs_hook=OrderedDict)
                    except OSError as exc:
                        raise RuntimeError('Invalid flash content, got error while opening'
                                       ' content file: '
                            + str(exc)) from exc
                if flash_dict.get('flashes') is None:
                    raise RuntimeError('No flashes dictionary in layout file')
                for flash_content in flash_dict.get('flashes'):
                    flash_name = flash_content.get('name')
                    if flash_name is None or self.flashes.get(flash_name) is None:
                        raise RuntimeError('Invalid flash name' +flash_name)
                    self.flashes.get(flash_name).set_content(flash_content)

        if len(args.flash_override) > 0:
            for content in args.flash_override:
                value, prop = content.rsplit('@', 1)
                flash_name, prop_name = prop.rsplit(':', 1)
                if self.flashes.get(flash_name) is None:
                    raise RuntimeError('Invalid flash for flasher: ' + content)
                flash = self.flashes.get(flash_name)
                flash.set_flash_attribute(prop_name, value)

    def set_target_dirs(self, target_dirs: list):
        """Set the target directories.

        This sets the directories where to look for targets.
        This should be called only by gapy executable.

        Parameters
        ----------
        target_dirs : list
            The directories.
        """

        self.target_dirs = target_dirs



    def get_working_dir(self) -> str:
        """Return the working directory.

        All files produced by gapy will be dumped into this directory.

        Returns
        -------
        str
            The working directory.
        """
        if self.work_dir is None:
            return os.getcwd()

        return self.work_dir


    def __get_target_files(self):

        result = []

        for path in self.target_dirs:
            for root, __, files in os.walk(path):

                for file in files:
                    if file.endswith('.py') and file != '__init__.py':
                        if root == path:
                            file_path = file
                        else:
                            rel_dir = os.path.relpath(root, path)
                            file_path = os.path.join(rel_dir, file)

                        target_name = file_path.replace('/', '.').replace('.py', '')

                        result.append(target_name)

        return result


    def __display_targets(self):

        table = rich.table.Table(title='Available targets')
        table.add_column('Name')
        table.add_column('Description')

        targets = {}

        # Go through all files from target directories and check which one is having a valid
        # Gapy target
        for target_name in self.__get_target_files():
            try:
                module = importlib.import_module(target_name)

            except ModuleNotFoundError:
                continue

            # Gapy targets have a Target class inside the module
            if 'Target' in dir(module):

                target_class = getattr(module, 'Target', None)

                # Check that the class comes from the module itself and not from an imported one
                if inspect.isclass(target_class) and target_class.__module__ == module.__name__:

                    # And must have the is_gapy_target attribute
                    if getattr(target_class, 'is_gapy_target') is True:

                        if targets.get(target_name) is None:
                            targets[target_name] = True

                            table.add_row(target_name,
                                f'{getattr(target_class, "gapy_description")}')

        rich.print(table)


    def __extract_flash_properties(self, args_properties: list):
        # Since they can be specified in any order on the command line, we first need to
        # sort them out in a dictionary
        properties = {}

        # First parse them and store them in the dictionary
        for property_desc in args_properties:
            # Property format is value@flash:section:property_name
            try:
                value, path = property_desc.rsplit('@', 1)
                flash, section, property_name = path.split(':')
            except Exception as exc:
                raise RuntimeError('Invalid flash property (must be of form '
                    '<value>@<flash:section:property>): ' + property_desc) from exc

            # Temporary hack for gap_sdk to keep compatibility with old gapy
            # Once all the tests are fully switched to new gapy, this can be removed
            if flash == 'target/chip/soc/mram':
                flash = 'mram'

            if properties.get(flash) is None:
                properties[flash] = {}
            if properties[flash].get(section) is None:
                properties[flash][section] = []
            properties[flash][section].append([property_name, value])

        # And then propagate them to the flashes
        for flash_name, flash_properties in properties.items():
            if self.flashes.get(flash_name) is None:
                raise RuntimeError('Invalid flash property, flash is unknown: ' + flash_name)

            self.flashes.get(flash_name).set_properties(flash_properties)

    def get_section_by_name(self, name):
        """Get a section by its name from all underlying flashes
        Parameters
        ----------
        name : str
            Name to search for
        Returns
        -------
        FlashSection
            Flash section with name "name"
        """
        section = None
        # go through all flashes
        for _, flash in self.flashes.items():
            section = flash.get_section_by_name(name)
            if section is not None:
                return section
        return None

    def get_section_index(self, name: str) -> int:
        """Get a section index by its name from all underlying flashes
        Parameters
        ----------
        name : str
            Name to search for
        Returns
        -------
        int
            index of Flash section with name "name"
        """
        index = 0
        # go through all flashes
        for _, flash in self.flashes.items():
            in_flash_index = flash.get_section_index(name)
            if in_flash_index is not None:
                index += in_flash_index
                return index
            index += len(flash.sections)
        return None

    def register_command_handler(self, handler):
        """Register a command handler

        The target will first try to make registered handlers execute the command and will execute
        it only if no handler has handled it.

        Parameters
        ----------
        handler : str
            Command handler
        """
        self.command_handlers.append(handler)
