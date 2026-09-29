# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

#!/usr/bin/env python3

import argparse
import re
import sys

LINE_RE = re.compile(r"^\s*(0x[0-9a-fA-F]+)\s*=\s*(.*?)\s*$")
IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def c_escape(s):
    return (
        s.replace("\\", "\\\\")
         .replace('"', '\\"')
         .replace("\n", "\\n")
         .replace("\r", "\\r")
         .replace("\t", "\\t")
    )


def parse_file(filename):
    array_name = None
    entries = []

    with open(filename, encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            line = line.rstrip("\n")

            # Ignore empty lines and comments.
            if not line.strip() or line.lstrip().startswith("#"):
                continue

            # The first meaningful line is the array name.
            if array_name is None:
                array_name = line.strip()

                if not IDENTIFIER_RE.fullmatch(array_name):
                    raise SystemExit(
                        f"{filename}:{lineno}: "
                        f"invalid C array name: {array_name!r}"
                    )

                continue

            match = LINE_RE.match(line)
            if not match:
                raise SystemExit(
                    f"{filename}:{lineno}: invalid entry: {line!r}"
                )

            value, text = match.groups()
            entries.append((value, text))

    if array_name is None:
        raise SystemExit(f"{filename}: missing array name")

    return array_name, entries


def main():
    parser = argparse.ArgumentParser(
        description="Generate a C array from one or more key=value files."
    )

    parser.add_argument(
        "--output",
        required=True,
        help="output C file",
    )

    parser.add_argument(
        "--input",
        nargs="+",
        required=True,
        help="input files",
    )

    args = parser.parse_args()

    tables = []
    names = set()

    for filename in args.input:
        array_name, entries = parse_file(filename)

        if array_name in names:
            raise SystemExit(
                f"duplicate array name: {array_name!r}"
            )

        names.add(array_name)
        tables.append((array_name, entries))

    with open(args.output, "w", encoding="utf-8") as f:
        f.write("""\
/* Generated file -- do not edit. */

#include <stddef.h>
#include <stdint.h>

struct hidpp_constant {
    uint16_t code;
    const char *name;
};

""")

        for array_name, entries in tables:
            f.write(
                f"static const struct hidpp_constant "
                f"{array_name}[] = {{\n"
            )

            for value, text in entries:
                f.write(
                    f'    {{ {value}u, "{c_escape(text)}" }},\n'
                )

            f.write(
                "};\n\n"
                f"static const size_t {array_name}_count = "
                f"sizeof({array_name}) / sizeof({array_name}[0]);\n\n"
            )


if __name__ == "__main__":
    main()