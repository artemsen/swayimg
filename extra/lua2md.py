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


class LuaAlias:
    """Lua alias description."""

    RE_ALIAS = r'^---\s*@alias\s+(\w+)\s*(\w+)?$'
    RE_VALUE = r'^---\s*\|\s*([^\s]+)\s+#\s*(.*)$'

    def __init__(self, name: str, description: list[str]):
        assert name
        assert description
        self.name = name
        self.values = {}
        self.description = '\n'.join(description[1:])
        self.title = description[0]
        if self.title.endswith('.'):
            self.title = self.title[:-1]

    def to_markdown(self):
        """Convert alias description to markdown format."""
        markdown = f'`{self.name}` - {self.title}'
        if self.description:
            markdown += f':\n\n{self.description}'
        if self.values:
            markdown += ':\n' + '\n'.join(f'* `{name}`: {desc}' for name, desc in
                                          self.values.items())
        return markdown

    @staticmethod
    def parse(lua_code: list[str]):
        """Get aliases from Lua code."""
        re_declare = re.compile(LuaAlias.RE_ALIAS)
        re_value = re.compile(LuaAlias.RE_VALUE)
        aliases = {}
        alias = None
        comment = []
        for line in lua_code:
            if not line or not line.startswith('---'):
                if alias:
                    aliases[alias.name] = alias
                alias = None
                comment = []
                continue
            if alias:
                match = re_value.search(line)
                if match:
                    alias.values[match.group(1)] = match.group(2)
            else:
                match = re_declare.search(line)
                if match:
                    alias = LuaAlias(match.group(1), comment)
                    comment = []
                else:
                    comment.append(line[3:])
        return aliases


class LuaClass:
    """Lua class description."""

    RE_DECLARE = r'^---\s*@class\s+([\w\.]+)(\s*:\s*([\w\.]+))?$'

    def __init__(self, name: str, parent: str, description: list[str]):
        assert name
        assert description
        self.name = name
        self.parent = parent
        self.fields = []
        self.functions = []
        self.description = '\n'.join(description[1:])
        self.title = description[0]
        if self.title.endswith('.'):
            self.title = self.title[:-1]

    def to_markdown(self, aliases: dict[str, LuaAlias]):
        """Convert class description to markdown format."""
        markdown = f'## {self.title}'
        if self.fields:
            markdown += '\n\n' + \
                '\n\n'.join(field.to_markdown(self.name, aliases)
                            for field in self.fields)
        if self.functions:
            markdown += '\n\n' + \
                '\n\n'.join(func.to_markdown(self.name, aliases)
                            for func in self.functions)
        return markdown

    @staticmethod
    def parse(lua_code: list[str]):
        """Get classes from Lua code."""
        re_declare = re.compile(LuaClass.RE_DECLARE)
        classes = {}
        comment = []
        for line in lua_code:
            if not line or not line.startswith('---'):
                comment = []
                continue
            match = re_declare.search(line)
            if match:
                name = match.group(1)
                classes[name] = LuaClass(name, match.group(3), comment)
                comment = []
            else:
                comment.append(line[3:])
        return classes


class LuaField:
    """Lua class field description."""

    RE_DECLARE = r'^---\s*@field\s+(\w+)\s+(\w+|table\<[\w\s,]+\>)\s*(.*)?$'

    def __init__(self, name: str, luatype: str, description: list[str]):
        assert name
        assert description
        self.name = name
        self.luatype = luatype
        self.description = '\n\n'.join(description[1:])
        self.title = description[0]
        if self.title.endswith('.'):
            self.title = self.title[:-1]

    def to_markdown(self, classname: str, aliases: dict[LuaAlias]):
        """Convert field description to markdown format."""
        markdown = f'### {classname}.{self.name}\n\n'
        markdown += f'```lua\n{classname}.{self.name}: {self.luatype}\n```\n\n'
        markdown += f'{self.title}.'
        if self.description:
            markdown += f'\n\n{self.description}'
        if self.luatype in aliases:
            markdown += f'\n\n{aliases[self.luatype].to_markdown()}'
        return markdown

    @staticmethod
    def parse(lua_code: list[str], classes: dict[str, LuaClass]):
        """Get class fields from Lua code."""
        re_class = re.compile(LuaClass.RE_DECLARE)
        re_field = re.compile(LuaField.RE_DECLARE)
        luaclass = None
        comment = []
        for line in lua_code:
            if not line or not line.startswith('---'):
                luaclass = None
                comment = []
                continue
            match = re_class.search(line)
            if match:
                name = match.group(1)
                assert name in classes
                luaclass = classes[name]
                comment = []
                continue
            match = re_field.search(line)
            if match:
                assert luaclass
                name = match.group(1)
                while comment and not comment[0]:
                    comment.pop(0)
                if match.group(3):
                    comment = [match.group(3)] + comment
                luaclass.fields.append(LuaField(name, match.group(2), comment))
                comment = []
                continue
            comment.append(line[3:])


