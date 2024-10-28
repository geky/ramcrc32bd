#!/usr/bin/env python3

import itertools as it
import functools as ft
import operator as op


def main(bits, *,
        p=None,
        no_zeros=False,
        brute_force=None,
        expected=None):
    # reparse to accept bits with and without spaces
    bits_ = []
    for b in bits:
        for c in b:
            if c == '1':
                bits_.append(1)
            elif c == '0':
                bits_.append(0)
    bits = bits_

    # concatenate zeros?
    if not no_zeros:
        bits += [0]*8

    # combine into a bigint, Python's bigints make the math here easy
    c = ft.reduce(lambda a,b: (a << 1) | b, bits, 0)
    n = len(bits)

    # print nicely
    def cbits(c, n):
        return ' '.join(
            '{:0{w}b}'.format(
                (c >> (i*8)) & 0xff,
                w=min(n - i*8, 8))
            for i in reversed(range((n+8-1)//8)))

    # calculate CRC
    def crc(c):
        for i in range(n-8):
            if (c >> (n-1-i)) & 1:
                c ^= p << (n-1-i-8)
        return c

    # brute force?
    if brute_force is not None:
        # default expected to zero
        expected = expected or 0

        # this function generates all single bit-error CRCs
        def bitcrcs(n):
            crc = 1
            for _ in range(n):
                yield crc
                crc = crc << 1
                if crc & 0x100:
                    crc ^= p

        # try an increasing number of bit errors
        crc_ = crc(c)
        for e in range(brute_force+1):
            for perm in it.combinations(enumerate(bitcrcs(n)), e):
                crc__ = crc_
                for _, crc___ in perm:
                    crc__ ^= crc___
                
                # found a match?
                if crc__ == expected:
                    # find original codeword
                    c_ = c
                    for i, _ in perm:
                        c_ ^= 1 << i

                    # print (we may find multiple)
                    print("%s => %s (0x%02x == 0x%02x)" % (
                        cbits(c_, n), cbits(crc(c_), 8),
                        crc(c_), expected))

    else:
        # print result
        if expected is not None:
            print("%s => %s (0x%02x %s 0x%02x)" % (
                cbits(c, n), cbits(crc(c), 8),
                crc(c),
                '==' if crc(c) == expected else '!=',
                expected))
        else:
            print("%s => %s (0x%02x)" % (
                cbits(c, n), cbits(crc(c), 8), crc(c)))


if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(
        description="Calculate/solve simple CRC-8 binary polynomials.",
        allow_abbrev=False)
    parser.add_argument(
        'bits',
        nargs='*',
        help="Bit sequence to find an LFSR from.")
    parser.add_argument(
        '-p',
        type=lambda x: int(x, 0),
        default=0x107,
        help="The generator polynomial that defines the CRC. Defaults "
            "to 0x107.")
    parser.add_argument(
        '-n', '--no-zeros',
        action='store_true',
        help="Don't concatenate with zeros to make space for the CRC.")
    parser.add_argument(
        '-b', '--brute-force',
        type=lambda x: int(x, 0),
        help="Brute force a solution to the given CRC codeword, up to the "
            "specified number of bit-errors.")
    parser.add_argument(
        '-e', '--expected',
        type=lambda x: int(x, 0),
        help="Expected CRC.")
    sys.exit(main(**{k: v
        for k, v in vars(parser.parse_args()).items()
        if v is not None}))
