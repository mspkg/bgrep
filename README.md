I'm terribly annoyed by the fact that grep(1) cannot look for binary
strings. I'm even more annoyed by the fact that a simple search for 
"binary grep" doesn't yield a tool which could do that. So I ~~wrote one~~ *gratuitously forked one*.

**Original work by [tmbinc/bgrep](https://github.com/tmbinc/bgrep)**

Feel free to modify, branch, fork, improve. Re-licenses as BSD.

### Building
First, you need to install `gnulib`.  On Debian and derivatives:
```
sudo apt-get install gnulib
```

Once you have that, it is the normal autotools build process.
```
./configure
make
make check
```
And optionally: (*note: this part is untested*)
```
sudo make install
```

### How it works

```
$ src/bgrep -h
bgrep version: 0.3
usage: src/bgrep [-hfc] [-s BYTES] [-B BYTES] [-A BYTES] [-C BYTES] <hex> [<path> [...]]

   -h         print this help
   -f         stop scanning after the first match
   -c         print a match count for each file (disables offset/context printing)
   -s BYTES   skip forward to offset before searching
   -B BYTES   print <bytes> bytes of context before the match
   -A BYTES   print <bytes> bytes of context after the match
   -C BYTES   print <bytes> bytes of context before AND after the match

      Hex examples:
         ffeedd??cc        Matches bytes 0xff, 0xee, 0xff, <any>, 0xcc
         "foo"           Matches bytes 0x66, 0x6f, 0x6f
         "foo"00"bar"   Matches "foo", a null character, then "bar"
         "foo"??"bar"   Matches "foo", then any byte, then "bar"

      BYTES may be followed by the following multiplicative suffixes:
         c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M
         GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.
```

### Examples
**Basic usage**
```
$ echo "1234foo89abfoof0123" | bgrep \"foo\"
stdin: 00000004
stdin: 0000000b
```
**Count findings option**
```
$ echo "1234foo89abfoof0123" | bgrep -c \"foo\"
stdin count: 2
```
**Find-first option**
```
$ echo "1234foo89abfoof0123" | bgrep -f \"foo\"
stdin: 00000004
```
**Overlapping matches**
```
$ echo "oofoofoofoo" | bgrep \"foof\"
stdin: 00000002
stdin: 00000005
```
**Wildcard matches**
```
$ echo "oof11f22foo" | bgrep '66????66'
stdin: 00000002
stdin: 00000005
```