class LuaFunctionReturn:
    """Lua function return description."""

    RE_DECLARE = re.compile(
        r'^---\s*@return\s+([\w\[\]\.\|]+|{.*})\s+#\s+(.*)$')

    def __new__(cls, line: str):
        match = LuaFunctionReturn.RE_DECLARE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.luatype = match.group(1)
        instance.description = match.group(2)
        return instance

    def to_markdown(self, aliases: dict[str, LuaAlias]):
        """Convert function return description to markdown format."""
        markdown = f'@_return_ - {self.description}'
        names = re.sub(r'[\[\]]', '', self.luatype).split('|')
        aliases = [aliases[name] for name in names if name in aliases]
        if aliases:
            markdown += '\n\n' + '\n'.join(a.to_markdown() for a in aliases)
        return markdown


class LuaFunctionParam:
    """Lua function parameter description."""

    RE_DECLARE = re.compile(
        r'^---\s*@param\s+(\w+)(\?)?\s+([\w\|\[\]\?]+)\s+(.*)$')

    def __new__(cls, line: str):
        match = LuaFunctionParam.RE_DECLARE.search(line)
        if not match:
            return None
        instance = super().__new__(cls)
        instance.name = match.group(1)
        instance.optional = '?' if match.group(2) else ''
        instance.luatype = match.group(3)
        instance.description = match.group(4)
        return instance

    def to_markdown(self, aliases: dict[str, LuaAlias]):
        """Convert function param description to markdown format."""
        markdown = f'@_param_ `{self.name}` - {self.description}'
        names = re.sub(r'[\[\]]', '', self.luatype).split('|')
        aliases = [aliases[name] for name in names if name in aliases]
        if aliases:
            markdown += '\n\n' + '\n'.join(a.to_markdown() for a in aliases)
        return markdown


class LuaFunction:
    """Lua function description."""

    RE_DECLARE = r'^function\s+([\w\.]+)\([^\)]*\)\s+end$'

    def __init__(self, name: str, description: list[str]):
        assert name
        assert description
        self.name = name
        self.params = []
        self.ret = None

        self.description = ''
        for line in description[1:]:
            if line.startswith('@deprecated'):
                continue
            if self.description:
                self.description += '\n'
            if line.startswith('@see '):
                line = line[len('@see '):]
                line = f'\nSee [{line}](#{line.replace(".", "")}).'
            self.description += line

        self.title = description[0]
        if self.title.endswith('.'):
            self.title = self.title[:-1]

    def to_markdown(self, class_name: str, aliases: dict[str, LuaAlias]):
        """Convert function description to markdown format."""
        markdown = f'### {class_name}.{self.name}\n\n'
        params = ', '.join(
            f'{param.name}{param.optional}: {param.luatype}' for param in self.params)
        ret = f' -> {self.ret.luatype}' if self.ret else ''
        markdown += f'```lua\n{class_name}.{self.name}({params}){ret}\n```\n\n'
        markdown += f'{self.title}.\n\n{self.description}'
        if self.params:
            markdown += '\n\n' + \
                '\n\n'.join(param.to_markdown(aliases)
                            for param in self.params)
        if self.ret:
            markdown += f'\n\n{self.ret.to_markdown(aliases)}'
        return markdown

    @staticmethod
    def parse(lua_code: list[str], classes: dict[str, LuaClass]):
        """Get functions from Lua code."""
        re_declare = re.compile(LuaFunction.RE_DECLARE)
        func_params = []
        func_return = None
        comment = []
        for line in lua_code:
            if not line:
                func = None
                func_params = []
                func_return = None
                comment = []
                continue
            fn_param = LuaFunctionParam(line)
            if fn_param:
                func_params.append(fn_param)
                continue
            fn_return = LuaFunctionReturn(line)
            if fn_return:
                func_return = fn_return
                continue
            match = re_declare.search(line)
            if match:
                class_name, func_name = match.group(1).rsplit('.', 1)
                func = LuaFunction(func_name, comment)
                func.params = func_params
                func.ret = func_return
                assert class_name in classes
                classes[class_name].functions.append(func)
                func_params = []
                func_return = None
                comment = []
                continue
            if line.startswith('---'):
                comment.append(line[3:])


def print_md(classes: list[LuaClass], aliases: list[LuaAlias]):
    """Print markdown."""
    visible_classes = ['swayimg',
                       'swayimg.imagelist',
                       'swayimg.text',
                       'swayimg.viewer',
                       'swayimg.slideshow',
                       'swayimg.gallery']
    print(HEADER)
    # table of contents
    print('## List of available functions\n')
    for name in visible_classes:
        lclass = classes[name]
        print(f'* {lclass.title}')
        for func in lclass.functions:
            full_name = f'{name}.{func.name}'
            anchor = full_name.replace('.', '')
            print(f'  * [{full_name}](#{anchor}): {func.title}')
    # api description
    for name in visible_classes:
        lclass = classes[name]
        print()
        print(lclass.to_markdown(aliases))


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description='Lua to MD converter.')
    parser.add_argument('source', help='path to source file')
    args = parser.parse_args()
    # parse Lua code
    with open(args.source, 'r', encoding='utf-8') as file:
        lua_code = [line.strip() for line in file.readlines()]
        aliases = LuaAlias.parse(lua_code)
        classes = LuaClass.parse(lua_code)
        LuaField.parse(lua_code, classes)
        LuaFunction.parse(lua_code, classes)
    # merge with parent class (remove inheritance)
    for lclass in classes.values():
        if lclass.parent:
            parent = classes[lclass.parent]
            lclass.fields += parent.fields
            lclass.functions += parent.functions
    # print final MD
    print_md(classes, aliases)


if __name__ == '__main__':
    main()
