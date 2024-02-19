3.49-2
- Fixed: Buffer overflow in output option commandline argument (VL-ID 2068) (Hosein Askari)
- Fixed: Minor fixes

3.48-23
- Fixed: on Linux, FTBFS with openssl 1.1.0

3.48-22:
- Fixed: on Windows, fixed possible DLL local injection (Tunisian Cyber)
- Fixed: various typos

3.48-21:
- Fixed: Google RPMs use /usr/bin/google-chrome as program location (Cickumqt)
- Fixed: Fixed htsserver not dying (immediately) on quit.
- New: Updated WIN32 OpenSSL to 1.0.1j (Evgeniy)

3.48-20:
- Fixed: webhttrack incompatibility with Chrome
3.48-19:
- Fixed: assertion failure at htslib.c:3458 (strlen(copyBuff) == qLen) seen on Linux

3.48-18:
- Fixed: infamous crashes inside the DNS cache due to a corruption within the option structure (E.Kalinowski/karbofos)
- New: added minimalistic crash reporting on Windows and Linux

3.48-17:
- Fixed: URL list not working anymore (tom swift)
- Fixed: FTBFS on ARM

3.48-14:
- Fixed: buggy FFFD (replacement character) in place of leading non-ascii character such as Chinese ones (aballboy)
- Fixed: FTBFS when compiling with zlib versions < 1.2.70 (sammyx)
- Fixed: buggy SVG (Smiling Spectre)
- Fixed: do not uncompress .tgz advertised as "streamed" (Smiling Spectre)
- Fixed: NULL pointer dereferencing in back_unserialize (htsback.c:976)

3.48-13
- Fixed: library development files

3.48-12
- Fixed: --advanced-maxlinks broken (Localhost)
- Fixed: -devel package should now be standalone
3.48-11
- Fixed: assertion failure at htscore.c:244 (len - liensbuf->string_buffer_size < liensbuf->string_buffer_capa)

3.48-10
- Fixed: injection-proof templates
- Fixed: htshash.c:330 assertion failure ("error invalidating hash entry") (Sergey)
- Fixed: Windows 2000 regression (fantozzi.usenet)
- Fixed: code cleanup (aliasing issues, const correctness, safe strings)
- New: handle --advanced-maxlinks=0 to disable maximum link limits
- New: updated ZIP routines (zlib 1.2.8)

3.48-9
- Fixed: broken 32-bit version
- Fixed: assertion "segOutputSize < segSize assertion fails at htscharset.c:993"

3.48-8
- Fixed: new zlib version fixing CVE-2004-0797 and CVE-2005-2096
- Fixed: more reliable crash reporting

3.48-7
- Fixed: fixed infamous "hashtable internal error: cuckoo/stash collision" errors

3.48-6
- Fixed: safety cleanup in many strings operations

3.48-3
- Fixed: buggy option pannels

3.48-2
- New: Enforce check against CVE-2014-0160

3.48-1
- New: improved hashtables to speedup large mirrors
- New: added unit tests
- New: Added %a option, allowing to define the "Accept:" header line.
- New: Added %X option, to define additional request header lines.
- New: Added option '-%t', preserving the original file type (which may produce non-browseable file locally)
- Fixed: remove scope id (% character) in dotted address resolution (especially for catchurl proxy)
- Fixed: build fixes, including for Android, non-SSL releases
- Fixed: buggy keep-alive handling, leading to waste connections
- Fixed: removed chroot and setuid features (this is definitely not our business)
- Fixed: removed MMS (Microsoft Media Server) ripping code (mmsrip) (dead protocol, unmaintained code, licensing issues)
- Fixed: type mishandling when processing a redirect (such as a .PDF redirecting to another .PDF, with a text/html type tagged in the redirect message)
- Fixed: infinite loop when attempting to download a file:/// directory on Unix (gp)
- Fixed: removed background DNS resolution, prone to bugs
- Fixed: do not choke on Windows 2000 because of missing SetDllDirectory() (Andy Hewitt)
- Fixed: %h custom build structure parameter not taken in account (William Clark)

3.47-27
- Fixed: infinite loop when attempting to download a file:/// directory on Unix (gp)

3.47-27
- Fixed: fixed logging not displaying robots.txt rules limits by default
- Fixed: license year and GPL link, libtool fixes (cicku)
- Fixed: Keywords field in desktop files (Sebastian Pipping)

3.47-26
- Fixed: buggy DNS cache leading to random crashes

3.47-25
- Fixed: content-disposition NOT taken in account (Stephan Matthiesen)

3.47.24
- Fixed: url-escaping regression introduced in the previous subrelease

3.47-23
- Fixed: unable to download an URL whose filename embeds special characters such as # (lugusto)
- New: Croatian translation by Dominko Aždajić (domazd at mail dot ru)

3.47-22
- Fixed: Russian translation fixes by Oleg Komarov (komoleg at mail dot ru)
- New: Added .torrent => application/x-bittorrent built-in MIME type (alexei dot co at gmail dot com)

3.47-21
- Fixed: Mishandling of '-' in URLs introduced in 3.47-15 (sarclaudio)
- Fixed: "Wildcard domains in cookies do not match" (alexei dot co at gmail dot com )
- Fixed: buggy referer while parsing: the referer of all links in the page is the current page being parsed, NOT the parent page. (alexei dot com at gmail dot com)

