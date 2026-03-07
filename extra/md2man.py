#!/usr/bin/env python

"""
Convert markdown to man.
"""

import argparse

HEADER = '.TH "SWAYIMG" "1" "2026-03-08" "swayimg" "Swayimg manual"'


def format_line(line):
    """Convert single line."""
    result = ''
    format_open = ''
    for char in line:
        if char == '\\':
            result += '\\'
        if char == '_' and not format_open:
            format_open = char
            result += '\\fI'
            continue
        if char in ['`', '*'] and not format_open:
            format_open = char
            result += '\\fB'
            continue
        if char in ['_', '`', '*'] and format_open == char:
            format_open = ''
            result += '\\fR'
            continue
        result += char
    if not result:
        result = '.PP'
    return result


def convert(src_file):
    """Convert MD to MAN."""
    with open(src_file, 'r', encoding='utf-8') as file:
        print(HEADER)
        indent = 0
        code_block = False
        list_block = False
        for line in file:
            line = line.rstrip()
            if line.startswith('# '):
                continue
            if line.startswith('## '):
                line = line.replace('##', '.SH')
            if not code_block and line.startswith(' '):
                line_lstrip = line.lstrip(' ')
                new_indent = len(line) - len(line_lstrip)
                line = line_lstrip
                if indent != new_indent:
                    indent = new_indent
                    print(f'.RS {indent}')
            elif indent != 0:
                indent = 0
                print('.RE')
            if line.startswith('```'):
                if code_block:
                    print('.RE\n.FI')
                else:
                    print('.NF\n.RS 4')
                code_block = not code_block
                continue
            if line.startswith('-'):
                if not list_block:
                    list_block = True
                    print('.PD 0')
                line = line.lstrip('- ')
                print('.IP \\(bu 4')
            elif list_block:
                list_block = False
                print('.PD')
            print(format_line(line))


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description='MD to MAN converter.')
    parser.add_argument('source', help='path to source file')
    args = parser.parse_args()
    convert(args.source)


if __name__ == '__main__':
    main()
