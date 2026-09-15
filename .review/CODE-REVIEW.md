# HTTrack fork — code review

Scope: `src/` (~52k lines C), HEAD `748c35de` (3.49.6), branch `master`.

Method: read-through of the network/parse/name/cookie/server paths, plus
`gcc -Wall -Wextra` and `clang --analyze` over every `.c` (the `src/coucal`
submodule is not vendored, so a permissive stub header was used for analysis
only — nothing in the repo was modified).

Findings are ordered by severity. Every item cites `file:line` and states the
observable failure, not just the smell.

---

## Status

Section 1 (confirmed bugs) is fixed, on branch `robustness-fixes`. Each item
below is annotated with the commit that closed it. Sections 2 and 3
(refactoring and features) are untouched.

Three things found while fixing, not in the original review:

* **A cookie scoped to `.x.foo.com` was also sent to `foo.com`** — the wildcard
  branch tested only `m < n`, widening a cookie's scope to its parent domain.
  Found when the fix for 1.1 made the degenerate cases testable. (3c2513d3)
* **`max(0, strlen(afs.af.fil) - 4)` in `htsparse.c`** — `strlen` is unsigned,
  so for a name under 4 characters the subtraction wraps and `max` picks the
  huge value, reading off the front of the buffer. Common URLs hit this.
  Surfaced by turning on `-Wtype-limits`. (91cf964b)
* **A heap overflow in the backlog serializer** — the temporary filename
  buffer is sized from `url_sav` but written from `path_html_utf8` when
  `getmode == 0`, so a project path longer than the save name overflows it.
  (37b2df52)

And one correction to this document: item 1.16 claimed the discarded `zErr`
values in `htscache.c` meant zlib failures were "silently ignored". That is
wrong — each is written as `if ((zErr = ...) != Z_OK) { assertf(...); }`, so
the value *is* tested and clang only means it is never read again. The real
issue there is that a transient failure (a full disk during a cache write)
aborts the entire crawl rather than degrading, which is an instance of §2.1,
not a separate bug.

| Item | Status |
|---|---|
| 1.1 cookie OOB read | fixed, 3c2513d3 (+ regression test) |
| 1.2 no TLS verification | fixed, 4fab7976 |
| 1.3 webhttrack sid / bind | fixed, d68615ee |
| 1.4 Location overflow | fixed, d68615ee |
| 1.5 uninitialised `fsfile` | fixed, d68615ee |
| 1.6 robots.txt fail-open | fixed, 1da6d01d (+ tests, 804d6156) |
| 1.7 `r.contenttype >= 0` | fixed, 3c2513d3 |
| 1.8 collision suffix | fixed, a40aa076 (bounded; hashing unchanged) |
| 1.9 `retour` null deref | fixed, 37b2df52 |
| 1.10 `strstr` on NULL body | fixed, 37b2df52 |
| 1.11 unchecked malloc | fixed, 37b2df52 |
| 1.12 `strcpybuff` degradation | 43 of 199 sites fixed; rest ratcheted in CI, a40aa076 |
| 1.13 memory leaks | charset leaks fixed, 37b2df52; startup one-shots left |
| 1.14 non-reentrant libc | fixed, b6c5fae6 |
| 1.15 macro hygiene | fixed, 91cf964b |
| 1.16 dead stores | assessed — see correction above |

CI now runs gcc and clang builds behind a `-Werror` list, an ASan/UBSan build,
the clang static analyzer, and the unchecked-buffer ratchet.

---

## 1. Confirmed bugs

### 1.1 `cookie_cmp_wildcard_domain` — infinite loop / out-of-bounds read — **critical**

`src/htsbauth.c:132-149`

```c
const size_t l = n < m ? n : m;
size_t i;
for (i = l - 1; i >= 0; i--) {
  if (chk_dom[n - i - 1] != domain[m - i - 1]) {
    return 1;
  }
}
```

`i` is `size_t`. `i >= 0` is **always true**, so the loop has no exit condition
other than a byte mismatch. Two consequences:

