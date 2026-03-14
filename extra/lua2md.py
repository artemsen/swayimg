#!/usr/bin/env python

"""
Generate markdown documentation from Lua API file.
"""

import argparse
import re

HEADER = """# Swayimg configuration

The Swayimg configuration file is a Lua script.

Please refer to the official Lua documentation for information about the file
format.

The source file [swayimg.lua](extra/swayimg.lua) contains a description of Lua
bindings and can be used for the LSP server, it is located in `/usr/share/swayimg/swayimg.lua`
after installing the program.

The program searches for the config file in the following locations:
1. `$XDG_CONFIG_HOME/swayimg/init.lua`
2. `$HOME/.config/swayimg/init.lua`
3. `$XDG_CONFIG_DIRS/swayimg/init.lua`
4. `/etc/xdg/swayimg/init.lua`

Config example:
```lua
swayimg.text.set_size(32)
swayimg.text.set_foreground(0xffff0000)

swayimg.viewer.set_default_scale("fill")

swayimg.gallery.on_key("Delete", function()
  local image = swayimg.gallery.get_image()
  os.remove(image.path)
end)
```

A more detailed example can be found on the [project website](extra/example.lua)
or in the file `/usr/share/swayimg/example.lua` after installing the program.
"""


# pylint: disable=too-few-public-methods
class LuaAliasValue:
    """Lua alias value description."""

    RE = re.compile(r'^---\s*\|\s*([^\s]+)\s+#\s*(.*)$')

    def __new__(cls, line: str):
        match = LuaAliasValue.RE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.name = match.group(1)
        instance.description = match.group(2)
        return instance


class LuaAlias:
    """Lua alias description."""

    RE = re.compile(r'^---\s*@alias\s+(\w+)\s*(\w+)?$')

    def __new__(cls, line, comment):
        match = LuaAlias.RE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.name = match.group(1)
        instance.values: list[LuaAliasValue] = []
        instance.rtype = match.group(2)
        instance.description = comment
        return instance

    @staticmethod
    def print(name, aliases):
        """Print alias description in markdown format."""
        name = re.sub(r'[\[\]]', '', name)
        for name in name.split('|'):
            if name in aliases:
                alias = aliases[name]
                if alias.values:
                    print(f'\n`{name}`, {alias.description}:')
                    for av in alias.values:
                        print(f'* `{av.name}`: {av.description}')
                else:
                    print(f'\n`{name}`:\n{alias.description}')


class LuaField:
    """Lua class field description."""

    RE = re.compile(r'^---\s*@field\s+(\w+)\s+([\w\|\[\]]+)\s+(.*)$')

    def __new__(cls, line, lclass, comment):
        match = LuaField.RE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.name = f'{lclass.name}.' + match.group(1)
        instance.type = match.group(2)
        instance.title = match.group(3)
        instance.description = comment
        return instance

    def print(self, aliases):
        """Print field description in markdown format."""
        print(f'### {self.name}: {self.title}')
        type_descr = ''
        if self.type not in aliases:
            type_descr = self.type
        else:
            for av in aliases[self.type].values:
                if type_descr:
                    type_descr += '|'
                type_descr += av.name
        print(f'```lua\n{self.name} = {type_descr}\n```')
        if self.description:
            print(self.description)
        LuaAlias.print(self.type, aliases)


class LuaParam:
    """Lua function parameter description."""

    RE = re.compile(r'^---\s*@param\s+(\w+)(\?)?\s+([\w\|\[\]\?]+)\s+(.*)$')

    def __new__(cls, line):
        match = LuaParam.RE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.name = match.group(1)
        instance.optional = '?' if match.group(2) else ''
        instance.type = match.group(3)
        instance.description = match.group(4)
        return instance

    def print(self, aliases):
        """Print function param description in markdown format."""
        print(f'@_param_ `{self.name}` - {self.description}')
        LuaAlias.print(self.type, aliases)


class LuaReturn:
    """Lua function return description."""

    RE = re.compile(r'^---\s*@return\s+([\w\[\]\.]+|{.*})\s+#\s+(.*)$')

    def __new__(cls, line):
        match = LuaReturn.RE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.type = match.group(1)
        instance.description = match.group(2)
        return instance

    def print(self, aliases):
        """Print function return description in markdown format."""
        print(f'@_return_ - {self.description}')
        LuaAlias.print(self.type, aliases)


