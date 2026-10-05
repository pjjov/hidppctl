# hidppctl -- Configure HID++ compatible peripherals.
#
# Copyright (C) 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Look at the COPYING file for more information.

if get_option('embed_tables')
  python = find_program('python3')

  table_files = files(
    'controls.txt',
    'features.txt',
  )

  tables_src = custom_target(
    'constant_tables',
    input: table_files,
    output: 'constant_tables_generated.c',
    command: [
      python,
      files('embed.py'),
      '--output', '@OUTPUT@',
      '--input', '@INPUT@',
    ],
    build_by_default: true,
  )
else
  tables_src = files('fallback.c')
endif