* If either domain is empty, `l == 0` and `l - 1` is `SIZE_MAX`. The first
  indexing operation is `chk_dom[n - SIZE_MAX - 1]`, i.e. a wild read.
* Otherwise the loop walks backwards off the front of both buffers until it
  happens to find a mismatch or faults.

This is the cookie domain matcher, reached from `cookie_find`
(`src/htsbauth.c:173`) on every `Set-Cookie` and every outgoing request. The
inputs are attacker-controlled (a remote `Set-Cookie: ...; domain=...`), so a
hostile or merely malformed site can crash the crawler, and a "lucky" early
mismatch can silently return the wrong match — sending one site's cookie to
another. GCC flags this with `-Wtype-limits`; it is invisible under the default
`CFLAGS`.

Fix — count up, not down, and the `l == 0` case falls out for free:

```c
for (i = 0; i < l; i++) {
  if (chk_dom[n - i - 1] != domain[m - i - 1]) {
    return 1;
  }
}
```

### 1.2 No TLS certificate verification whatsoever — **critical**

`src/htslib.c:5193`

```c
openssl_ctx = SSL_CTX_new(SSLv23_client_method());
```

That is the entire TLS setup. There is no `SSL_CTX_set_default_verify_paths`,
no `SSL_CTX_set_verify`, no `SSL_get_verify_result` check, and no hostname
verification (`X509_VERIFY_PARAM_set1_host`). SNI is set
(`src/htsback.c:2541`), so the handshake completes against any certificate at
all. Every `https://` mirror is trivially machine-in-the-middle-able, and the
crawler will happily write the attacker's content to disk as if it were the
real site.

Secondary issues in the same block: `SSLv23_client_method()` is deprecated in
favour of `TLS_client_method()`, and no minimum protocol version is set, so the
floor is whatever the linked OpenSSL defaults to. The heartbleed version-string
check above it (`ssl_vulnerable`) is a 2014 artefact that is now dead weight.

Minimum fix:

```c
openssl_ctx = SSL_CTX_new(TLS_client_method());
SSL_CTX_set_min_proto_version(openssl_ctx, TLS1_2_VERSION);
SSL_CTX_set_default_verify_paths(openssl_ctx);
SSL_CTX_set_verify(openssl_ctx, SSL_VERIFY_PEER, NULL);
```
plus `SSL_set1_host(ssl, hostname)` alongside the existing
`SSL_set_tlsext_host_name` call, and a `--insecure` / `-%!` style option for
users who genuinely need the old behaviour (self-signed intranet mirrors are a
real use case — make it opt-in and loud).

### 1.3 webhttrack control server: predictable session id + binds to all interfaces — **critical**

`src/htsweb.c:250-254`

```c
srand((unsigned int) time(NULL));
snprintf(buff, sizeof(buff), "%d-%d", (int) time(NULL), (int) rand());
domd5mem(buff, strlen(buff), digest, 1);
smallserver_setkey("sid", digest);
```

The session token is `MD5(time(NULL) + "-" + rand())` where `rand()` was seeded
from `time(NULL)` in the same second. The entire keyspace is "which second did
the process start", so the token is recoverable by anyone who can guess the
start time to within a minute or two — a few thousand guesses.

`src/htsserver.c:264` then binds with `SOCaddr_initany`, which is
`sin_addr = 0` (`src/htsnet.h:170-176`) — i.e. `0.0.0.0`, every interface.

And the check itself (`src/htsserver.c:532-541`) only runs when *both* `sid`
and `_sid` are present:

```c
if (coucal_readptr(NewLangList, "sid", &adr)) {
  if (coucal_readptr(NewLangList, "_sid", &adr2)) {
    if (strcmp((char *) adr, (char *) adr2) != 0) { meth = 0; }
  }
}
```

A plain `GET` carries no POST body, so no `_sid` is parsed from the request and
the stored one is simply re-compared with itself. GET navigation is therefore
unauthenticated outright.

