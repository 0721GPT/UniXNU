#!/usr/bin/env python3
# Copyright (c) 2026, UniXNU contributors. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Generate the UniXNU logo from one vector description.

The logo is white strokes on black: an apple outline with a bite, an
arrow-shaped leaf, a chip in the middle and the "UniXNU" wordmark ("Uni"
heavy, "XNU" light). Every shape is a stroked path of lines and Bezier
curves with round joins and caps.

Outputs:
  Docs/Images/unixnu-logo.svg                      the vector logo
  CrabApplePkg/Library/CrabAppleLib/LogoShape.h    the same strokes, flattened
                                                   to integer polylines for the
                                                   firmware renderer (Logo.c)
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Design space: a 1000-unit square (the artwork's 1280 grid, cropped by ORIGIN).
DESIGN = 1000
ORIGIN = (140, 148)

# (stroke width, path). Paths: ('M', x, y), ('L', x, y), ('C', x1, y1, x2, y2, x, y), ('Q', x1, y1, x, y), ('Z',).
APPLE = [('M', 655, 372),
         ('C', 600, 352, 560, 340, 500, 342),
         ('C', 420, 345, 372, 420, 366, 520),
         ('C', 360, 625, 400, 730, 460, 800),
         ('C', 500, 845, 560, 848, 600, 834),
         ('C', 630, 824, 680, 824, 710, 834),
         ('C', 750, 848, 805, 845, 840, 800),
         ('C', 868, 762, 893, 712, 910, 666),
         ('C', 852, 640, 820, 584, 822, 524),
         ('C', 824, 472, 850, 434, 896, 408),
         ('C', 862, 370, 805, 346, 742, 350),
         ('C', 706, 353, 682, 362, 655, 372),
         ('Z',)]
LEAF = [('M', 641, 336),
        ('C', 643, 296, 664, 266, 694, 238),
        ('L', 668, 212),
        ('L', 788, 196),
        ('L', 776, 300),
        ('L', 749, 274),
        ('C', 724, 304, 694, 330, 641, 336),
        ('Z',)]


def rounded_rect(x0, y0, x1, y1, r):
    k = 0.5523 * r
    return [('M', x0 + r, y0), ('L', x1 - r, y0), ('C', x1 - r + k, y0, x1, y0 + r - k, x1, y0 + r),
            ('L', x1, y1 - r), ('C', x1, y1 - r + k, x1 - r + k, y1, x1 - r, y1),
            ('L', x0 + r, y1), ('C', x0 + r - k, y1, x0, y1 - r + k, x0, y1 - r),
            ('L', x0, y0 + r), ('C', x0, y0 + r - k, x0 + r - k, y0, x0 + r, y0), ('Z',)]


CHIP = rounded_rect(561, 511, 719, 669, 34)
PINS = []
for at in (596, 640, 684):
    PINS += [[('M', at, 468), ('L', at, 500)], [('M', at, 680), ('L', at, 712)]]
for at in (546, 590, 634):
    PINS += [[('M', 518, at), ('L', 550, at)], [('M', 730, at), ('L', 762, at)]]

HEAVY = [  # "Uni"
    [('M', 259, 952), ('L', 259, 1035), ('Q', 259, 1075, 306, 1075), ('Q', 353, 1075, 353, 1035), ('L', 353, 952)],
    [('M', 425, 1075), ('L', 425, 1012)],
    [('M', 425, 1038), ('Q', 430, 1012, 462, 1012), ('Q', 496, 1012, 496, 1045), ('L', 496, 1075)],
    [('M', 559, 1012), ('L', 559, 1075)],
    [('M', 559, 941), ('L', 559, 941)],
]
LIGHT = [  # "XNU"
    [('M', 603, 937), ('L', 720, 1088)],
    [('M', 720, 937), ('L', 603, 1088)],
    [('M', 775, 1088), ('L', 775, 937), ('L', 873, 1088), ('L', 873, 937)],
    [('M', 923, 937), ('L', 923, 1035), ('Q', 923, 1088, 980, 1088), ('Q', 1037, 1088, 1037, 1035), ('L', 1037, 937)],
]

STROKES = ([(24, APPLE), (22, LEAF), (22, CHIP)] + [(16, p) for p in PINS] + [(44, p) for p in HEAVY[:4]] + [(50, HEAVY[4])]
           + [(14, p) for p in LIGHT])


def flatten(path, steps=20):
    points, start, cur = [], None, None
    for seg in path:
        op = seg[0]
        if op == 'M':
            cur = start = seg[1:]
            points.append(cur)
        elif op == 'L':
            cur = seg[1:]
            points.append(cur)
        elif op == 'Z':
            if cur != start:
                points.append(start)
        else:
            ctrl = seg[1:]
            for i in range(1, steps + 1):
                t, u = i / steps, 1 - i / steps
                if op == 'C':
                    x = u**3 * cur[0] + 3 * u * u * t * ctrl[0] + 3 * u * t * t * ctrl[2] + t**3 * ctrl[4]
                    y = u**3 * cur[1] + 3 * u * u * t * ctrl[1] + 3 * u * t * t * ctrl[3] + t**3 * ctrl[5]
                else:
                    x = u * u * cur[0] + 2 * u * t * ctrl[0] + t * t * ctrl[2]
                    y = u * u * cur[1] + 2 * u * t * ctrl[1] + t * t * ctrl[3]
                points.append((x, y))
            cur = ctrl[-2:]
    return [(round(x - ORIGIN[0]), round(y - ORIGIN[1])) for x, y in points]


def svg_path(path):
    out = []
    for seg in path:
        op, values = seg[0], seg[1:]
        moved = [v - ORIGIN[i % 2] for i, v in enumerate(values)]
        out.append(op + ' '.join('%g' % v for v in moved))
    return ''.join(out)


def main():
    lines = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">' % (DESIGN, DESIGN, DESIGN, DESIGN),
             '  <title>UniXNU</title>',
             '  <rect width="%d" height="%d" fill="#000"/>' % (DESIGN, DESIGN),
             '  <g fill="none" stroke="#fff" stroke-linecap="round" stroke-linejoin="round">']
    for width, path in STROKES:
        lines.append('    <path stroke-width="%d" d="%s"/>' % (width, svg_path(path)))
    lines += ['  </g>', '</svg>', '']
    (ROOT / 'Docs/Images/unixnu-logo.svg').write_text('\n'.join(lines))

    header = ['/** @file',
              '  UniXNU logo strokes, generated by Scripts/gen-logo.py from the vector',
              '  description that also produces Docs/Images/unixnu-logo.svg. Do not edit.',
              '',
              '  Copyright (c) 2026, UniXNU contributors. All rights reserved.',
              '  SPDX-License-Identifier: BSD-2-Clause-Patent',
              '**/',
              '',
              '#ifndef LOGO_SHAPE_H_',
              '#define LOGO_SHAPE_H_',
              '',
              '#define LOGO_DESIGN  %d' % DESIGN,
              '',
              '//',
              '// Points of every stroke, in strokes order.',
              '//',
              'STATIC CONST INT16  mLogoPoints[][2] = {']
    strokes = []
    at = 0
    for width, path in STROKES:
        points = flatten(path)
        header.append('  ' + ', '.join('{ %d, %d }' % p for p in points) + ',')
        strokes.append((at, len(points), width // 2))
        at += len(points)
    header += ['};', '',
               '//',
               '// First point, point count and half width of each round-capped stroke.',
               '//',
               'STATIC CONST UINT16  mLogoStrokes[][3] = {']
    header += ['  { %d, %d, %d },' % s for s in strokes]
    header += ['};', '', '#endif', '']
    (ROOT / 'CrabApplePkg/Library/CrabAppleLib/LogoShape.h').write_text('\n'.join(header))
    print('logo: %d strokes, %d points' % (len(strokes), at))


main()
