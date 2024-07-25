### About
`TIGHT` is a lossless file compression library and a binary written in C (C99)
compatible mostly with (and only tested on) GNU/Linux.
Two main algorithms used for compressing are [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding)
and [LZW](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch), best possible compression
results are achieved by first compressing files with `LZW` then generating `Huffman` codes on top of it.
That is exactly how the `tight` binary preforms full compression.

`TIGHT` is not meant to be a replacement for any of the already established and much more
fine tuned, smarter implemented compression algorithms, instead it serves as a
simple easy to read piece of source code intended for educational purposes.
Of course any contributions are still welcome.