What this server can do: start a mirror with an arbitrary `path` and arbitrary
URL filters. That is remote, largely unauthenticated, arbitrary file write.

Fix: bind to `127.0.0.1`/`::1` by default (with an explicit `--bind` to opt
out), generate the token from a CSPRNG (`RAND_bytes`, `getrandom(2)`,
`/dev/urandom`), and make the token check fail-closed — reject any request that
does not present a matching token, GET included.

### 1.4 Stack buffer overflow in the redirect header — **high**

`src/htsserver.c:897-906`

```c
const char *newfile = file;
if (coucal_readptr(NewLangList, "redirect", &adr) && adr != 0) {
  ...
  newfile = newadr;
}
...
char tmp[256];
if (strlen(file) < sizeof(tmp) - 32) {
  sprintf(tmp, "Location: %s\r\n", newfile);   /* <-- guards `file`, formats `newfile` */
}
```

The bounds check is on `file`; the string actually written is `newfile`. They
are different whenever a `redirect` entry exists.

And `redirect` is reachable: every POST form field name becomes a key in
`NewLangList` verbatim (`src/htsserver.c:517-525`,
`coucal_write(NewLangList, s, ...)` where `s` is the raw field name). A POST
body of `redirect=<300 bytes>` overflows `tmp[256]`. Combined with 1.3, the
attacker does not need to be local.

Fix: check `strlen(newfile)`, and use `snprintf` — or better, refuse to let
client-supplied keys collide with engine-internal ones (namespace the
form-derived keys, or whitelist them).

### 1.5 Uninitialized read on `fsfile` — **medium**

`src/htsserver.c:804-849`

`char fsfile[1024];` is declared uninitialized. `fsfile[0] = '\0'` happens
**only** inside the `if (error_redirect == NULL)` branch. Two paths reach
`if (fsfile[0] && ...)` with the buffer never written:

* `error_redirect != NULL` (the `HTS_ERROR`-style path, `src/htsserver.c:310`);
* the `/website/` branch where `coucal_readptr(..., "projpath", ...)` returns
  false, so neither `sprintf` runs.

Then `fopen(fsfile, "rb")` is called on stack garbage. Move the
`fsfile[0] = '\0';` up to the declaration.

While you are there: the `..` traversal guard at `src/htsserver.c:853` is
`strstr(file, "..") == NULL`, applied *after* the path has already been
composed, and applied to the raw (still percent-encoded in some paths) string.
Normalize first, then compare against a canonical root prefix.

### 1.6 robots.txt silently fails open on large rule sets — **high (behavioural)**

`src/htscore.c:1789` collects rules into `char buff[8192]` and loops
`while (... && strlen(buff) < sizeof(buff) - 32)`, so up to ~8160 bytes.

`src/htsrobots.c:77-79` then rejects the whole thing:

```c
if (((int) strlen(data)) >= sizeof(robots->token) - 2)
  return 0;
```

where `robots->token` is `char[4096]` (`src/htsrobots.h:44`). The return value
is discarded at the call site (`src/htscore.c:1871`). So for any site whose
`Disallow:` rules total more than ~4 KB — which is extremely common on large
sites — **every rule is dropped and the crawler proceeds as if robots.txt did
not exist**, while still logging "forbidden links for %s are: ..." as though it
had applied them.

That is both a correctness bug and a real-world liability: the operator
believes they are respecting robots.txt and they are not. Fix by making the
token storage dynamic (it is a `char[4096]` inside a linked-list node — a
`char *` costs nothing here), and by checking the return value and logging
loudly if rules cannot be stored.

### 1.7 `r.contenttype >= 0` is always true — **medium**

`src/htscoremain.c:2132`

```c
if (r.contenttype >= 0) {
  fprintf(stdout, "Content-Type: %s\r\n", r.contenttype);
}
```

