#!/bin/env python
# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

import sys

mode = sys.argv[1]
in_fn = sys.argv[2]
out_fn = sys.argv[3]

with open(in_fn, 'rb') as f:
    data = f.read()
if 't' in mode:
    data += b'\0'

with open(out_fn, 'w') as f:
    cols = 16
    for i in range(0, len(data), cols):
        hexes = [f'0x{b:02x}' for b in data[i:i + cols]]
        print(', '.join(hexes), end=',\n', file=f)
