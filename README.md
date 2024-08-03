### About
`TIGHT` is a lossless file compression library and a binary written in C (C99)
compatible mostly with (and only tested on) GNU/Linux.
Two main algorithms used for compressing are [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding)
and [LZW](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch), best possible compression
results are achieved by first compressing files with `LZW` then generating `Huffman` codes on top of it.
That is exactly how the `tight` binary preforms full compression.
**Note: run-length-encoding (`LZW`) is not yet implemented.**

`TIGHT` is not meant to be a replacement for any of the already established and much more
fine tuned, smarter implementations of mentioned compression algorithms, instead it is a naive
implementation intended for educational purposes. Source code is kept minimal
and is easy to read (I hope).

---
#### TODO
- Implement Vitter algorithm.
- Implement RLE LZW algorithm.
- Combine Vitter and LZW in order to read the file only once.
- Make the library portable on OSes defined in `tinternal.h`

---
### Build
Clone the repo:
```sh
git clone https://github.com/b-jure/TIGHT.git
cd TIGHT
```
Build as `tight` binary:
```sh
make
```
Build as `libtight.so` shared library:
```sh
make library
```
Build as `libtight.a` archive:
```sh
make archive
```

---
### Install & Uninstall
Note: add `sudo` in front of `make` if needed.

Install/Uninstall `tight` binary:
```sh
make install
make uninstall
```
Install/Uninstall `libtight.so` shared library:
```sh
make install-lib
make uninstall-lib
```