`contenttype` is `char[64]` (`src/htsopt.h:502`), so this compares an array
decay against `0` — always true. Every neighbouring field in the same block is
tested correctly as `r.etag[0]`, `r.cdispo[0]`, `r.contentencoding[0]`, which
makes the intent unambiguous. Result: the stdout/CGI output mode always emits
`Content-Type: ` with an empty value. Should be `r.contenttype[0]`. Caught by
`-Wextra` (`ordered comparison of pointer with integer zero`).

### 1.8 Collision suffix derived from an additive checksum — **medium**

`src/htsname.c:1165-1183`

```c
for(i = 0; fil_complete[i] != '\0'; i++) s += (unsigned int) fil_complete[i];
for(i = 0; adr_complete[i] != '\0'; i++) s += (unsigned int) adr_complete[i];
srand(s);
for(i = 0; i < 8; i++) afs->save[i + j] = C[(rand() % L)];
```

This generates the 8-character disambiguating suffix used when local filenames
would otherwise collide. The seed is a plain byte sum, so any permutation of
the URL produces the same seed and therefore the *same* suffix —
`/a/bc.html` and `/a/cb.html` collide, as does any pair differing by a
character swap. The mechanism that exists to prevent filename collisions
generates collisions. Files get silently overwritten.

Separately, `srand()` clobbers the process-global PRNG (also used for the
webhttrack session id, see 1.3) and `rand()` is not thread-safe — this runs on
crawler worker threads.

Fix: use the MD5 already linked into the binary (or `murmurhash3.h`, which is
sitting in `src/` unused as `.orig`) and base-36 encode the digest. No global
state, no collisions from anagrams.

### 1.9 `retour` null-checked, then dereferenced anyway — **medium**

`src/htslib.c:705-721`

```c
if (soc == INVALID_SOCKET) {
  if (retour) { ... }          /* <-- explicitly guarded */
}
if (soc == LOCAL_SOCKET_ID) {
  retour->is_file = 1;         /* <-- unguarded */
```

Flagged by clang (`core.NullDereference`). Same pattern at
`src/htslib.c:2148` (`retour->ssl`). Either `retour` can be NULL and both sites
need the guard, or it cannot and the guard above is misleading — pick one. Given
this is a public-ish entry point, add the guard.

### 1.10 `strstr` on a possibly-NULL body — **medium**

