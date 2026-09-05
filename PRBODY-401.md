A 5GB file enumerated through `hts_findgetsize()` measures 1GB. The function returns `off_t` truncated into an `int` on POSIX, and on Windows it returns `nFileSizeLow` alone, dropping the high dword. A caller sizing a buffer from that plausible-looking number takes a heap overflow. Nothing in the tree calls it any more. It is `HTSEXT_API` and declared in the installed `httrack-library.h`, so an out-of-tree consumer can be doing exactly that today.

`hts_findgetsize64()` returns `LLint`, the width `fsize()` and `fpsize()` already use for file sizes, and combines both dwords on Windows.

`hts_findgetsize()` stays exported with the same signature and is now `HTS_DEPRECATED`. The decision worth flagging is what it returns past `INT_MAX`: -1, the sentinel it already uses for "no entry", rather than the low bits. A caller that ignores -1 gets a `malloc` failure, where one handed a truncated size gets a silent short buffer. Every caller must already cope with -1.

`VERSION_INFO` moves 3:20:0 to 3:21:0. An export arrived, none moved and none went away, so the soname stays `.so.3` and no Debian package is renamed.

The test is `tests/421_engine-findsize.test`. It drives a new `-#test=findsize` over a 5GB sparse file plus a 1234-byte sibling. It skips on EFBIG or ENOSPC, where the host cannot hold the probe. Both halves of the fix are pinned. Reverting the 64-bit form makes it report 1073741824, and reverting the deprecated form's clamp makes that one report it instead.