class LuaFunction:
    """Lua function description."""

    RE = re.compile(r'^function\s+([\w\.]+)\([^\)]*\)\s+end$')

    def __init__(self):
        self.name: str = None
        self.params: list[LuaParam] = []
        self.retval: LuaReturn = None
        self.title: str = None
        self.description: str = None

    def get_class_name(self):
        """Get class name."""
        return self.name[:self.name.rfind('.')]

    def copy(self, class_name: str):
        """Create copy of function instance with different class name."""
        fn = LuaFunction()
        fn.name = f'{class_name}.{self.name.split(".")[-1]}'
        fn.params = self.params
        fn.retval = self.retval
        fn.title = self.title
        fn.description = self.description
        return fn

    def print(self, aliases):
        """Print function description in markdown format."""
        print(f'### {self.name}\n')

        params = ''
        for param in self.params:
            if params:
                params += ', '
            params += f'{param.name}{param.optional}: {param.type}'
        ret = ''
        if self.retval:
            ret = f' -> {self.retval.type}'
        code = f'{self.name}({params}){ret}'
        print(f'```lua\n{code}\n```\n')

        print(f'{self.title}.\n')
        if self.description:
            print(self.description)
            print()

        for param in self.params:
            param.print(aliases)
            print()
        if self.retval:
            self.retval.print(aliases)
            print()

    def parse(self, line, comment):
        """Parse Lua annotation line."""
        match = LuaFunction.RE.search(line)
        if match:
            self.name = match.group(1)
            self.title = comment.split('\n', 1)[0]
            if self.title.endswith('.'):
                self.title = self.title[:-1]
            if '\n' in comment:
                self.description = comment.split('\n', 1)[1]
            return True
        return False


class LuaClass:
    """Lua class description."""

    RE = re.compile(r'^---\s*@class\s+([\w\.]+)(\s*:\s*([\w\.]+))?$')

    def __new__(cls, line, comment):
        match = LuaClass.RE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.name = match.group(1)
        instance.parent = match.group(3)
        instance.fields: list[LuaField] = []
        instance.functions: list[LuaFunction] = []
        instance.description = comment
        return instance

    def merge(self, classes):
        """Merge with parent class (remove inheritance)."""
        if self.parent:
            lclass = classes[self.parent]
            for func in lclass.functions:
                self.functions.append(func.copy(self.name))

    def print(self, aliases):
        """Print class description in markdown format."""
        for func in self.functions:
            func.print(aliases)


# pylint: disable=too-many-locals,too-many-branches,too-many-statements
def parse(file):
    """Parse API description."""
    classes = {}
    aliases = {}

    cur_alias = None
    cur_class = None
    cur_func = None
    cur_comment = ''

    for line in file:
        line = line.strip()
        if not line:
            cur_alias = None
            cur_class = None
            cur_func = None
            cur_comment = ''
            continue

        alias = LuaAlias(line, cur_comment)
        if alias:
            cur_alias = alias
            aliases[cur_alias.name] = cur_alias
            cur_comment = ''
            continue

        alias_val = LuaAliasValue(line)
        if alias_val:
            assert cur_alias
            cur_alias.values.append(alias_val)
            continue

        cur_alias = None

        lclass = LuaClass(line, cur_comment)
        if lclass:
            cur_class = lclass
            classes[cur_class.name] = cur_class
            cur_comment = ''
            continue

        field = LuaField(line, cur_class, cur_comment)
        if field:
            cur_class.fields.append(field)
            cur_comment = ''
            continue

        fnparam = LuaParam(line)
        if fnparam:
            if not cur_func:
                cur_func = LuaFunction()
            cur_func.params.append(fnparam)
            continue

        fnret = LuaReturn(line)
        if fnret:
            if not cur_func:
                cur_func = LuaFunction()
            assert not cur_func.retval
            cur_func.retval = fnret
            continue

        if cur_func and cur_func.parse(line, cur_comment):
            classes[cur_func.get_class_name()].functions.append(cur_func)
            cur_func = None
            cur_comment = ''
            continue

        # comments
        if line and line.startswith('---'):
            line = line[3:]
            line = line.rstrip()
            if line.startswith('@see '):
                line = line[len('@see '):]
                if '\n' in cur_comment:
                    cur_comment += '\n'
                line = f'See [{line}]({line.replace(".", "")})'
            if cur_comment:
                cur_comment += '\n'
            cur_comment += line

    return (aliases, classes)


# def print_class(aliases, classes):
def print_md(aliases, classes):
    """Print markdown."""
    sections = [('General functionality', 'swayimg'),
                ('Image list', 'swayimg.imagelist'),
                ('Text layer', 'swayimg.text'),
                ('Viewer mode', 'swayimg.viewer'),
                ('Slide show mode', 'swayimg.slideshow'),
                ('Gallery mode', 'swayimg.gallery')]
    print(HEADER)
    # table of contents
    print('## List of available functions\n')
    for section, class_name in sections:
        print(f'* {section}')
        for func in classes[class_name].functions:
            anchor = func.name.replace('.', '')
            print(f'  * [{func.name}](#{anchor}): {func.title}')
    print()
    # api description
    for section, class_name in sections:
        print(f'## {section}\n')
        classes[class_name].print(aliases)


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description='Lua to MD converter.')
    parser.add_argument('source', help='path to source file')
    args = parser.parse_args()
    with open(args.source, 'r', encoding='utf-8') as file:
        aliases, classes = parse(file.readlines())
    for lclass in classes.values():
        lclass.merge(classes)
    print_md(aliases, classes)


if __name__ == '__main__':
    main()