`src/htsparse.c:479` — `strstr(html, "http://purl.org/rss/")` in the RSS
sniffing branch, where `html` derives from `r->adr`, which the surrounding code
elsewhere tests for NULL (e.g. `src/htsparse.c:474`'s sibling branch at line
450, and `src/htscore.c:1750`'s explicit `if (!r.adr)`). clang reports both a
`nonnull` violation at 479 and a dereference at 498. A zero-length `text/xml`
response reaches it.

### 1.11 Unchecked `malloc` followed by `memcpy`

`src/htscore.c:1771-1780`

```c
char *adr = (char *) malloct(HTS_DATA_UNKNOWN_GIF_LEN);
hts_log_print(...);
if (r.adr) { freet(r.adr); r.adr = NULL; }
memcpy(adr, HTS_DATA_UNKNOWN_GIF, HTS_DATA_UNKNOWN_GIF_LEN);
```

The `create_html_warning` branch immediately above it *does* check
(`if (adr)`), the GIF branch does not. There are ~70 `malloct`/`calloct` call
sites in `src/*.c`; a sweep for unchecked ones is worth one pass.

### 1.12 The "safe" string layer silently disarms itself

`src/htssafe.h:127-160`

```c
#define strcpybuff(A, B) \
  ( HTS_IS_NOT_CHAR_BUFFER(A) \
  ? strcpy(A, B) \
  : strcpy_safe_(A, sizeof(A), B, ...) )
```

When the destination is a `char *` rather than a `char[]` — which is the case
for every buffer passed into a function as a parameter — `strcpybuff` degrades
to a bare `strcpy` with **no bound at all**. The codebase has 1000+
`strcpybuff`/`strcatbuff` call sites and reads as if it were memory-safe; a
large fraction of them are not. This is the single most misleading thing in the
tree.

Worse on non-GCC compilers: the fallback discriminator is
`sizeof(VAR) != sizeof(char*)`, so under MSVC on 64-bit a `char[8]` buffer is
classified as a pointer and gets the unchecked path. The header comments on
this ("a bit lame") but ships it.

Two things to do, in order:
1. Add `strlcpybuff`/`strlcatbuff` (already present, they take an explicit
   size) at every site where the destination is a parameter. A mechanical audit
   can find these: any `strcpybuff(x, ...)` where `x` is not a local array.
2. Consider making the pointer case a *compile error* rather than a silent
   fallback, so the audit is enforced going forward.

Also note the failure mode of the safe path is `abort()` (`abortf_`,
`src/htssafe.h:93-99`). That is defensible for the CLI and indefensible for
`libhttrack` consumers — see §2.1.

### 1.13 Memory leaks (clang `unix.Malloc`)

| Location | Leaked |
|---|---|
| `src/htsback.c:314` | `filename` |
| `src/htscoremain.c:302` | `x_argv` |
| `src/htscoremain.c:329` | `url` |
| `src/htscoremain.c:649` | `argv` |
| `src/htstools.c:973` | `category` |
| `src/htscharset.c:506` | `outbuf` |
| `src/htscharset.c:1239` | `dest` |

The `htscharset.c` two are on conversion error paths and will leak per bad
input — i.e. an unbounded leak driven by remote content. The `htscoremain.c`
ones are one-shot at startup and matter mainly for valgrind cleanliness.

### 1.14 Non-reentrant libc in a threaded crawler

`src/*.c` uses `localtime()` ×7, `gmtime()` ×2, `strerror()` ×20 and
`gethostbyname()` ×7, all from worker threads. These return pointers to shared
static storage; concurrent calls race and produce corrupted timestamps, wrong
error messages, and — for `gethostbyname` — corrupted address structures.

`getaddrinfo` already exists in the tree (`src/htslib.c:4640`,
`src/htsserver.c:196`), so the `gethostbyname` sites are pure legacy. Replace
with `localtime_r`/`gmtime_r`/`strerror_r`/`getaddrinfo` (with `_s` variants
behind the existing `_WIN32` guards).

### 1.15 Macro hygiene (latent)

* `src/htslib.c:3170` — `#define CHAR_BETWEEN(c, a, b) ( (c) >= 0x##a ) && ( (c) <= 0x##b )` has **no outer parentheses**. It happens to parse correctly at its current call sites because `=` binds looser than `&&`, but any use in a context binding tighter than `&&` silently drops the second half. Wrap it.
* `src/htsbase.h:83-89` — `min`/`max` are defined twice, identically, three lines apart. `Sleep(a)` evaluates `a` four times.
* `src/htscore.c:411-421` — `HT_INDEX_END`: the `else` guards only `tempo[0]='\0';` while the indentation suggests it guards the `hts_template_format` call too. The behaviour is correct; the formatting is a trap for the next reader. (`-Wmisleading-indentation`.)
* `src/htsparse.c:4586` — raw `strncat(in_error_msg, back[b].r.msg, sizeof(in_error_msg) - 1)`. The third argument to `strncat` is *characters to append*, not buffer size. It is safe here only because `in_error_msg[0] = 0` runs on the line before. Use the project's own `strlcatbuff`.

### 1.16 Dead stores

~25 `deadcode.DeadStores` from clang, concentrated in `src/htscache.c`
(`zErr` assigned and never read at lines 341, 357, 380, 398, 405 — these are
**zlib error codes being discarded**, which is worth a real look, not just a
cleanup), `src/htscoremain.c:2382-2436` (`na`), and `src/htsback.c:1802`
(`send_too`). The `zErr` ones are the interesting subset: cache
compression/decompression failures may be silently ignored.

---

## 2. Refactoring priorities

### 2.1 `abort()` is not an error-handling strategy for a library

`libhttrack` is a shipped shared library with a documented callback API
(`src/httrack-library.h`, `libtest/`). Any buffer-length surprise anywhere in
the engine calls `abort()` and takes the host process with it. Embedders cannot
defend against this.

Introduce a failure mode that unwinds: have `strncat_safe_` return NULL /
set an error on the `httrackp *` and let callers propagate, with `abort()`
retained only under an explicit `HTTRACK_FATAL_ASSERT` build flag. This is
invasive but it is the difference between "a library" and "a program with a
`.so` extension".

### 2.2 Fixed-size buffers → the `String` type that already exists

`src/htsstrings.h` provides a growable `String` with `StringCat`,
`StringMemcat`, `StringBuff`, etc., and `htsserver.c` uses it. The rest of the
engine does not: `HTS_URLMAXSIZE`-sized stack arrays and `BIGSTK char buf[1024]`
are everywhere, and a large share of the bugs above (1.4, 1.5, 1.6) are
downstream of that choice.

This is the highest-leverage refactor in the tree, and it is incremental:
convert one call chain at a time, starting with the URL path
(`htsname.c` → `htslib.c` URL building), which is where truncation causes
*silent wrong behaviour* rather than a clean abort.

### 2.3 Break up `htsparse.c`

4762 lines, dominated by a single function with ~10 levels of nesting, `goto`s
into shared cleanup (`XH_uninit`), and French/English mixed comments. It is the
component most exposed to hostile input and the one nobody can safely modify.

A tractable decomposition:
* tag/attribute tokenizer (pure, testable, fuzzable)
* URL extraction per element+attribute (table-driven — `hts_detect[]` at
  `src/htslib.c:114` is already halfway there)
* rewrite/emit
* the link-queueing side effects

Even extracting just the tokenizer behind a clean interface would let you fuzz
it, which currently is not possible.

### 2.4 Build and CI

* No CI at all (`.github/` absent, no `.travis.yml`). For a fork whose stated
  goal is robustness, this is the first thing to add.
* Default `CFLAGS` carry no `-Wall`. The bugs in 1.1 and 1.7 are both plain
  `-Wextra` diagnostics that have been sitting in the tree.
* `configure.ac` sets no `-std=` and no `AC_USE_SYSTEM_EXTENSIONS`.
* `src/coucal` is a git submodule pointing at a third-party GitHub repo. The
  tree does not build at all without network access (I hit exactly this).
  Vendor it, or add a `--with-system-coucal` path.

Concretely, a first CI job:

```
- gcc and clang builds with -Wall -Wextra -Werror on the fixed subset
- -fsanitize=address,undefined build running tests/
- clang --analyze over src/*.c
- a libFuzzer target for the HTML tokenizer and one for fil_simplifie()
```

### 2.5 Tests

13 shell scripts in `tests/`, most gated on `ONLINE_UNIT_TESTS` — i.e. they
need the live internet and a live httrack.com. For 52k lines that is close to
zero effective coverage, and none of it runs in a sandbox.

The engine-level tests (`01_engine-charset`, `01_engine-idna`,
`01_engine-simplify`, `01_engine-hashtable`) are the good pattern — they drive
`httrack --testbench`-style entry points (`src/htscoremain.c:2329`). Extend
that: a local static HTTP server fixture (Python's `http.server` is enough)
would make the entire `1x_crawl-*` suite hermetic and let you add regression
tests for every bug in §1.

---

## 3. Feature suggestions

### 3.1 Correctness gaps in what it already claims to do

**robots.txt is pre-2000.** `src/htscore.c:1818-1866` handles only
`User-agent:` and `Disallow:`. Missing, all now mandatory under RFC 9309:
* `Allow:` — completely absent. A site using the standard
  `Disallow: /` + `Allow: /public/` pattern is mirrored as fully blocked.
* `*` wildcards and `$` end-anchors in paths.
* `Crawl-delay:` — arguably the single most useful directive for a mirroring
  tool, and it would let you be a better citizen by default.
* `Sitemap:` — free, high-quality URL discovery, ignored.
* Group semantics: consecutive `User-agent:` lines should form one group; the
  current `record` flag handles the common cases but not that one.

Note also `IGNORE_RESTRICTIVE_ROBOTS` (`src/htscore.c:1845`) silently discards
`Disallow: /` unless `opt->robots >= 3`. That is a deliberate choice, but it
means the default build ignores the single most explicit "do not crawl me"
signal on the web. Worth revisiting for a fork that will be used by other
people.

**Responsive images are not mirrored.** `src/htslib.c:114-130` lists
`data-srcset` in `hts_detect[]` but **not** `srcset`, the actual HTML5
attribute. And both are treated as single-URL attributes, whereas `srcset` is a
comma-separated descriptor list (`a.jpg 1x, b.jpg 2x`) — so even the
`data-srcset` entry extracts a malformed URL. `<picture><source srcset=...>`
therefore does not mirror at all. This is probably the most visible everyday
breakage on modern sites.

**Other modern-web gaps**, in rough order of impact:
* `@import "x";` (the string form) in CSS — only `url(...)` is handled
  (`src/htsparse.c:1348`).
* `<link rel=preload as=...>`, `rel=manifest`, and web app manifest JSON.
* `integrity=` / SRI attributes are not stripped on rewrite, so mirrored-and-
  rewritten subresources fail their hash check and the browser refuses to load
  them. Silent, confusing breakage.
* Content encodings: gzip only (`src/htslib.c:1119`). Brotli (`br`) is now the
  default on a large share of the web; `zstd` is arriving.
* HTTP/2 and HTTP/3. HTTP/1.1 with keep-alive is implemented
  (`src/htslib.c:988`); ALPN would be the first step.

### 3.2 New capability worth adding

* **WARC output.** The natural modern output format for a crawler, and it
  would make the tool interoperable with the whole web-archiving ecosystem.
  A large share of people who would use this fork want WARC.
* **A JS-rendering hook.** Not a JS engine in-tree — an optional
  `--render-command` that shells out to a headless browser for pages matching a
  filter, feeding the rendered DOM back into the existing parser. Cheap to
  build, and it is the difference between mirroring 2005's web and today's.
* **Per-host adaptive rate limiting** with `Crawl-delay` and `Retry-After`
  support, plus exponential backoff on 429/503. Current throttling is a global
  `maxrate`.
* **Structured logging** (JSON lines) and a `--stats` endpoint. The current
  `hts_log_print` free-text log is hard to machine-consume, and for long
  crawls that matters.
* **SOCKS5 proxy support** and PAC files — currently HTTP proxy only.
* **Resumable crawl state as a real database.** The `hts-cache/` format
  (`src/htscache.c`, 2214 lines of hand-rolled binary + zlib) is a recurring
  source of complexity and of the discarded `zErr` codes in 1.16. SQLite would
  eliminate that file wholesale, give you crash-safe resume, and make the cache
  inspectable with off-the-shelf tools.

---

## 4. Suggested order of work

1. **1.1** (cookie loop) and **1.7** (`contenttype`) — one-line fixes, both are
   live defects. Land with regression tests.
2. **CI with `-Wall -Wextra -Werror` + ASan/UBSan** (§2.4). Do this before the
   rest so nothing regresses, and so the next bug of this class is caught by a
   machine.
3. **1.2** (TLS verification). Self-contained, and it is the finding with the
   worst consequence-to-effort ratio.
4. **1.3, 1.4, 1.5** (webhttrack). Consider whether `htsserver.c` should ship
   enabled by default at all.
5. **1.6** (robots.txt fail-open), then the robots.txt rewrite in §3.1 — the
   two are the same code and worth doing in one pass.
6. **1.12** (the `strcpybuff` audit). Mechanical, tedious, high value.
7. Everything else.

---

*Analysis artefacts (build log, compiler and analyzer output) are in the
session scratchpad; nothing outside `.review/` was modified.*
