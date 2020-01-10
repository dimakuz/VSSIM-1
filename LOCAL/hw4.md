Problem Set 4
=============

Dry assignment / Intra-SSD Compression
---------------------------------------

### a


### b


### c

A good garbage collection algorithm will strive to vacate the most space with least amount of IO. Without compression, picking block with most obsolete pages works because each page uses up the same amount of real-estate.

With compresstion enabled, pages take up variable sized chunks of the block. One block can have a lot of obsolete pages that do not page up a lot of space, while another block can have less obsolete pages that take up a lot of space. E.g.

```
"One block":
+-------+-------+---+---+---+---------+
|       |       |   |   |   |         |
|       |       | X | X | X |         |
|       |       |   |   |   |         |
+-------+-------+---+---+---+---------+

"Another block":
+-------+-------+---+---+-------------+
|       |       |   |   |             |
|       |   X   |   |   |     X       |
|       |       |   |   |             |
+-------+-------+---+---+-------------+
Invalid pages marked by X

```

We suggest to revise the garbage collection policy to prefer blocks where the sum of compressed sized of all valid pages is maximal. This way each eviction has to deal with less data and target block will have the most vacant space.

### d

Copyback command allows copy of data between 2 storage locations, without passing this data through the host (SSD controller in our case). This saves time - controller can perform other tasks, additionally, copyback commands can be parallelized if performed on disjoint physical resources.

Garbage collection requires copying data between blocks. Copyback speeds up this process because actual copying is off-loaded to the chips.

If compression mechanism is enabled, we might not be able to use copyback for all of our IO operations. In the no-compression case, we copy pages between block, and those pages are aligned to block boundaries. Once we enable compression, pages are no longer aligned. Copyback will be useful only in very specific cases where whole page is copies as is. E.g.:

```
Victim block

+----+-------+-----+----------+----+------+-----+
|    |       |     |          |    |      |     |
|    |       |     |     X    |    |   X  |  X  |
|    |       |     |          |    |      |     |
+----+-------+-----+---------------+------+-----+
#   #   #   #   #   #   #   # | #   #   #   # 
| Copyback range    | +-------+
#   #   #   #   #   # | #   #   #   #   #   #
+----+-------+-----+--v-+-----------------------+
|    |       |     |    |                       |
|    |       |     |    |                       |
|    |       |     |    |                       |
+----+-------+-----+----+-----------------------+

Target block                   # <- Page boundary
```

In the example above, the first several pages can be copied directly with copyback because they are aligned and whole-pages. The last page cannot be copied with copyback and will have to be though the controller. This is however a very optimistic example, and for real workloads, we anticipate a very small percentage of pages to be copied with copyback.

### e

Trim operation will be performed in similar manned to uncompressed implementation - by invalidating the entry in the mappings. However, due to compression, we are no longer invalidating an entire page of flash, but rather a sub-page region (or two if page was stored cross-page boundary). Once page is marked as invalid, the above garbage collection mechanism will take care of recycling the space.

### f

Secure trim ensures data becomes unrecoverable, as such, to achieve secure trim, it not enough to unmap the page from the relevant mapping tables and mark the data as invalid.

One simple way to implement secure trim is to evict (and wipe) the whole block of the page. This ensures data will not be recoverable but also quite slow and wasteful (high WA).

Another, more efficient (yet more complex) way is to implement intra-SSD encryption. In this scheme, each page has some key (length depending on desired security level).
When page is written, a key is enrolled, stored, and page is encryped prior to writing to flash. When page is read, the key is retrieved, and page is decrypted.

Because our pages are compressed and of variable size, we can use a stream cipher and receive a more-or-less similarly sized ciphertext. One simple implementation for such cipher would be XORing with LFSR output, where key serves as initialization of the LFSR.

### g

In order to recover the data from our proposed drive, the following steps should be taken:

* Page mapping should be reconstructed, by reading the blocks dedicated to holding the mapping tables. (Inverse mappings are not required as we're not going to move pages).

* (If we implement per-page encryption) we'll need to read the encryption keys of the pages.

To read a specific page:

* Take the LBA of the page, and with the recovered mapping table retrieve `(PBA, offset, len)` of the page.
* Read `len` bytes from `PBA` at offset `offset`.
* Decrypt the page using key from recovered key table at `LBA` index.
* Decompress the decrypted key.

The page's data is now available.