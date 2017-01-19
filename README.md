I'm terribly annoyed by the fact that grep(1) cannot look for binary
strings. I'm even more annoyed by the fact that a simple search for 
"binary grep" doesn't yield a tool which could do that. So I ~~wrote one~~ *gratuitously forked one*.

**Original work by [tmbinc/bgrep](https://github.com/tmbinc/bgrep)**

Feel free to modify, branch, fork, improve. Re-licenses as BSD.

## Building
First, you need to have [make](https://www.gnu.org/software/make/manual/make.html), [gcc](https://gcc.gnu.org/), [automake](https://www.gnu.org/software/automake/), and [gnulib](https://www.gnu.org/software/gnulib/) installed.
On Debian and derivatives:
```
sudo apt-get install build-essential gnulib
```

Once you have those tools, it is the normal autotools build process.
```
./configure
make
make check
```
And optionally: (*note: this part is untested*)
```
sudo make install
```

## How `bgrep` works

```
$ src/bgrep -h
bgrep version: 0.3
usage: src/bgrep [-hFHbclr] [-s BYTES] <hex> [<path> [...]]

   -h         Print this help
   -F         Stop scanning after the first match
   -H         Print the file name for each match
   -b         Print the byte offset of each match INSTEAD of xxd-style output
   -c         Print a match count for each file (disables offset/context printing)
   -l         Suppress normal output; instead print the name of each input file from
              which output would normally have been printed. Implies -F
   -r         Recurse into directories
   -s BYTES   Skip forward by BYTES before searching

      Hex examples:
         ffeedd??cc        Matches bytes 0xff, 0xee, 0xff, <any>, 0xcc
         "foo"           Matches bytes 0x66, 0x6f, 0x6f
         "foo"00"bar"   Matches "foo", a null character, then "bar"
         "foo"??"bar"   Matches "foo", then any byte, then "bar"

      BYTES may be followed by the following multiplicative suffixes:
         c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M
         GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.

```

## Examples
### Basic (xxd-style) usage
```
$ echo "1234foo89abfoof0123" | bgrep \"foo\"
0000004: 666f 6f                                  foo
000000b: 666f 6f                                  foo
```
### Use `xxd -r` to convert `bgrep` output back to bytes

*Note:* `xxd -r` pads with null characters you can't see here!
```
$ echo "1234foo89abfoof0123" | bgrep \"foo\" | xxd -r -c3 ; echo
foofoo
```
### Use `xxd -r | xxd` to understand the limitations of `xxd -r`
```
$ echo "1234foo89abfoof0123" | bgrep \"foo\" | xxd -r -c3 | xxd
0000000: 0000 0000 666f 6f00 0000 0066 6f6f       ....foo....foo
```
### Print just the byte offset of each match
```
$ echo "1234foo89abfoof0123" | bgrep -b \"foo\"
00000004
0000000b
```
### Count the matches
```
$ echo "1234foo89abfoof0123" | bgrep -c \"foo\"
2
```
### Find-first option
```
$ echo "1234foo89abfoof0123" | bgrep -Fb \"foo\"
00000004
```
### Overlapping matches
```
$ echo "oofoofoofoo" | bgrep \"foof\"
0000002: 666f 6f66 6f6f 66
```
### This time with filenames and byte offsets
```
$ echo "oofoofoofoo" | bgrep -Hb \"foof\"
stdin:00000002
stdin:00000005
```
### Wildcard matches
```
$ echo "oof11f22foo" | bgrep -Hb '66????66'
stdin:00000002
stdin:00000005
```
### Get the wildcard bytes with xxd-style output
```
$ echo "oof11f22foo" | bgrep '66????66'
0000002: 6631 3166 3232 66                        f11f22f
```
### Skip forward in the file
```
$ echo "oof11f22foo" | bgrep  -s 3 '66????66'
0000005: 6632 3266                                f22f
```
### Skip ahead using `dd`-style byte counts
```
$ (dd if=/dev/urandom bs=1 count=2k status=none; echo foo; dd if=/dev/urandom bs=1 count=1k status=none) \
   | bgrep -s 1k \"foo\"
0000800: 666f 6f                                  foo
```
### Extreme example

Suppose you have a corrupt bzip2-compressed TAR file called `backups.tar.bz2`.  You want to locate TAR headers within this file.  You do know that all the paths start with "home", since it was your /home volume you had backed up.

Looking [here](https://www.gnu.org/software/tar/manual/html_node/Standard.html), you learn that a filename is typically in the `name[100]` field of the TAR file header.  There is a magic number `magic[6]`, starting 257 bytes after the start of `name[100]`.
The magic number for GNU TAR files is "ustar  ".

Let's get to work breaking down and reconstructing that archive!  First we take advantage of bzip2's block CRC property to find non-corrupt blocks.

```
$ bzip2recover backups.tar.bz2
```
This produces many small files, each called "rec00001backups.tar.bz2", "rec00002backups.tar.bz2", etc.  We can then string them together until we hit a bad one, searching for TAR headers...

```
$ bzcat rec*backups.tar.bz2 | bgrep -f  '"home"??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????"ustar  "'
```
This will locate the first place in the stream where "ustar  " is preceeded by "home", 257 bytes earlier (257-4==253 wildcard bytes).  This is a strong indication that you've found the first GNU tar header.  We can then split the archive at the tar header (tar can expand any files if you start it at a header), then reconstruct files up to the next bad bzip2 block.  After the bad block, we can resume this process as needed until we squeeze all we can out of that corrupt archive.