3.47-20
- Fixed: Fixed div-by-zero when specifying more than 1000 connections per seconds (probably not very common)

3.47-19
- Fixed: more windows-specific fixes regarding 260-character path limit
- Fixed: escaping issue in top index
- Fixed: Linux build cleanup (gentoo patches merge, lintian fixes et al.)

3.47-18
- Fixed: --timeout alias did not work

3.47-17
- Fixed: URL-encoding issue within URI

3.47-15
- Fixed: non-ascii characters encoding issue inside query string (lugusto)
- Fixed: HTML entities not properly decoded inside URI and query string

3.47.14
- Fixed: 260-characters path limit for Windows (lugusto)
- New: openssl is no longer dynamically probed at stratup, but dynamically linked

3.47-13
- Fixed: bogus charset for requests when filenames have non-ascii characters (Steven Hsiao)
- Fixed: bogus charset on disk when filenames have non-ascii characters (Steven Hsiao)
- New: support for IDNA / RFC 3492 (punycode) handling

3.47-12
- Fixed: images in CSS were sometimes not correctly detected (Martin)
- Fixed: links within javascript events were sometimes not correctly detected (wquatan)
- Fixed: webhttrack caused bus error on certain systems, such as Mac OSX, due to the stack size (Patrick Gundlach)

3.47-11
- Fixed: zero-length files not being properly handled (not saved on disk, not updated) (lugusto)
- Fixed: serious bug that may lead to download several times the same file, and "Unexpected 412/416 error" errors

3.47-7
- Fixed: on Windows, fixed possible DLL local injection (CVE-2010-5252)
- Fixed: UTF-8 conversion bug on Linux that may lead to buggy filenames

3.47-6
- Fixed: memory leak in hashtable, that may lead to excessive memory consumption

3.47-5
- Fixed: random closing of files/sockets, leading to "zip_zipWriteInFileInZip_failed" assertion, "bogus state" messages, or random garbage in downloaded files
- Fixed: libssl.dylib is now in the search list for libssl on OSX (Nils Breunese)
- Fixed: bogus charset because the meta http-equiv tag is placed too far in the html page
- Fixed: incorrect \\machine\dir structure build on Windows (TomZ)
- Fixed: do not force a file to have an extension unless it has a known type (such as html), or a possibly known type (if delayed checks are disabled)
- Fixed: HTML 5 addition regarding "poster" attribute for the "video" tag (Jason Ronallo)
- Fixed: memory leaks in proxytrack.c (Eric Searcy)
- Fixed: correctly set the Z flag in hts-cache/new.txt file (Peter)
- Fixed: parallel patch, typo regarding ICONV_LIBS (Sebastian Pipping)

