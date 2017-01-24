# Intro
I'm terribly annoyed by the fact that grep(1) cannot look for binary
strings. I'm even more annoyed by the fact that a simple search for 
"binary grep" doesn't yield a tool which could do that. So I ~~wrote one~~ *gratuitously forked one*.

**Original work by [tmbinc/bgrep](https://github.com/tmbinc/bgrep)**

Feel free to modify, branch, fork, improve. Re-licenses as BSD.
# "Man" page

```
$ bgrep --help
Usage: bgrep [OPTION...] PATTERN [FILE...]
  or:  bgrep [OPTION...] --hex-pattern=PATTERN [FILE...]
  or:  bgrep [OPTION...] -x PATTERN [FILE...]
Search for a byte PATTERN in each FILE or standard input

  -b, --byte-offset          show byte offsets; disables xxd output mode
  -c, --count                print a match count for each file; disables xxd
                             output mode
  -l, --files-with-matches   print the names of files containing matches;
                             implies 'first-only'; disables xxd output mode
  -q, --quiet                suppress all normal output; implies 'first-only'
  -F, --first-only           stop searching after the first match in each file
  -H, --with-filename        show filenames when reporting matches
  -r, --recursive            descend recursively into directories
  -A, --after-context=BYTES  print BYTES of context after each match if
                             possible (xxd output mode only)
  -B, --before-context=BYTES print BYTES of context before each match if
                             possible (xxd output mode only)
  -C, --context=BYTES        print BYTES of context before and after each match
                             if possible (xxd output mode only)
  -s, --skip=BYTES           skip or seek BYTES forward before searching
  -x, --hex-pattern=PATTERN  use PATTERN for matching
  -?, --help                 give this help list
      --usage                give a short usage message
  -V, --version              print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

 PATTERN may consist of the following elements:
    hex byte values:                '666f6f 62 61 72'
    quoted strings:                 '"foobar"'
    wildcard bytes:                 '??'
    groupings:                      '(66 6f 6f)'
    repeated bytes/strings/groups:  '(666f6f)*3'
    escaped quotes in strings:      '"\"quoted\""'
    any combinations thereof:       '(("foo"*3 ??)*1k ff "bar") * 2'

 More examples:
    'ffeedd??cc'            Matches bytes 0xff, 0xee, 0xdd, <any byte>, 0xcc
    '"foo"'                 Matches bytes 0x66, 0x6f, 0x6f
    '"foo"00"bar"'          Matches "foo", a null character, then "bar"
    '"foo"??"bar"'          Matches "foo", then any byte, then "bar"
    '"foo"??*10"bar"'       Matches "foo", then exactly 10 bytes, then "bar"

 BYTES and REPEAT may be followed by the following multiplicative suffixes:
   c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M
   GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.

 FILE can be a path to a file or '-', which means 'standard input'

Report bugs to <https://github.com/rsharo/bgrep/issues>.
```
# Build Instructions
First, you need to have [gcc](https://gcc.gnu.org/), [make](https://www.gnu.org/software/make/manual/make.html),
[automake](https://www.gnu.org/software/automake/), and [autoconf](https://www.gnu.org/software/autoconf/autoconf.html)
installed.  You may also need [gnulib](https://www.gnu.org/software/gnulib/) if you're building from a machine with no
network access: gnulib normally auto-downloads during the `bootstrap` script.

On Debian and derivatives:
```bash
sudo apt-get install build-essential
```

Once you have those tools, it is the normal autotools build process:
```bash
./bootstrap
./configure
make
```
The binary is saved as `src/bgrep`

>Optional build steps:
```bash
make check
sudo make install
```
*Note*: `make check` requires [xxd](https://github.com/ThatOtherPerson/xxd) to be installed as well.  It is readily available in Debian, Redhat, Cygwin, and derivative repos.

# Examples
### Basic (xxd-style) usage
```bash
$ echo "1234foo89abfoof0123" | bgrep \"foo\"
0000004: 666f 6f                                  foo
000000b: 666f 6f                                  foo
```
### Use `xxd -r` to convert `bgrep` output back to bytes

*Note:* `xxd -r` pads with null characters you can't see here!
```bash
$ echo "1234foo89abfoof0123" | bgrep \"foo\" | xxd -r -c3 ; echo
foofoo
```
> ### *Note:* Limitations of `xxd`
> `xxd -r` inserts (zero-valued) padding bytes whenever it sees a discontinuity on its input. It also believes every line of
> input contains its configured number of columns (16 by default).  If you give it two lines of data that "overlap", it will
> try to seek backward in the output file.  Pipes, standard output, and some devices do not support backward seek.
```bash
$ echo "1234foo89abfoof0123" | bgrep \"foo\" | xxd -r | xxd
xxd: sorry, cannot seek backwards.
0000000: 0000 0000 666f 6f                        ....foo
```
> One workaround is to change the number of columns in xxd. But this only works if all your data is the same length.
```bash
$ echo "1234foo89abfoof0123" | bgrep \"foo\" | xxd -r -c3 | xxd
0000000: 0000 0000 666f 6f00 0000 0066 6f6f       ....foo....foo
```

### Print just the byte offset of each match
```bash
$ echo "1234foo89abfoof0123" | bgrep -b \"foo\"
00000004
0000000b
```
### Count the matches
```bash
$ echo "1234foo89abfoof0123" | bgrep -c \"foo\"
2
```
### Find-first option
```bash
$ echo "1234foo89abfoof0123" | bgrep -Fb \"foo\"
00000004
```
### Overlapping matches
```bash
$ echo "oofoofoofoo" | bgrep \"foof\"
0000002: 666f 6f66 6f6f 66                        foofoof
```
### This time with filenames and byte offsets
```bash
$ echo "oofoofoofoo" | bgrep -Hb \"foof\"
stdin:00000002
stdin:00000005
```
### Wildcard matches
```bash
$ echo "oof11f22foo" | bgrep -Hb '66????66'
stdin:00000002
stdin:00000005
```
### Get the wildcard bytes with xxd-style output
```bash
$ echo "oof11f22foo" | bgrep '66????66'
0000002: 6631 3166 3232 66                        f11f22f
```
### Use repeat groups to enter complex patterns
```bash
$ echo "a1a2a3a4a5bbbokok" | bgrep -H '(61??)*5 62*3 "ok"*2'
stdin:0000000: 6131 6132 6133 6134 6135 6262 626f 6b6f  a1a2a3a4a5bbboko
stdin:0000010: 6b                                       k
```
### Skip forward in the file
```bash
$ echo "oof11f22foo" | bgrep  -s 3 '66????66'
0000005: 6632 3266                                f22f
```
### Skip ahead using `dd`-style byte counts
```bash
$ (dd if=/dev/urandom bs=1 count=2k status=none; echo foo; dd if=/dev/urandom bs=1 count=1k status=none) \
   | bgrep -s 1k \"foo\"
0000800: 666f 6f                                  foo
```
### Extreme example

Suppose you have a corrupt bzip2-compressed TAR file called `backups.tar.bz2`.  You want to locate TAR headers within this file.  You do know that all the paths start with "home", since it was your /home volume you had backed up.

Looking [here](https://www.gnu.org/software/tar/manual/html_node/Standard.html), you learn that a filename is typically in the `name[100]` field of the TAR file header.  There is a magic number `magic[6]`, starting 257 bytes after the start of `name[100]`.
The magic number for GNU TAR files is "ustar  ".

Let's get to work breaking down and reconstructing that archive!  First we take advantage of bzip2's block CRC property to find non-corrupt blocks.

```bash
$ bzip2recover backups.tar.bz2
```
This produces many small files, each called "rec00001backups.tar.bz2", "rec00002backups.tar.bz2", etc.  We can then string them together until we hit a bad one, searching for TAR headers...

```bash
$ bzcat rec*backups.tar.bz2 | bgrep -F  '"home/" ??*252 "ustar  "'
```
This will locate the first place in the stream where "ustar  " is preceeded by "home", 257 bytes earlier (257-5==252 wildcard bytes).  This is a strong indication that you've found the first GNU tar header.  We can then split the archive at the tar header (tar can expand any files if you start it at a header), then reconstruct files up to the next bad bzip2 block.  After the bad block, we can resume this process as needed until we squeeze all we can out of that corrupt archive.

