## ramcrc32bd

An example of a CRC-32 based error-correcting block device backed by RAM.

Often overlooked, the humble [CRC][crc] can already provide a simple form
of error detection and correction, capable of repairing a handful of
bit-errors.

It does scale poorly, $O(n^e)$, but if you're only worried about the
occasional one or two bit errors, this may be sufficient. It's hard to
beat the simplicity, low-cost, and hardware available for CRCs.

This block device uses littlefs's CRC-32, since we assume it's already
available. But the same idea can be extended to any other CRC, as long
as it has a sufficient [Hamming distance][hamming-distance] for the
expected number of bit-errors.

See also:

- [littlefs][littlefs]
- [ramrsbd][ramrsbd]

## RAM?

Right now, [littlefs's][littlefs] block device API is limited in terms of
composability. It would be great to fix this on a major API change, but
in the meantime, a RAM-backed block device provides a simple example of
error-correction that users may be able to reimplement in their own
block devices.

## How it works

First, a quick primer on [CRCs][crc].

Some of why CRCs are so prevalent because they are mathematically quite
pure. You view your message as a big [binary polynomial][binary-polynomial],
divide it by the CRC polynomial (choosing a good CRC polynomial is the
hard part), and the remainder is your CRC:

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
crc = 0x3b <------------------------'
```

You can describe this mathematically in [$GF(2)$][gf2] (the extra $x^|P|$
represents shifting the message to make space for the CRC), but the
above example is probably easier to understand:

$$
C(x) = M(x) - (M(x) \bmod P(x))
$$

The neat thing is that this remainder operation does a real good job of
mixing up all the bits. So if you choose a good CRC polynomial, it's very
unlikely a message with a bit-error will result in the same CRC.

```
a couple bit errors:
    = 01101010 01101001 00100001 00000000 => ........ (0x.. != 0x..)
    = 01101000 01101000 00100001 00000000 => ........ (0x.. != 0x..)
    = 01101000 01101001 01100001 00000000 => ........ (0x.. != 0x..)
    = 01101000 01101001 00100001 00000100 => ........ (0x.. != 0x..)
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
TODO
```

But the interesting thing about Hamming distance is that it's, well, a
distance.

A Hamming distance of 4 means that there is at least 3 invalid codewords
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

Of course more bits generally means a better CRC. littlefs's CRC-32, for
example, has a Hamming distance of 7 up to 171 bits (21 bytes). This
means we can reliably correct up to 3 bit-errors in any message <= 21
bytes (12 byte message + 4 byte CRC makes for a nice 16 byte codeword,
for example).

In general the number of bits we we can reliably correct is
$\left\lfloor\frac{HD-1}{2}\right\rfloor$.

---

Ok, but that's enough about theory. How do actually correct these
bit-errors?

The answer is brute force. Try every bit-flip until we find a matching
CRC. Since we know our Hamming distance is >=3, this should only ever
find one valid codeword, the original codeword:

```
TODO
```

If we don't find a valid codeword, we must have had at least 2
bit-errors, making our original codeword unrecoverable.

This idea can be extended to CRCs with larger Hamming distances by brute
force searching multiple bit-errors with nested loops. See
[ramcrc32bd_read][ramcrc32bd_read] for an example of up to 3 bit-errors
with littlefs's CRC-32.

### Tradeoffs

Two big tradeoffs:

1. For any error-correcting code, attempting to _correct_ errors reduces
   the code's ability to _detect_ errors.

   In the HD=4 example, when we assumed 1 bit-error, if there were
   actually 3 bit-errors, we would have assumed wrong and "corrected" to
   the wrong codeword.

   In practice this isn't _that_ big of a problem. 1 bit-errors are
   usually much more common than 3 bit-errors, and at 4 bit-errors you're
   going to have a collision anyways. Still, it's good to be aware of.

   ramcrc32bd's [`error_correction`][error-correction] lets you control
   exactly how many bit-errors to attempt to repair in case detection is
   more valuable.

2. Brute force doesn't really scale.

   The error-correction implemented here grows $O(n^e)$ for $e$
   bit-errors, which isn't really great.

   But for larger Hamming distances, CRCs are already pretty limited in
   terms of message size, <=21 bytes for 3 bit-errors with CRC-32 for
   example. So this may not be that big of a deal in practice.

   ramcrc32bd's [`error_correction`][error-correction] can also help here
   by limiting how many bit-errors to attempt to repair. If you set
   `error_correction=1` and only repair 1-bit errors, the runtime reduces
   to to $O(n)$, about the same runtime it takes to actually read the
   message.

### Tricks

There are a few tricks worth noting in ramcrc32bd:

1. Try the faster solutions first.

   Correcting 1 bit-error, $O(n)$, is much faster than correcting
   2 bit-errors, $O(n^2)$, and 1 bit-errors are also much more common. It
   makes sense to only search for more bit-errors when a solution with
   fewer bit-errors could not be found.

   This means ramcrc32bd should read quite quickyl in the common case of
   few/none bit-errors. Though this does risk reads sort of slowing down
   as bit-errors develop.

2. We don't actually need to permute the message for every bit-flip.

   CRCs are pretty nifty in that they're linear. The CRC of the xor
   of two messages is equivalent to the xor of the two CRCS:

   ```
   TODO
   ```

   This means we can quickly check the effect of a bit-flip by xoring our
   CRC with the CRC of the bit flip.

   If this wasn't convenient enough, shifting (multiplying by 2) is also
   preserved over CRCs:

   ```
   TODO
   ```

   So testing every bit-flip requires only a single register that we
   repeatedly shift and xor until we find a CRC match (or don't).

   Once we find a match, we can use the number of shifts to figure out
   which bit we needed to flip in the original message.

   Still $O(n^e)$, but limited only by your CPU's shift, xor, and
   branching hardware. No memory required.

   See [ramcrc32bd_read][ramcrc32bd_read] for an implementation of this.





vvv TODO scratch vvv

   allows control over the number of bit-errors to attempt to repair in case detection


, 1 bit-error is much more common than 3 bit-errors, so
   this
   
by assuming 1 bit-error, we're basically saying
   there can't be any 
   3 bit-errors. 






Of course the downside of brute force is that it doesn't really scale,
growing $O(n^e)$ for $e$ bit-errors. But for CRCs the larger Hamming
distances are already pretty limited in terms of message size (<=21 bytes
for 3 bit-errors with CRC-32), so it may not be that big of a deal in
practice.

 this may not be that big of a problem.



 If we
wanted to align this to a power-of-two, we could 


This
means we can reliably correct up to 3 bit-errors in a 21 byte message. 








vvv TODO vvv

It's useful to think of the Hamming distance as, well, a distance between
valid codewords. Every bit-error moves our message+CRC away from the
original codeword, but as long as there is enough distance between
codewords, we can probably guess which one it was.

Take our Hamming distance of 4 for example. With a Hamming distance of 4,
there are 3 invalid codewords between every valid codeword:

```
                      Hamming distance = 4
.-----------------------------'-----------------------------.

o <-bit-flip-> x <-bit-flip-> x <-bit-flip-> x <-bit-flip-> o
^              '--------------.--------------'              ^
|                             |                             |
valid codeword         invalid codewords       valid codeword
```

Consider an increasing number of bit-errors in our original message:

1. 1 bit-error - There is only one valid codeword a single bit-flip away,
   so in theory we can find it and correct the error.

2. 2 bit-errors - Hmmm, we may be equidistant between two valid
   codewords. We can't be sure which one was the original codeword, so
   we can't correct the errors.

3. 3 bit-errors - Uh oh, we can find a valid codeword a single bit-flip
   away, but this won't be our original codeword. We can't tell the
   difference between 3-bit errors and 1-bit errors so our error
   correction falls apart here.

If our message has 1 bit-error, there's only one codeword that is 

If we have 1 bit-error, there is only one codeword 


TODO WIP TODO BLABLABLA

A Hamming distance of 4 means there are 3 invalid codewords between every
valid codeword.





codeword, but as long as there is enough di

It's useful to think of the message + CRC as a codeword, and the Hamming
distance as, well, the distance between valid codewords. Every bit-error
moves our message away from the original codeword, but as long as there
is enough distance between valid codewords, we can probably guess which
one it was.


=-=====-=-=------=-=-=-

https://users.ece.cmu.edu/~koopman/crc/c08/0x83_len.txt
https://users.ece.cmu.edu/~koopman/crc/c32/0x82608edb_len.txt


