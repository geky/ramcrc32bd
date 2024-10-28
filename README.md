## ramcrc32bd

An example of a CRC-32 based error-correcting block device backed by RAM.

```
corrupted:
        '         ..:::::::::::.:.                  .:::....:::.. :.'  ''::..:':: '.:.::'.: '':.
               .:::::::::::::::::::..           .   ::'::::::::...:::.: :'.'::: .' ..:.: :.'  '.
            '.::::::::::::::::::::::::.          '  ::::::::::' . .' ': .  :'' . ': .: :. ..:::
           .:::::::::::::::::::::::::::.    .    . ::::':::::   ' . ''.:. ..:. : ::.':.'::. ....
   .      .::::::::::::::::::::::::::::::    ... :: :'::::::'    :.:....:..:  .  .   ' '':.:   :
          :::::::::::::::::::::::::.:::'' .. '::: '     '''     :..'::'.'''.:'. :''::. ' '  .'..
         ::::::::::::'::::::::::::::''..:::::''                 : .'' ::'. : .:'.''  .'' ' .. ..
         :::::::::::::::::::::::'' ..:::::''                  . ''' :'.: :'.:. :': .':'..': :.'.
         :::::':::::::::::::'' ..:::::'' .'                    '' .::.   . . :: .''::'.  '.. ...
     ..:'::::::::::::::''' ..:::::'' ...::                      '' ::..':' :'  .....:'' :'.  ::.
  . :::'  :::::::::'  ..::::::': ..:::::::        .              '' .::  :. '.: .''': :: .:.'' :
 :::'     ':::'' ...:.::::':...::::::::::                       ': :.:':: ..: .'' : ::..: .'' :
::'         ...::::::'' ..:::::':':::::'               '      . . .:' :  ''':':'.:..' :'::':.'':
     ....::::::::' . ::::::::::.::::::'     .                   ::.: :' . '.:. .. .' ...'.:':.'
'::::::::'''    :::::::::::::::::::''    '                      . :: ::' :' ':' .'...'':  ' ':':
                  '':::::::::::''' .   .       '          '     ::::. '.'. :::   . . '.' '  '..'
```

```
corrected:
                  ..:::::::::::...                  .:::....:::.. :.'  '':: .:::: '.:.:::.: '':.
               .:::::::::::::::::::..               ::::::::::::..:::.: :'.':': .' ..:.: :.'  '.
             .::::::::::::::::::::::::.             ::::::::::' . .' ': .  :'' . ': .: :. ..:::
           .:::::::::::::::::::::::::::.         . ::::::::::   ' . ''.:. ..:. : ::.':.'::. ....
          .::::::::::::::::::::::::::::::    ... :: :'::::::'    :.:....:..:  .  .   ' '':.:   :
          :::::::::::::::::::::::::::::'' .. '::: '     '''     :..'::'.'''.:'..:''::. ' '  .'..
         :::::::::::::::::::::::::::''..::::: '                 : .'' ::'. : .::.''  .'' ' .. ..
         :::::::::::::::::::::::'' ..:::::''                    ''' :'.: ''.:. :': :':'..': :.'.
         :::::::::::::::::::'' ..:::::'' .                      ' .::.   . . :: .''::'.  '.. ...
     ..: ::::::::::::::''' ..:::::'' ..:::                      '' ::..':' :'  .....:'' :'.  ::.
  ..:::'  :::::::::'' ..::::::'' ..:::::::                       '' .::  :. '.: .''': :: .:.'' :
 :::'     ':::'' ...::::::''...::::::::::                       ': ' :':: ..: .'' : ::..: .'' :
::'         ...::::::'' ..:::::::::::::'                        . .:' :  ''':':'.:..' :'::':.'':
     ....:::::::'' ..:::::::::::::::::'                         ::.: :' . '.:. .. .' ...'.:':.'
'::::::::'''    :::::::::::::::::::''                           . :: ::' :' ':' .'...'':  ' ':':
                  '':::::::::::'''                              :::'. '.'. :::   . . '.' '  '..'
```

Often overlooked, the humble [CRC][crc] can already provide a simple form
of error detection and correction, capable of repairing a handful of
bit-errors.

It does scale poorly, $O(n^e)$, but if you're only worried about the
occasional one or two bit errors, it may be sufficient. It's hard to
beat the simplicity, low-cost, and hardware availability of CRCs.

This block device uses littlefs's CRC-32, since we assume it's already
available. But the same idea can be extended to any other CRC, as long
as it has a sufficient [Hamming distance][hamming-distance] for the
expected number of bit-errors.

A quick comparison of current ram-ecc-bds:

