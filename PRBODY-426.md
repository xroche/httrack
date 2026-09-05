`HTS_INET6` picks which arm of `SOCaddr` the installed `htsnet.h` declares. Until now the only thing setting it was an `AC_CHECK_LIB(c, getaddrinfo)` with no flag behind it. When the probe failed it printed a warning and carried on, leaving the macro undefined for `htsglobal.h` to default. Two builds of one version could therefore ship different struct layouts under the same soname, and the installed `htsfeatures.h` recorded neither.

`--enable-ipv6=yes/no/auto` follows the shape #1545 gave `--enable-https`. The default is still `auto`, which probes and then defines `HTS_INET6` to 0 or 1 rather than leaving it undefined. `yes` on a host with no `getaddrinfo` is a hard error naming `--disable-ipv6`, and `no` skips the probe. The configure summary now names the value reached and who decided it, and `V6_SUPPORT` keeps carrying the outcome rather than the request.

`--disable-ipv6` did not build. Three defects are fixed here because the flag is worthless otherwise, and all three were already reachable on a host without `getaddrinfo`. `htsdns_selftest.c` gained `dns_timeout_selftests` without the `HTS_INET6=0` stub its neighbour has, so the library did not link. `-@i` left its digits in the option cluster, so an IPv4-only httrack read the `2` of `-@i2` as another option and exited 255. That is the #615 class `01_engine-cmdline.test` already guards. And `htsweb.c` tested `HTS_INET6` with `#ifdef`, which a defined 0 satisfies.

Two tests presumed the IPv6 build. `381`'s negative control wrote 0 over a build that already had 0, so it now flips to whichever value is wrong. `341` gated its `::1` bind on the host alone.

No installed struct moves, so `VERSION_INFO` does not move either. `SOCaddr` for a given setting is byte-identical to master's, checked by compiling `pubheaderlayout.c` against both trees' headers at `HTS_INET6=0` and at `=1`. The two settings do differ from each other, 16 bytes against 28, so that probe is not vacuous. On a machine with `getaddrinfo` the generated `config.h`, `htsfeatures.h` and every `V6_*` substitution come out identical to master's.

Windows is untouched, because it never reads `config.h`. `htsglobal.h`'s `_WIN32` arm has defaulted `HTS_INET6` to 1 for as long as the file has existed, so that build has always been IPv6.

IPv6 is deliberately not wired to `--disable-auto-features`. `getaddrinfo` is a libc function rather than an optional dependency, and defaulting it off would move the layout under every recipe passing that flag.

`426_configure-ipv6-flag.test` runs configure eight times. It asserts the macro, the summary qualifier and `V6_SUPPORT`/`V6_FLAG` agree in each state, and it fails on master.
