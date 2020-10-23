import io
import sys

"""
Normalize the input text to output ascii-only text
by removing any line that contains non-ascii

Also lowercase all text
"""

printable_chars = ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~'

def main():
    with io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip()
            skip = False
            for c in line:
                if c not in printable_chars:
                    skip = True
                    continue
            if not skip:
                print(line.lower())


if __name__ == '__main__':
    main()
