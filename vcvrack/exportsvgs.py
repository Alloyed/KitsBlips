#!/usr/bin/env python
# Rewrite affinity designer-exported SVG files to be compatible with nanosvg
# adapted from:
# https://gist.github.com/lindenbergresearch/90d8a856d60eb537401e3855b6374b17
#    Copyright 2017-2021 by Patrick Lindenberg / LRT
#    heapdump@icloud.com
#    3-Clause BSD License.
# With extra fixes from:
# https://github.com/nickfeisst/ADSVGFix/blob/main/ADSVGFix.py
#    Copyright (c) 2021 nickfeisst
import xml.etree.ElementTree as ET
import os
import argparse

# basic config
SVG_NAMESPACE = 'http://www.w3.org/2000/svg'

ET.register_namespace('', SVG_NAMESPACE)

def fix_svg(filename):
    """Applies fixes to the given SVG file in place. Returns True if file was modified."""
    tree = ET.parse(filename)
    root = tree.getroot()
    modified = False

    # reorder defs to be the first child of root. needed for Reasons(tm)
    defs_tag = '{' + SVG_NAMESPACE + '}defs'
    defs = root.find(defs_tag)
    if defs and root[0] != defs:
        root.remove(defs)
        root.insert(0, defs)
        modified = True

    # rewrite width/height to be absolute mm values. VCV Rack uses this to determine the size of the panel.
    if root.get('width', '100%') == '100%' and root.get('height', '100%') == '100%':
        _, _, w, h = [float(x) for x in root.get('viewBox', '0 0 0 0').split(' ')]
        mmPerInch = 25.4
        mmHeight = h * mmPerInch / DPI
        mmWidth = w * mmPerInch / DPI
        root.set('width', str(mmWidth) + 'mm')
        root.set('height', str(mmHeight) + 'mm')
        modified = True

    # remove all serif: attributes. unused.
    for elem in root.iter():
        attribs_to_remove = [key for key in elem.attrib if key.startswith('serif:')]
        for key in attribs_to_remove:
            del elem.attrib[key]
            modified = True


    if modified:
        tree.write(filename)

    return modified


def scan_path(paths, suffix):
    """ scan a specific path for the given files that matches suffix """
    matches = []

    for path in paths:
        try:
            print('scanning path: ' + path)
            entries = os.listdir(path)
        except OSError:
            print('error: invalid path: ' + path)
            return []
        else:
            for entry in entries:
                if entry.endswith(suffix):
                    matches.append(path + '/' + entry)
    return matches


def main():
    parser = argparse.ArgumentParser(description='Rewrite SVG files for nanosvg compatibility.')
    parser.add_argument('paths', nargs='*', help='Paths to scan for SVG files')
    parser.add_argument('--dpi', type=float, default=300.0, help='DPI to use for mm conversion (default: 300)')
    args = parser.parse_args()

    global DPI
    DPI = args.dpi
    SVG_PATHS = args.paths
    
    files = scan_path(SVG_PATHS, '.svg')

    print("found " + str(len(files)) + " image(s) to examine.")

    fixcount = 0
    for file in files:
        if fix_svg(file):
            print(file)
            fixcount += 1

    print('fixed: ' + str(fixcount) + ' images.\n')

if __name__ == '__main__':
    main()