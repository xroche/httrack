# hts-cache/winprofile.ini

The project settings a HTTrack front end saves beside a mirror, so reopening the project restores the wizard. Four programs touch it, in three repositories:

- **WinHTTrack** (httrack-windows, `WinHTTrack/Shell.cpp`) writes it from the dialog fields and reads it back. It is the original writer, and its conventions are the ones below.
- **WebHTTrack** (`src/htsserver.c`, `html/server/step[24].html`) writes it from the posted form and reads it into the wizard's session state.
- **HTTrack for Android** (httrack-android, `app/src/main/java/com/httrack/android/OptionsMapper.java`) reads and writes the same keys, with its own table of key to flag and key to default.
- The **engine** reads one key, `Category`, to group projects in the top index, and with its own decoder: case-insensitive, first occurrence, `unescapehttp` rather than `unescapeini`, so a `+` becomes a space (`src/htstools.c`).

Neither GUI needs the file to run a mirror: the command line the wizard builds is what the engine takes, and `hts-cache/doit.log` records it. Getting the file wrong loses settings on reopen; it does not corrupt a mirror.

## Syntax

`key=value`, one per line, CRLF, no `[section]` header despite the extension. The first `=` separates; later ones belong to the value. Lines are capped at 8192 bytes by WebHTTrack and 32000 by WinHTTrack; Android caps nothing.

Neither GUI writes the other's full key set, so a missing key is the normal case rather than an edge: a WebHTTrack save drops `MailIndex`, `AcceptLanguage`, `OtherHeaders` and `DefaultReferer`, and WinHTTrack drops `WarcFile` and `WarcMaxSize`. **A reader that does not find a key falls back to its own default, which is often not "off" or "zero"**: WinHTTrack defaults `ParseAll`, `Cache`, `Index`, `Log`, `KeepAlive`, `Cookies`, `CheckType` and `Travel` to 1, `FollowRobotsTxt` to 2 and `PrimaryScan` to 3 (the `MyGetProfileInt` defaults in `Read_profile`). Omission happens inside one GUI too: WinHTTrack skips a list key when its combo has no selection (`GetCurSel() != CB_ERR`). Write a key whenever you have a value for it.

Two more divergences, both older than this document:

- **Duplicate keys resolve differently.** WebHTTrack and Android take the last occurrence, WinHTTrack the first (`MyGetProfileStringFile` returns on its first match). Write each key once.
- **The escapes are not the same set.** WinHTTrack escapes `%`, `=`, TAB, CR and LF and passes every other byte through, so its decoder is the exact inverse and turns **every other** `%xx` into a space; that decode is also case-sensitive, and `%0D` or `%3D` becomes a space. WebHTTrack escapes `%` and any byte under 32, writes `=` raw, and writes `%22` for a double quote in the 18 `${unquoted:}` fields. Android's `profileEncode` is WebHTTrack's set without the quote case. A footer or a filter list holding a quote therefore survives a WebHTTrack round trip and degrades in WinHTTrack, and an `=` inside a value survives only in the other direction. WebHTTrack collapses a `%0d%0a` pair to a lone CR; Android keeps CRLF. Android also **throws** on an escape it cannot parse or a raw control byte, abandoning the whole profile where the other two degrade one value.

Neither side transcodes. WinHTTrack is an ANSI build and writes local-codepage bytes; WebHTTrack writes back whatever the browser posted, in the catalog's `LANGUAGE_CHARSET`. A project moved between two machines on the same codepage keeps its accents, and one moved across codepages does not.

## Value conventions

Four kinds of value, and the kind decides how `0` reads:

| Kind | Spelling | Notes |
| --- | --- | --- |
| String | verbatim, escaped as above | `CurrentUrl`, `Category`, `UserID`, `Footer`, the `MIMEDefs*` pairs. `Depth`, `MaxRate` and `Sockets` are numbers written as strings, so empty is legal and means unset |
| Checkbox | `1` on, explicit `0` off | Listed in `ini_checkbox_keys[]` in `src/htsserver.c` |
| List | the **0-based** index of the entry, in `LISTDEF_N` order (`lang.def`) | Listed in `ini_list_keys[]` |
| Bitmask | `Dos` only: bit 0 DOS 8.3 names, bit 1 ISO 9660 | In neither table. See below |