3.46-1
- New: Unicode filenames handling
- Fixed: fixed bug in handling of update/continue with erased files or renamed files, leading to "Unexpected 412/416 error (Requested Range Not Satisfiable)" and/or "Previous cache file not found" (-1)" errors
- Fixed: escape characters >= 128 when sending GET/HEAD requests to avoid server errors
- Fixed: do not use "delayed" extensions when the mirror is aborting
- Fixed: generate error pages when needed (Brent Palmer)
- Fixed: parsing issue with js files due to "script" tags (Vasiliy)
- Fixed: anonymous FTP without password (Luiz)
- Fixed: Makefile issues regarding parrallel build and examples (Sebastian Pipping)
- Fixed: removed deprecated and annoying "Accept-Charset" header in requests (Piotr Engelking) (closes:#674053)

3.45-4
- New: source license is now GPLv3
- New: added a "K5" feature to handle transparent proxies (Brent Palmer)
- New: option -y to control ^Z behavior (Julian H. Stacey)
- Fixed: replace // by / when saving rather than _/ (Brent Palmer)
- Fixed: do not interpret ^C before mirror is finished, or after
- Fixed: webhttrack: do not use md5sum to produce a temporary filename, but mktemp (Ryan Schmidt)
- Fixed: document %k for custom structure (full query string)

3.45-3
- Fixed: spurious "Previous file not found (erased by user ?)" messages leading to retransfer existing files in cache (Alain Desilets)
- Fixed: --max-time now stops the mirror smoothly (Alain Desilets)

3.45-2
- Fixed: number of simultaneous connections was often only one (Illyria, William Roeder)
- Fixed: "Unexpected 412/416 error" leading to have broken files on disk

3.45-1
- Fixed: interrupting an update/continue mirror session should not delete anymore previously downloaded content (William Roeder, Alain Desilets and many others)
- Fixed: --continue/--update bug leading to download again already cached data in some cases (especially redirect/error pages)

3.44-5
- Fixed: crash when using -d with non-fully-qualified hostname (Alain Desilets)
- Fixed: typo in logs (Pascal Boulerie)

3.44-4
- Fixed: random crash when interrupting the mirror (spotted by -fstack-protector) in htscoremain.c (closes:#657878)

3.44-3
- Fixed: Linux build (closes:#657334)

3.44-2
- Fixed: malformed format htslib.c (Moritz Muehlenhoff)
- Fixed: default footer print format
- New: clever "^C" handling
- New: added --do-not-generate-errors option
- New: increased maximum cookie name

3.44-1
- Fixed: Randomly corrupted files during updates due to "engine: warning: entry cleaned up, but no trace on heap"/"Unexpected 412/416 error" errors (Petr Gajdusek ; closes:#614966)

3.43-12
- Fixed: buffer overflow while repairing httrack cache if a damaged cache is found from a previous mirror (Linus Hoppe ; closes:#607704)

3.43-11
- Fixed: webhttrack fixes for icecat (Uriy Zenkov ; closes:#605140)

3.43-10
- Fixed: capture URL not working properly when IPv6 is installed (John Bostelman)

3.43-9
- Fixed: application/xhtml-xml not seen as "html" (Peter Fritzsche)
- Fixed: various linux fixes for desktop files

3.43-8
- Fixed: URL encoding bugs with filenames containing '%' characters (sandboxie32)
- Fixed: MacPorts Darwin/Mac fixes to webhttrack (Ross Williams)
- Fixed: Flash link extraction has been improved (Vincent/suei8423)

3.43-7
- Fixed: "Open error when decompressing" errors due to temporary file generation problems (Scott Mueller)

3.43-6
- Shell: WIN32 setup cosmetic fixes: do not probe the proxy on non-local network, do not force *.whtt registration

3.43-5
- Fixed: code tag handling bug in certain cases leading to produce invalid links (Tom-tmh13 and William Roeder)

3.43-4
- Fixed: horrible SSL slowdowns due to bogus select() calls (Patrick Pfeifer)
- Fixed: Konqueror fixes

3.43-3
- Updated: Portugues-Brasil language file

3.43-2
- Fixed: wizard question buggy, and commandline version did not print it (Maz)
- Fixed: do not rename xml subtypes (such as xsd schemas) (Eric Avrillon)

3.43
- Fixed: Fixed too aggressive javascript url= parsing (Chris)
- Fixed: fixed --urllist option "sticking" the list content to the list of URL (Andreas Maier)
- Fixed: "Previous cache file not found" not redownloading file when deleted before an update (William Roeder)
- Fixed: *.rpm.src files renamed to *.src.src with bogus servers (Hippy Dave)
- Fixed: "pause" is pausing much faster (William Roeder)
- Fixed: binary real media files and related files are no longer being parsed as html (William Roeder)
- Fixed: "File not parsed, looks like binary" check no longer corrupt the checked binary file
- Fixed: multiple download of error pages (several identical '"Not Found" (404) at link [identical link]') leading to a slowdown in certain cases (William Roeder)
- Fixed: sometimes, a double request was issued to update a broken file
- Fixed: display bug "link is probably looping, type unknown, aborting .."
- Fixed: missing library references at build time and other build related issues (Debarshi Ray)
- Fixed: on windows, switched from wsock32.dll to ws2_32.dll
- Fixed: minor argument size validation error for "-O" option (Joan CALVET)

3.42-3
- Fixed: Bad URL length validation in the commandline (CVE-2008-3429) (Joan CALVET)

3.42-2
- Fixed: Random crashes at the end of the mirror due to a dangling file pointer (Carlos, angus at quovadis.com.ar)

3.42
- Fixed: size limits are stopping the mirror gently, finishing pending transfers (David Stevenson)

3.41-3
- Fixed: text/plain advertised files renamed into .txt
- Fixed: broken configure.in

3.41-2
- Fixed: major WIN32 inlined function bug caused the cache not to be used at all, causing update not to work

3.41
- New: changed API/ABI to thread-safe ones (libhttrack1 2), big cleanup in all .h definitions
- Fixed: major memory usage bug when downloading large sites
- Fixed: do not rename files if the original MIME type was compatible
- Fixed: several source fixes for freeBSD (especially time problems)
- New: option %w to disable specific modules (java, flash..)
- Fixed: 'no space left in stack for back_add' error
- Fixed: fixed redirected images with "html" type
- Fixed: 'Crash adding error, unexpected error found.. [4268]' error

3.40-2
- Fixed: bogus '.del' filenames with ISO-9660 option
- Fixed: now merges the header charset even with an empty footer string
- New: --port option for webhttrack

3.40
- New: mms:// streaming capture (thanks to Nicolas Benoit!)
- New: proxyTrack project released
- New: new experimental parser that no longer needs link testing ('testing link type..')
- New: Redirect handled transparently with delayed type check and broken links made external when the "no error page" option is enabled
- New: improved background download to handle large sites
- New: '--assume foo/bar.cgi=text/html' is now possible
- New: MIME type scan rules (such as -mime:video/* -mime:video/mpeg)
- New: size scan rules now allows to rewrite uncaught links as external links
- Fixed: crash fixed when ftime()/localtime()==NULL
- Fixed: iso-9660 option now using '_' for collision character
- Fixed: collision problems with CaSe SeNsItIvItY
- Fixed: a href='..' fixed!
- Fixed: redirects are now handled by the new experimental parser
- Fixed: "./" links generated with fixed outbound links (../../)
- Fixed: 'base href' bogus in many cases
- Fixed: enforce security limits to avoid bandwidth abuses
- Fixed: bogus external (swf) parser, fixed remaining .delayed files
- New: new check-mime and save-file2 callbacks
- New: "always delayed type check" enabled
- Fixed: totally bogus finalizer causing compressed files not to be uncompressed, and many files to be truncated
- Shell: new Finnish interface added!
- Fixed: "..html" bogus type
- Fixed: remaining bogus .delayed entries
- Fixed: flush before user-defined command
- Fixed: fixed user-defined command call and background cleaner
- Fixed: fixed 'Crash adding error, unexpected error found.. [4250]' error
- Fixed: fixed cache absolute file reference (the reference is now relative) preventing the cache form being moved to another place
- Fixed: webhttrack 'Browse Sites' path bug
- Fixed: old httrack cache format fixes (import of older versions did not work anymore)
- Fixed: port fixes in htsnet.h
- Fixed: -N option with advanced extraction (bogus "not found" member)
- Fixed: javascript: location=URL was not recognized
- Fixed: no more character escaping when not needed (such as UTF-8 codes)
- Fixed: possibly temporary files left on disk with bogus servers giving compressed content on HEAD reuests
- Fixed: URL hack caused unexpected filename collisions (index.html vs INDEX.HTML)
- Fixed: "do not erase already downloaded file" option now correctly works (it leaves files linked in the mirror)
- Fixed: UCS2 encoded pages are now converted properly into UTF-8
- New: "near" option now also catch embedded (images, css, ..) files
- Fixed: bogus chunked multimedia link text files (such as x-ms-asf files)
- Fixed: compilation problems on Un*x version

3.33
- Fixed: Bogus redirects with same location in https
- Fixed: Bogus file naming with URL hack
- Fixed: Extremly slow redirections and empty files
- Fixed: Bogus names with directories ending with a "."
- New: Number of connection per second can now be.. decimal, to delay even more
- New: Enforce stronger ISO9660 compliance
- Shell: "URL Hack" in interface
- Shell: "Save settings" now rebuild categories
- Shell: "Shutdown PC after mirror" option
- Shell: Sound at the beginning/end or the mirror (configurable through system sound properties)
- Shell: Fixed drag & drop, .url import
- Shell: Fixed "wizard" mode (crash)
- Fixed: Crash at the end due to unterminated pending threads (Berti)
- Fixed: \ is not anymore transformed into / after the query (?) delimiter
- New: Two new callbacks for pre/post-processing html data
- New: link-detected2 callback (additional tag name parameter)
- Fixed: Broken ISO9660
- Fixed: Crash on file:// links
- Fixed: Unescaped ampersands (&) in URLs
- Fixed: Transfer hangs introduced in 3.33-beta-2
- Fixed: Display bug "Waiting for scheduled time.."
- Fixed: Bug "Waiting for scheduled time.." (NOT a display bug, actually)
- Fixed: CaSe SenSiTiViTy bugs with mutliple links reffering to the same URL but using different case styles
- Fixed: Failed to build from sources (FTBFS) on amd64 archs because of cast problems (Andreas Jochens)
- Fixed: & were converted into &nbsp; (Leto Kauler)
- Shell: Fixed crash with long URL lists (Libor Striz)
- Fixed: connection/seconds limiter replugged
- Fixed: "no files updated" display bug
- Fixed: bogus links encoded with UTF (Lukasz Wozniak)
- New: --assume can be used to force a specific script type (Brian Schröder)

3.32
- Fixed: css and js files were not parsed!
- Fixed: again broken file:// (infinite loops with local crawls)
- Fixed: Bandwidth limiter more gentle with low transfer rate
- Fixed: external wrappers were not called during updates/continue
- New: additional callback examples
- Fixed: overflow in unzip.c fixed
- New: tests are now cached for better performances!
- New: %r (protocol) option for user-defined structure
- Fixed: Broken engine on 64-bit archs

3.31
- New: Experimental categories implemented
- New: New cache format (ZIP file)
- New: .m3u files now crawled
- New: .aam files now crawled
- Fixed: Broken ftp
- Fixed: Broken file://
- Fixed: Broken cookies management and loading
- Fixed: HTTrackInterface.c:251 crash
- Fixed: "N connections" means "N connections" even in scan phase
- Fixed: javascript:location bug
- Fixed: libtool versioning problem fixed
- Fixed: More javascript bugs with \' and \"
- Fixed: .HTM files not renamed into .html anymore
- Fixed: OSX fixes in the Makefile script
- New: Default "referer" and "from" fields
- New: Full HTTP headers are now stored in cache
- Fixed: ftp transfer not logged/properly finalized
- Fixed: Missing symbolic link in webhttrack install
- Fixed: path and language not saved in webhttrack
- Shell: Avoid invalid project names
- Fixed: Javascript bug with src=
- Fixed: Keep-alive consistency problems on Linux/Unix with bogus servers (SIGPIPE)
- Fixed: Parsing bug inside javascript (bogus parsing with empty quotes in function: foo(''))
- Fixed: static compiling on Linux/Unix
- Fixed: bloated .h headers (internal function definitions)
- Fixed: Bogus query strings with embedded ../ and/or ./
- New: Added "change-options" call in the crawl beginning
- New: Query arguments now sorted for normalized URL checks (when "url hack" option is activated)
- Fixed: Previous dependency to zlib.dll to zlib1.dll
- Fixed: Broken static files were not correctly updated with the new cache format
- Shell: Launch button in Internet Explorer
- Fixed: Crash when dealing with multiple '?' in query string with 3.31-alpha

3.30
- New: Webhttrack, a linux/unix/bsd Web GUI for httrack
- New: "URL hack" feature
- New: HTTP-headers charset is now propagated in the html file
- New: loadable external engine callbacks
- New: Experimental ".mht" archives format
- Fixed: Query ?? bug
- Fixed: Bogus base href without http://
- Fixed: Several javascript bugs
- Fixed: UCS2 pages badly detected
- Fixed: Build structure change does not redownload files
- Fixed: "?foo" URL bug (link with only a query string) fixed
- Fixed: ' or " inside non-quoted URLs
- Fixed: keep-alive problems with bogus servers
- Fixed: Broken .ra files
- Fixed: More javascript bugs
- Fixed: ftp transfers not properly monitored in the shell
- Fixed: various fixes in webhttrack
- Fixed: Blank final page in webhttrack
- Fixed: Javascript comments (//) are skipped
- Fixed: Temporary fix for "archive" bug with multiple java archives
- Fixed: Inlined js or css files have their path relative to the parent
- Fixed: Unescaped quotes ("") when continuing/updating in commandline mode
- Fixed: Null-character in html page bug
- Fixed: External depth slightly less bogus
- Fixed: Filters based on size bogus ("less than 1KiB" is now functionning)
- Fixed: Strange behaviour with filters (last filter "crushed")
- Fixed: Bogus downloads when using ftp (unable to save file)
- Fixed: Freeze with keep-alive on certain sites due to bad chunk encoding handling
- Fixed: Problems with javascript included paths
- Fixed: The mirror now aborts when the filesystem is full
- Fixed: "No external pages" option fixed
- Fixed: Javascript and \" in document.write bug fixed
- Fixed: Two memory leaks in temporary file generation, and in link build fixed
- Fixed: Bogus compression with non-gzip format
- Fixed: Larger range of charsets accepted
- Fixed: Bogus robots.txt when using comments (#)
- Fixed: Missing MIME types for files such as .ico
- Shell: Fixed continuous proxy search
- Shell: Fixed missing HelpHtml/ link
- Fixed: Overflow in htsback.c:2779
- Fixed: Bogus style and script expressions due to too aggressive parsing
- Fixed: Javascript parsing bugs with \" and \'
- Fixed: Javascript link detection bugs when comments were inserted between arguments
- Fixed: Bug when valid empty gzip content was received
- New: More aggressive "maximum mirroring time" and "maximum amount of bytes transfered" options
- New: Windows file://server/path syntax handled
- Fixed: mht archive fixes
- Fixed: Serious bugs with filters given in commandline erased by the engine
- Fixed: Bogus parsing of javascript: generated inside document.write() inside javascript code removed

3.23
- New: Keep-alive
- New: URLs size limit is now 1024 bytes
- New: Bogus UCS2 html files hack
- Fixed: base href bugs
- Fixed: windows "dos devices" bug fixed
- Fixed: dirty parsing now avoids ","
- Fixed: "get non-html files near a link" option sometimes caused huge mirrors
- Fixed: Bugs if zlib library is not found
- Fixed: Bug with "near" and "no external pages"
- Fixed: "Link empty" crash
- Fixed: Several javascript bugs
- Fixed: Keep-alive problems ("unknown response structure")
- Fixed: Major keep-alive bug (connection not closed)
- Fixed: 8-3 options not working, ISO9660 option improved
- Fixed: Bogus links with embedded CR, TAB..
- Fixed: small ../ external link bug fixed

3.22-3
- Fixed: Slow engine due to (too strict) memory checks
- Fixed: Overflow in htscore.c:2353
- Fixed: Bogus chunked files with content-length fixed
- Fixed: Folders renamed into ".txt" on Un*x platforms bug fixed!
- New: Scan rule list (-%S) added
- New: Cache debugging tool (-#C) added

3.21-8
- New: Basic Macromedia Flash support (links extraction)
- New: Modular design for https, flash parser and zlib
- New: Standard autoconf/configure design on Un*x platforms
- New: Modular design also on Windows platforms (dll/lib)
- Fixed: Text files without extension not renamed "html" anymore
- Fixed: Bug with "?foo" urls
- Fixed: No chmod 755 on home anymore
- Fixed: Stability problems due to bad file structure checks
- Fixed: Overflow in GUI/commandline when displaying statistics
- Fixed: Directory creation error

3.20
- New: HTTPS support (SSL)
- New: ipv6 support
- New: 'longdesc' added
- New: new file 'new.txt' generated for transfer status reports
- New: ISO9660 compatibility option
- New: empty mirror/update detection improved
- New: Update hack now recognizes "imported" files
- New: Option to disable ipv4/ipv6
- New: Filters now recognize patterns like -https://*
- Fixed: The engine should be now fully reentrant
- Fixed: Fixes for alpha and other 64-bit systems
- Fixed: Files downloaded twice if not found in cache
- Fixed: ftp problems with 2xx responses
- Fixed: ftp problems with multiple lines responses
- Fixed: ftp %20 not escaped anymore
- Fixed: ftp RETR with quotes problems
- Fixed: now tolerent to empty header responses
- Fixed: hts-log closed
- Fixed: Compressed pages during updates
- Fixed: Crash when receiving empty compressed pages
- Fixed: Random crashes in 'spider' mode
- Fixed: bcopy/bzero not used anymore..
- Fixed: various code cleanups
- Fixed: Better UTF8 detection
- Fixed: External links now work with https and ftp
- Fixed: Top index.html corrupted or missing
- Fixed: URL list crashes
- Fixed: Random crashes with large sites due to bogus naming handler
- Fixed: Freezes on some robots.txt files
- Fixed: Compressed files not stored
- Fixed: SVG fixes
- Fixed: Raw HTML responses
- Fixed: 406 error workaround
- Fixed: Crashes due to binary files with bogus HTML type (not parsed anymore)
- Fixed: External https and ftp links broken, relative https links broken
- Fixed: Automatic resizing of filter stack
- Fixed: Various ampersand (&) elements added
- Fixed: https with proxy temporary workaround (direct connection)
- Fixed: "base href" with absolute uris
- Fixed: stack frame too large on some systems
- Fixed: random bad requests due to bogus authentication
- Fixed: crash with bogus ampersand escapes
- Fixed: local files
- Shell: Several fixes, including registration type problems
- Shell: "template files not found" fixed

3.16-2
- Fixed: Zlib v1.4
- Fixed: Gzipped files now downloaded without problems (HTTP compression bug)
- Fixed: Ending spaces in URLs now handled correctly
- Fixed: META-HTTP bug
- Shell: Type registration done only once

3.15
- Fixed: Bogus HTTP-referer with protected sites
- Fixed: Fatal IO/socket error with large sites (handles not closed)
- Fixed: K4 option now works
- Fixed: --continue-URL(s) now clears previous URLs
- Fixed: Parsing bug with 'www.foo.com?query'
- Fixed: Bug with embeded / and : in URLs
- Fixed: Bug parsing meta tags
- Shell: 'Soft cancel' documented
- Shell: 'Kx' options added

3.10
- Fixed: Broken pipes on Linux version
- Fixed: Commandline version bug with gzipped files
- Fixed: Crash when reaching compressed error pages
- Fixed: Bogus html-escaped characters in query strings
- Fixed: Files skipped (bogus anticipating system)
- Fixed: Crash when showing stats (div by zero)
- Fixed: Problems with URLs/redirects containing spaces or quotes
- Fixed: Slash added when ~ detected
- Fixed: Ugly VT terminal
- New: Faster and cleaner mirror interrupt

3.09
- Fixed: Several problems with javascript parsing
- Fixed: Elements after onXXX not parsed
- New: Source update wrapper
- New: Style url() and @import parsed
- Shell: Word database and maximum number of links
- Shell: Option changes taken in account immediately
- Shell: Cleaner installer (registry keys)

3.08
- New: HTTP compression is now supported
- New: Faster response analysis
- Fixed: External page in html if cgi
- Fixed: Mix between CR and CR/LF for comments
- Fixed: Top index corrupted
- Shell: Better refresh during parsing
- Shell: DLL error

3.07
- Fixed: Random crashes with HTTP redirects
- New: New rate limiter (should be sharper)
- New: Code cleaned up, new htscore.c/httrack.c files

3.06
- Fixed: Redirect to https/mailto now supported
- New: Top index/top dir for Un*x version
- New: Sources more modular (.so)
- New: Quicktime targetX= tags
- New: HTTP 100 partially supported

3.05
- Fixed: Non-scannable tag parameters ("id","name",..)
- Fixed: Java classes not found when using "." as separator
- Fixed: Java classes not found when missing .class

3.04
- Fixed: URLs with starting spaces
- Fixed: bogus URLs when using "base href"
- Shell: --assume and -%e options included
- New: Documentation updated a little

3.03
- New: Parser optimizations, 10 times faster now!
- New: New --assume option to speed up cgi tests
- New: Option to avoid Username/password storage for external pages
- New: Query string kept for local URIs
- Fixed: RFC2396 compliant URLs accepted (//foo and http:foo)
- Fixed: foo@foo.com not considered as URL anymore
- Fixed: Space encoded into %20 in URIs
- Fixed: "Unable to save file" bug
- Fixed: Corrupted top index.html
- Fixed: Cookies disabled with --get
- Fixed: Cache bug for error pages

3.02
- Fixed: Pages without title recorded in top index
- Fixed: Error with Content-type-Content-disposition
- Fixed: backblue.gif/external.html files not purged anymore
- Fixed: Encoding problems with files containing %2F or other characters
- Fixed: Write error reported for HTML files
- New: hts-stop.lock file to pause the engine
- New: New install system using InnoSetup

3.01
- New: HTTP real media files captured
- Fixed: Bogus statitics
- Fixed: Minor fixes

3.00
- New: New interface, with MANY improvements!
- New: Better parsing (enhanced javascript parsing, sharper HTML parsing)
- New: Faster and more efficient background download system
- New: ETag properly handled
- New: Optional URL list
- New: Optionnal config file
- New: New structure options
- New: New filters options (size filters)
- New: Better password site handling
- New: Traffic control to avoid server overload
- New: Setuid and Chroot for Unix release
- New: limited 64-bit handling
- New: .js files are now parsed
- New: Single hts-log.txt file, error level
- New: New top index.html design
- New: "Update hack" option to prevent unnecessary updates
- New: Default language sent for mirrors
- New: Searchable index
- Fixed: Bogus ftp routines (Linux version)
- Fixed: Bug that caused to mirror a complete site from a subdir
- Fixed: Bug that caused restart to be very slow
- Fixed: Bug that caused loops on several query-string pages (?foo=/)
- Fixed: Corrupted cache bug
- Fixed: Random broken links (pages not downloaded)
- Fixed: Shared links problems
- Fixed: Bogus URLs with commas (,)
- Fixed: Bogus / and \ mixed
- Fixed: Bogus addresses with multiple @
- Fixed: Bogus links with %2E and %2F
- Fixed: Bogus empty links
- Fixed: "Unexpected backing error" bug fixed
- Fixed: Files with incorrect size no more accepted
- Fixed: Top index.html created even for untitled pages
- Fixed: Bogus N100 option (unable to save file)
- Fixed: Deadlock when using many hosts in URLs
- Fixed: Password stored internally to avoid access errors
- Fixed: Fixed /nul DOS limit
- Fixed: Bogus -* filter (nothing mirrored)
- Fixed: .shtml now renamed into .html
- Fixed: Content-disposition without ""
- Fixed: External html page for /foo links
- Fixed: Username/password % compliant
- Fixed: Javascript parser sometimes failed with " and ' mixed
- Fixed: Some Range: bugs when regeting complete files
- Fixed: Range: problems with html files
- Fixed: HTTP/1.1 407 and 416 messages now handled
- Fixed: Bogus timestamp
- Fixed: Null chars in HTML bug
- Fixed: Error pages cache bug
- Fixed: Connect error/site moved do not delete everything anymore!
- Fixed: Bogus garbage ../ in relative URL
- Shell: New transfer rate estimation
- Shell: Fixed crash when using verbose wizard
- Shell: dynamic lang.h for easier translation updates
- Shell: Fixed some options not passed to the engine
- Fixed: A lots of minor fixes!

2.2
Note: 3.00 alpha major bug fixes are included in the 2.2

2.02
- New: Cache system improved, compatible with all platforms
- New: Update process improved (accurate date)
- New: Remote timestamp for files
- New: ETag (HTTP/1.1) supported
- Shell: Portugese interface available
- Fixed: Bug with links containing commas
- Fixed: 'file://' bug with proxy
- New: Engine a little bit faster
- Shell: Some bugs fixed in the interface

2.01
- New: ftp through proxy finally supported!
- New: Sources cleaned up
- New: Again some new marvelous options
- New: Speed improved (links caught during parsing, faster "fast update")
- New: Tool to catch "submit" URL (forms or complex javascript links)
- Shell: German interface available
- Shell: Dutch interface available
- Shell: Polish interface available
- Fixed: Level 1 bug fixed
- Fixed: Still some parsing/structure problems
- Fixed: Referer now sent to server
- Fixed: Cookies did not work properly
- Fixed: Problems with redirect pages
- New: Better javascript parsing
- Fixed: Problems with URL-parameters (foo.cgi?param=2&choice=1)
- Fixed: Problems with ftp
- New: ftp transfers are now in passive mode (firewall compliant)

2.00 -- The First Free Software Release of HTTrack!
- New: HTTrack sources (command line), now free software, are given
- Shell: Interface rewritten!
- New: Documentation rewritten
- Shell: Drag&Drop abilities
- Shell: More URL informations
- Shell: Fixed: Remote access problems
- Fixed: Loop problems on some sites causing crashes
- Fixed: URL encoding problems
- Fixed: Some file access problems for ../
- Fixed: Some fixes for updating a mirror
- Shell: Crazy progress bar fixed
- Fixed: Form action are rewritten so that cgi on form can work from an offline mirror
- Fixed: Crashes after continuing an "hand-interrupted" mirror
- Fixed: Bogus files with some servers (chunk bug)

1.30
- Shell: Interface improved
- New: robots.txt are followed by default
- New: Parsing speed improved on big (>10,000 links) sites with an hash table
- New: Mirror Link mode (mirror all links in a page)
- New: Cookies are now understood
- New: No external pages option (replace external html/gif by default files)
- New: Command line version improved, background on Unix releases
- Fixed: Problems with javascript parsing
- Fixed: Username/password not set to lowercase anymore
- Fixed: Problems with base href
- New: Links in level 1 html files now patched
- New: Expurge now deletes unused folders
- New: Option -V executes shell command for every new file
- Shell: Primary filter now works

1.24
- Fixed: Ftp protocol bogus (with login/pass)
- Fixed: Cache problems (corrupted files)
- New: Expurge old files for updates
- New: "Updated" messages for mirror updates
- Shell: Autodial/hangup option to RAS
- Fixed: index.html were not created sometimes
- Shell: Fixed: Random crashes with the interface
- Shell: Fixed: Filters profile not saved
- Fixed: Various (and numerous) fixes

1.23
- Shell: Interface improved
- Shell: Multiple setups
- Shell: Redefine options
- Shell: Resume interrupted mirror improved

1.22
- Fixed: Parsing up/down did not work well
- Fixed: Several files not catched, bugs fixes
- Fixed: Problems with classes (1.21)
- New: Transfer rate can be limited (-A option)
- Shell: Smooth refresh
- New: ftp basic protocol a little bit improved

1.21
- Fixed: Several java classes were not parsed
- Fixed: Some folders without ending / ignored
- Fixed: Crashes due to content-type too long

1.20
- Shell: documentation!
- Fixed: Some problems with 'host cancel' system after timeouts (crashes)
- New: Get only end of files if possible (file partially get)
- New: New cache system (only HTML stored)
- New: User-defined structure possible
- New: Also available: french interface
- Fixed: Random crashes (div by 0/illegal instruction) with null size files
- New: Limited ftp protocol (files only), e.g. -ftp://* now works
- Fixed: Some connect problems with several servers or proxies
- New: New option, save html error report by default
- Shell: Browse and see log files at the end of a mirror
- New: Proxy authentication (ex: guest:star@myproxy.com:8080)
- Shell: Interface improved (especially during mirror)
- Fixed: Ambiguous files are renamed (asp,cgi->html/gif..)
- Shell: New test link mode option
- New: Site authentication (ex: guest:star@www.myweb.com/index.html)
- Fixed: Minor bugs fixed
- Shell: See log files during a mirror
- Fixed: Some problems using CGI (different names now)
- Fixed: Go down/up/both options and filters
- Fixed: "Store html first" did not work
- New: -F option ("Browser ID") disguise HTTrack into a browser
- New: New filter system
- Shell: New "Save as default" options
- Fixed: "Build options" did NOT work properly! (files overwritten or missing)
- Fixed: User agent ID fixed
- Shell: Skip options
- Shell: Better interface control during mirrors
- Shell: InstallShield and Help files
- Fixed: Some external links were not filtered sometimes
- Fixed: Mirror crash at the end

1.16b
- Shell: Really *stupid* bug fixed causing WinHTTrack to be slooow
- Fixed: Crash if the first page has no title fixed
- Fixed: Bogus options like "Just scan" saved empty files
- Fixed: Forbid all links (*) with manual accept did not work
- Shell: Filters interface improved

1.16:
- New : Java Classes and subclasses are now retrieved!
- New: Better JavaScripts parsing
- New: Option: Abandon slowest hosts if timeout/transfer too slow
- Shell: Interface improved

1.15b
- Fixed: Some bugs fixed

1.15:
- Shell: Interface improved
- New: Robot improved (some files through javascript are now detected!)
- New: Improved wild cards (for example, -www.*.com/*.zip)
- New: 'config' file to configurate proxy, path.. only once

1.11
- New: Wait for specific time (begin transfer at specific hour)
- New: Time limit option (stops transfer after x seconds)
- Shell: Interface improved for an easy use

1.10e
- Fixed: Maps were not correctly managed (stupid bug)

1.10d:
- Fixed: Bogus index.html fixed

1.10c
- Shell: "Time out" field needed "transfer rate" field

1.10b
- Fixed: Better memory management

1.10
- New: "Transfer rate out" option added (abandon slowests sites)
- New: "Deaf" hosts do not freeze HTTrack any more
- Fixed: Again problems with code/codebase tags
- New: Broken links detection improved

1.04
- Fixed:Some links were not correctly read (pages with "codebase" tags)
- Shell: Interface improved

1.03 (No changes for the command-line robot)
- Shell: Big bug fixed! (VERY slow transfer rates..)

1.02
- Fixed: Some java files were not correctly transfered
- New: Speed has been improved
- Fixed: Log file more accurate
- Shell: Interface has been improved

1.01
- Fixed: Structure check error in some cases

1.00 -- The 1.00, Yeah!
- New: base and codebase are now scanned

0.998 beta-2
- Fixed: Multiple name bug (files having the same name in the same directory) with -O option fixed

0.997 beta-2
- Fixed: Filenames with '%' were not correctly named
- Fixed: Bug detected in 0.996: several files are not written on disk!!

0.996 beta-2
- New: -O option (path for mirror and log)
- New: Unmodified file time/date are not changed during an update

0.99 beta-2
- New: User-agent field
- New: Shortcuts (--spider etc.)
- New: Links not retrieved are now rebuilt absolutly
- New: The 'g' option (juste get files in current directory) has been added
- New: Primary links analyste has been improved
- Fixed: "304" bug fixed

0.25 beta-2
- Fixed: Freeze during several mirrors fixed!
- New: More 'N' options (filenames type)

0.24 beta-2
- Fixed: Restart/Update with cache did not work (really not..)
- Fixed: Wild cards now work properly (e.g. -www.abc.com* do works)
- New: The 'n' option (get non-html files near a link) has been added!

0.23 beta-2
- Fixed: The 'M' option (site size) did not work
- Fixed: Files larger than 65Kb were not correctly written

older beta
- Many, many bugs fixed