|            | code   | tables | stack | buffers  | runtime                  |
|:-----------|-------:|-------:|------:|---------:|-------------------------:|
| ramcrc32bd |  940 B |   64 B |  88 B |      0 B |      $O\left(n^e\right)$ |
| ramrsbd    | 1430 B |  512 B | 128 B | n + 4e B | $O\left(ne + e^2\right)$ |

See also:

- [littlefs][littlefs]
- [ramrsbd][ramrsbd]

### RAM?

Right now, [littlefs's][littlefs] block device API is limited in terms of
composability. It would be great to fix this on a major API change, but
in the meantime, a RAM-backed block device provides a simple example of
error-correction that users may be able to reimplement in their own
block devices.

### Testing

Testing is a bit jank right now, relying on littlefs's test runner:

``` bash
$ git clone https://github.com/littlefs-project/littlefs -b v2.9.3 --depth 1
$ make test -j
```

### How it works

First, a quick primer on [CRCs][crc].

Some of why CRCs are so prevalent because they are mathematically quite
pure. You view your message as a big [binary polynomial][binary-polynomial],
divide it by a predetermined "generator polynomial" (choosing a good
polynomial is the hard part), and the remainder is your CRC:

```
message = "hi!":
    = 01101000 01101001 00100001

polynomial = 0x107:
    = 1 00000111

binary division:
    = 01101000 01101001 00100001 00000000
    ^  1000001 11
    = 00101001 10101001 00100001 00000000
    ^   100000 111
    = 00001001 01001001 00100001 00000000
    ^     1000 00111
    = 00000001 01110001 00100001 00000000
    ^        1 00000111
    = 00000000 01110110 00100001 00000000
    ^           1000001 11
    = 00000000 00110111 11100001 00000000
    ^            100000 111
    = 00000000 00010111 00000001 00000000
    ^             10000 0111
    = 00000000 00000111 01110001 00000000
    ^               100 000111
    = 00000000 00000011 01101101 00000000
    ^                10 0000111
    = 00000000 00000001 01100011 00000000
    ^                 1 00000111
    = 00000000 00000000 01100100 00000000
    ^                    1000001 11
    = 00000000 00000000 00100101 11000000
    ^                     100000 111
    = 00000000 00000000 00000101 00100000
    ^                        100 000111
    = 00000000 00000000 00000001 00111100
    ^                          1 00000111
    -------------------------------------
    = 00000000 00000000 00000000 00111011
                                 '--.---'
        .---------------------------'
        v
crc = 0x3b
```

You can describe this mathematically in [GF(2)][gf2], but depending on
your experience with GF(2) and other finite-fields, the above example may
be easier to understand:

<p align="center">
<img
    alt="C(x) = M(x) x^{|P|} - \left(M(x) x^{|P|} \bmod P(x)\right)"
    src="https://latex.codecogs.com/svg.image?C%28x%29%20%3d%20M%28x%29%20x%5e%7b%7cP%7c%7d%20%2d%20%5cleft%28M%28x%29%20x%5e%7b%7cP%7c%7d%20%5cbmod%20P%28x%29%5cright%29"
>
</p>

The extra $x^{|P|}$ multiplications represent shifting the message to
make space for the CRC, and gives us what's called a
["systematic code"][systematic-code]. Alternatively we could actually
multiply the message with our polynomial to get valid codewords, but that
would just make interacting with the message more annoying without much
benefit...

The neat thing is that this remainder operation does a real good job of
mixing up all the bits. So if you choose a good CRC polynomial, it's very
unlikely a message with a bit-error will result in the same CRC:

```
a couple bit errors:
    = 01101010 01101001 00100001 00000000 => 11101101 (0xed != 0x3b)
    = 01101000 01101000 00100001 00000000 => 00101110 (0x2e != 0x3b)
    = 01101000 01101001 01100001 00000000 => 11111100 (0xfc != 0x3b)
    = 01101000 01101001 00100001 00001000 => 00110011 (0x33 != 0x3b)
```

How unlikely? Well thanks to Philip Koopman's [exhaustive CRC work][koopman-crc],
we know exactly how many bit errors we need to see a collision for a
given CRC polynomial and message size. This is called the
[Hamming distance][hamming-distance], and is a very useful metric for an
error-correcting code.

For this 8-bit CRC, p=0x107, Philip Koopman's work shows a Hamming
distance of 4 up to a message size of 119 bits (14 bytes), which means
our 3-byte message should have no collisions up until 4 bit-errors:

```
a collision:
    = 01101000 01101000 00100110 00000000 => 00111011 (0x3b == 0x3b)
```

But the interesting thing about Hamming distance is that it's, well, a
distance.

A Hamming distance of 4 means that there are at least 3 invalid codewords
between every valid codeword:

```
                      Hamming distance = 4
.-----------------------------'-----------------------------.

o <-bit-flip-> x <-bit-flip-> x <-bit-flip-> x <-bit-flip-> o
^              '--------------.--------------'              ^
|                             |                             |
valid codeword         invalid codewords       valid codeword
```

If we assume our message has a single bit-error, it will be 1 bit-flip
away from the original codeword, and at least 3 bit-flips away from any
other codeword. It's not until we have 2 bit-errors that the original
codeword becomes ambiguous.

But this is only an 8-bit CRC. With more bits, we can find a better CRC.
littlefs's 32-bit CRC, for example, has a Hamming distance of 7 up to
171 bits (21 bytes), which means for any message <=21 bytes we can
reliably correct up to 3 bit-errors.

In general the number of bits we we can reliably correct is
$\left\lfloor\frac{HD-1}{2}\right\rfloor$.

---

Ok, but that's enough about theory. How do actually correct these
bit-errors?

The simple/naive/cheap answer is brute force. Try every bit-flip until we
find a matching CRC. Since we know our Hamming distance is >=3, this
should only ever find one valid codeword, the original codeword:

```
brute force search:
    = 01101000 01101001 01100001 00000000 => 11111100 (0xfc != 0x3b)
    ^ 1                                   => 11110111 (0xf7 != 0x3b)
    ^  1                                  => 01111010 (0x7a != 0x3b)
    ^   1                                 => 10111111 (0xbf != 0x3b)
    ^    1                                => 01011110 (0x5e != 0x3b)
    ^     1                               => 10101101 (0xad != 0x3b)
    ^      1                              => 01010111 (0x57 != 0x3b)
    ^       1                             => 00101010 (0x2a != 0x3b)
    ^        1                            => 10010111 (0x97 != 0x3b)
    ^          1                          => 01001010 (0x4a != 0x3b)
    ^           1                         => 10100111 (0xa7 != 0x3b)
    ^            1                        => 01010010 (0x52 != 0x3b)
    ^             1                       => 10101011 (0xab != 0x3b)
    ^              1                      => 01010100 (0x54 != 0x3b)
    ^               1                     => 10101000 (0xa8 != 0x3b)
    ^                1                    => 11010110 (0xd6 != 0x3b)
    ^                 1                   => 11101001 (0xe9 != 0x3b)
    ^                   1                 => 01110101 (0x75 != 0x3b)
    ^                    1                => 00111011 (0x3b == 0x3b) !!! found our bit-error

corrected message:
    = 01101000 01101001 00100001 00000000 => 00111011 (0x3b == 0x3b)
```

If we don't find a valid codeword, we must have had at least 2
bit-errors, making our original codeword unrecoverable.

This idea can be extended to CRCs with larger Hamming distances by brute
force searching multiple bit-errors with nested loops. See
[ramcrc32bd_read][ramcrc32bd_read] for an example of up to 3 bit-errors
with littlefs's CRC-32.

### Tricks

There are a couple implementation tricks worth noting in ramcrc32bd:

1. Try the faster solutions first.

   Correcting 1 bit-error $O(n)$, is much faster than correcting
   2 bit-errors $O(n^2)$, and 1 bit-errors are also much more common. It
   makes sense to only search for more bit-errors when a solution with
   fewer bit-errors can't be found.

   By trying fewer bit-errors first, ramcrc32bd should return quickly in
   the common case of few/no bit-errors.

   Though this does risk degraded performance over time as bit-errors
   develop.

2. We don't actually need to permute the message to try every bit-flip.

   First note that since CRCs are a glorified remainder operation,
   shifting a message (multiplying by $x$ in GF(2)) and then calculating
   the CRC is equivalent to shifting the CRC and then calculating the
   remainder:

   ```
   crc(a << 1):
       = 00111001 10110100 00110110 00000000 => 01000010 (0x42)
       s 01110011 01101000 01101100 00000000 => 10000100 (0x84)
       s 11100110 11010000 11011000 00000000 => 00001111 (0x0f)

   (crc(a) << 1) % p:
       = 00111001 10110100 00110110 00000000 =>     01000010 (0x42)
                                                s 0 10000100
                                                ^ 0 00000000
                                                =   10000100 (0x84)
                                                s 1 00001000
                                                ^ 1 00000111
                                                =   00001111 (0x0f)
   ```

   We can use this to quickly iterate through all CRCs that represent a
   single bit:

   ```
   a = (a << 1) % p:
       =                            00000001 => 00000001 (0x01)
       s                            00000010 => 00000010 (0x02)
       s                            00000100 => 00000100 (0x04)
       s                            00001000 => 00001000 (0x08)
       s                            00010000 => 00010000 (0x10)
       s                            00100000 => 00100000 (0x20)
       s                            01000000 => 01000000 (0x40)
       s                            10000000 => 10000000 (0x80)
       s                         (1)00000000 => 00001110 (0x0e)
       s                        (1) 00000000 => 00011100 (0x1c)
       s                       (1)  00000000 => 00111000 (0x38)
       s                      (1)   00000000 => 01110000 (0x70)
       s                     (1)    00000000 => 11100000 (0xe0)
       s                    (1)     00000000 => 11000111 (0xc7)
       s                   (1)      00000000 => 10001001 (0x89)
       s                  (1)       00000000 => 00010101 (0x15)
   ```

   Combining this with the fact that CRCs are linear, i.e. the CRC of the
   xor of two messages (addition in GF(2)) is equivalent to the xor of
   two CRCs:

   ```
   crc(a ^ b):
       = 01100001 01100100 01100100 00000000
       ^ 01111000 01101111 01110010 00000000
       = 00011001 00001011 00010110 00000000 => 01101101 (0x6d)

   crc(a) ^ crc(b):
       = 01100001 01100100 01100100 00000000 =>   00110100 (0x34)
       ^ 01111000 01101111 01110010 00000000 => ^ 01011001 (0x59)
                                                = 01101101 (0x6d)
   ```

   And we can quickly test the affect of every possible bit-flip by
   shifting a single register per simulated bit-flip and xoring it
   into our original CRC:

   ```
   fancy brute force search:
       = 01101000 01101001 01100001 00000000 => 11111100 (0xfc != 0x3b)
       ^                            00000001 => 11111101 (0xfd != 0x3b)
       ^                            00000010 => 11111110 (0xfe != 0x3b)
       ^                            00000100 => 11111000 (0xf8 != 0x3b)
       ^                            00001000 => 11110100 (0xf4 != 0x3b)
       ^                            00010000 => 11101100 (0xec != 0x3b)
       ^                            00100000 => 11011100 (0xdc != 0x3b)
       ^                            01000000 => 10111100 (0xbc != 0x3b)
       ^                            10000000 => 01111100 (0x7c != 0x3b)
       ^                         (1)00000111 => 11111011 (0xfb != 0x3b)
       ^                        (1) 00001110 => 11110010 (0xf2 != 0x3b)
       ^                       (1)  00011100 => 11100000 (0xe0 != 0x3b)
       ^                      (1)   00111000 => 11000100 (0xc4 != 0x3b)
       ^                     (1)    01110000 => 10001100 (0x8c != 0x3b)
       ^                    (1)     11100000 => 00011100 (0x1c != 0x3b)
       ^                   (1)      11000111 => 00111011 (0x3b == 0x3b) !!! found our bit-error

   corrected message:
       = 01101000 01101001 00100001 00000000 => 00111011 (0x3b == 0x3b)
   ```

   The end result is still $O(n^e)$, but limited only by your CPU's
   shift, xor, and branching hardware. No memory accesses required.

   See [ramcrc32bd_read][ramcrc32bd_read] for an implementation of this.

### Caveats

And some caveats:

1. For _any_ error-correcting code, attempting to _correct_ errors
   reduces the code's ability to _detect_ errors.

   In the HD=4 example, we assumed 1 bit-error. If we were wrong and
   there were actually 3 bit-errors, we would have "corrected" to the
   wrong codeword.

   In practice this isn't _that_ big of a problem. 1 bit-errors are
   usually much more common than 3 bit-errors, and at 4 bit-errors you're
   going to have a collision anyways.

   Still, it's good to be aware of this tradeoff.

   ramcrc32bd's [`error_correction`][error-correction] config option lets
   you control exactly how many bit-errors to attempt to repair in case
   better detection is more useful.

2. Brute force doesn't really scale.

   The error-correction implemented here grows $O(n^e)$ for $e$
   bit-errors, which really isn't great.

   That being said, larger CRC Hamming distances are also pretty limited
   in terms of message size, so this performance may be excusable if
   messages are small and bit-errors are rare.

   ramcrc32bd's [`error_correction`][error-correction] config option can
   also help here by limiting how many bit-errors we attempt to repair.
   If you set `error_correction=1`, for example, the runtime reduces to
   $O(n)$ worst case, which is roughly the same runtime it takes to read
   the data from the underlying storage.

   But if you need a performant error-correcting block device, consider
   ramcrc32bd's big brother, [ramrsbd][ramrsbd], which brings the
   decoding cost down to $O(ne + e^2)$.

---

https://users.ece.cmu.edu/~koopman/crc/c08/0x83_len.txt
https://users.ece.cmu.edu/~koopman/crc/c32/0x82608edb_len.txt