The list keys are `CurrentAction`, `Build`, `PrimaryScan`, `Travel`, `GlobalTravel`, `RewriteLinks`, `CheckType`, `FollowRobotsTxt` and `LogType`. WinHTTrack stores the combo box's `GetCurSel()`, which is where 0-based comes from. WebHTTrack numbers the same `<select>` from 1, because id 0 is how its templates spell "no value". It shifts by one at the file boundary rather than renumbering the options, and leaves a value alone that is not a plain number.

That last clause is load-bearing: a WinHTTrack older than httrack-windows#124 saved `CheckType`, `FollowRobotsTxt` and `LogType` with `GetDlgItemText`, so those keys carry the combo's displayed **text** rather than its index (and `Cookies` and `StoreAllInCache` the control's caption). Its own reader took them with `atoi`, so such a file silently reopened on the reader's default. Those files exist, so a reader still has to tolerate both spellings.

Three checkbox keys were rotated in WebHTTrack until #1324: it filed the "test all links" box under `Near`, the "catch all URLs" box under `Test`, and the "get non-HTML files near a link" box under `ParseAll`. WinHTTrack's names match their meanings on all three, and Android's table agrees with it key for key, so WebHTTrack was the lone outlier. Files WebHTTrack saved before that fix carry the rotation and nothing marks them, so those three settings come back shuffled once.

Android spells three keys that already had WinHTTrack names differently: `ProxyProtocol` for `ProxyType`, `KeepWwwPrefix` for `KeepWww`, `KeepDoubleSlashes` for `KeepSlashes` (its field-to-key tables). The values agree, so only the name keeps those settings from crossing.

`Dos` is the one key that is not a number, a checkbox or a list. WinHTTrack packs two independent boxes into it, `m_dos | (m_iso9660 << 1)`, and reads them back with `& 1` and `& 2`, so it alone can write 3. The other two lose it in different places. WebHTTrack has no "both" entry, so it shows 3 as DOS names and drops the ISO bit on re-save; Android decodes only an exact `1`, so 2 and 3 arrive as neither. `Dos=0` is neither box and so means long names, not DOS names, which is why a front end that cannot show 3 must fall back on 1: storing 0 turns the setting into its opposite on the next reopen. Both bits set means DOS names on every side, since that is the precedence WinHTTrack applies when it builds `-L`.

`ProxyType` is a tenth 0-based combo index (`0` HTTP, `1` SOCKS5, `2` HTTP CONNECT), written by both GUIs and deliberately **outside** `ini_list_keys[]`: WebHTTrack's proxy page already reads `0` as its own first entry, so the two agree and a shift would break them. Adding an entry to that list in one GUI alone is #1314 over again.

`ProfileFormat=1` marks the current conventions. Nothing reads it yet, so its presence changes no behaviour. It is there so a later format change can be told apart, which needs every writer to stamp it first.

## The machine-readable form

`winprofile-keys.tsv` at the repository root states every key as data: which
front ends write it, its scope and kind, the engine options it produces, its
default, what a present-but-empty value means, and the key it is an old spelling
of. `winprofile-escapes.tsv` does the same for the codec, carrying the vectors
all three encode identically and marking the cases where they diverge.

`tools/gen-winprofile-keys.py` turns the table into `src/winprofile-keys.h`, and
`tests/324_winprofile-table.test` regenerates that header, diffs it, and checks
this repository's own templates and tables against the rows. Generation only
runs that way round: parsing the header back is how three hand-kept copies
drifted in the first place.

Read a row rather than trusting prose here, and where the two disagree the table
is the one with a test.

## Changing the format

httrack-windows asserts the lossy decode in its own self-test, so converging the two escape sets means moving that expectation in the same breath. A new key needs the same value on both sides or it is worse than no key. Adding one to WebHTTrack alone is fine and common (`WarcFile`, `WarcMaxSize`); adding one whose *meaning* differs between the GUIs is what #1314 was.

`tests/322_webhttrack-list-ids.test` and `tests/274_wizard-profile-load.test` hold the two tables in `src/htsserver.c` against the keys `step4.html` writes. Nothing checks either against `Shell.cpp`, so a change to WinHTTrack's side still has to be read across by hand.
