#!/usr/bin/env python

"""
Generate markdown documentation from Lua API file.
Script uses lua-language-server to generate JSON and then convert it to MD format.
"""

import json
import subprocess
import argparse
import re
from pathlib import Path

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
  local image = swayimg.gallery.current_image()
  os.remove(image["path"])
end)
```

A more detailed example can be found on the [project website](extra/example.lua)
or in the file `/usr/share/swayimg/example.lua` after installing the program.
"""


def get_funcs(src_file, tmp_dir):
    """Get functions list."""
    # lua to json
    subprocess.check_call(
        ['lua-language-server', '--doc_out_path', tmp_dir, '--doc', src_file],
        stdout=subprocess.DEVNULL)
    # json to python dict
    jfuncs = {}
    with open(tmp_dir / 'doc.json', 'r', encoding='utf-8') as file:
        data = json.load(file)
        for top_node in data:
            if len(top_node['defines']):
                defines = top_node['defines'][0]
                if defines['view'] == 'function':
                    title = defines['rawdesc']
                    title = title[:title.find('.')]
                    desc = defines['desc']
                    desc = re.sub(r'\[(swayimg)\.(\w+)\.(\w+)\]\(file:.*\)',
                                  r'[\1.\2.\3](#\1\2\3)', desc)
                    desc = re.sub(r'\[(swayimg)\.(\w+)\]\(file:.*\)',
                                  r'[\1.\2](#\1\2)', desc)
                    lua = defines['extends']['view']
                    lua = lua[len('function '):]
                    if not lua.startswith('swayimg.'):
                        lua = 'swayimg.' + lua
                    lua = lua.replace('\n ', '')
                    jfuncs[top_node['name']] = {
                        'name': top_node['name'],
                        'title': title,
                        'desc': desc,
                        'lua': lua,
                    }
    # restore original order (luals sorts by name)
    sfuncs = []
    with open(src_file, 'r', encoding='utf-8') as file:
        for line in file:
            if line.startswith('function '):
                name = line[len('function '):]
                name = name[:name.find('(')]
                sfuncs.append(jfuncs[name])
    return sfuncs


def filter_funcs(funcs, subsystem):
    """Filter functions by subsystem."""
    filtered = []
    for fn in funcs:
        parts = fn['name'].split('.')
        if (len(parts) == 2 and not subsystem) or parts[1] == subsystem:
            filtered.append(fn)
    return filtered


def print_md(funcs):
    """Print functions list as markdown document."""
    sections = [('General functionality', ''),
                ('Image list', 'imagelist'),
                ('Text layer', 'text'),
                ('Viewer mode functions', 'viewer'),
                ('Slide show mode', 'slideshow'),
                ('Gallery mode', 'gallery')]
    print(HEADER)
    # table of contents
    print('## List of available functions\n')
    for desc, name in sections:
        print(f'* {desc}')
        for fn in filter_funcs(funcs, name):
            anchor = fn['name'].replace('.', '')
            print(f'  * [{fn["name"]}](#{anchor}): {fn["title"]}')
    print()
    # functions description
    for desc, name in sections:
        print(f'## {desc}\n')
        for fn in filter_funcs(funcs, name):
            print(f'### {fn["name"]}\n')
            print(f'```lua\n{fn["lua"]}\n```\n')
            print(f'{fn["desc"]}\n')


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description='Lua to MD converter.')
    parser.add_argument('source', help='path to source file')
    parser.add_argument('-t', '--tmpdir', default='./', help='temp path')
    args = parser.parse_args()
    funcs = get_funcs(args.source, Path(args.tmpdir))
    print_md(funcs)


if __name__ == '__main__':
    main()
