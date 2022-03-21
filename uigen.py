#!/usr/bin/env python3

import sys, os

def generate(ifile, ofile):
    contents = open(ifile).read()
    open(ofile, 'w').write(f'''const char *ui_text = R"(
{contents}
)";
''')

if __name__ == '__main__':
    generate(sys.argv[1], sys.argv[2])
