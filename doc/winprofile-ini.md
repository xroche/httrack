# hts-cache/winprofile.ini

The project settings a HTTrack front end saves beside a mirror, so reopening the project restores the wizard. Three programs touch it, in two repositories:

- **WinHTTrack** (httrack-windows, `WinHTTrack/Shell.cpp`) writes it from the dialog fields and reads it back. It is the original writer, and its conventions are the ones below.
- **WebHTTrack** (`src/htsserver.c`, `html/server/step[24].html`) writes it from the posted form and reads it into the wizard's session state.
- The **engine** reads one key, `Category`, to group projects in the top index, and with its own decoder: case-insensitive, first occurrence, `unescapehttp` rather than `unescapeini`, so a `+` becomes a space (`src/htstools.c`).

Neither GUI needs the file to run a mirror: the command line the wizard builds is what the engine takes, and `hts-cache/doit.log` records it. Getting the file wrong loses settings on reopen; it does not corrupt a mirror.

## Syntax

`key=value`, one per line, CRLF, no `[section]` header despite the extension. The first `=` separates; later ones belong to the value. Lines are capped at 8192 bytes by WebHTTrack and 32000 by WinHTTrack.

Neither GUI writes the other's full key set, so a missing key is the normal case rather than an edge: a WebHTTrack save drops `MailIndex`, `AcceptLanguage`, `OtherHeaders` and `DefaultReferer`, and WinHTTrack drops `HostAlias`, `WarcFile` and `WarcMaxSize`. **A reader that does not find a key falls back to its own default, which is often not "off" or "zero"**: WinHTTrack defaults `ParseAll`, `Cache`, `Index`, `Log`, `KeepAlive`, `Cookies`, `CheckType` and `Travel` to 1, `FollowRobotsTxt` to 2 and `PrimaryScan` to 3 (`Shell.cpp:2929-3011`). Omission happens inside one GUI too: WinHTTrack skips a list key when its combo has no selection (`GetCurSel() != CB_ERR`). Write a key whenever you have a value for it.

Two more divergences, both older than this document:

- **Duplicate keys resolve differently.** WebHTTrack takes the last occurrence, WinHTTrack the first (`MyGetProfileStringFile` returns on its first match). Write each key once.
- **The escapes are not the same set.** WinHTTrack escapes `%`, `=`, TAB, CR and LF and passes every other byte through, so its decoder is the exact inverse and turns **every other** `%xx` into a space; that decode is also case-sensitive, and `%0D` or `%3D` becomes a space. WebHTTrack escapes `%` and any byte under 32, writes `=` raw, and writes `%22` for a double quote in the 18 `${unquoted:}` fields. So a footer or a filter list holding a quote survives a WebHTTrack round trip and degrades in WinHTTrack, and an `=` inside a value survives only in the other direction. WebHTTrack's own decoder also collapses a `%0d%0a` pair to a lone CR, so a multi-line value comes back CR-separated.

Neither side transcodes. WinHTTrack is an ANSI build and writes local-codepage bytes; WebHTTrack writes back whatever the browser posted, in the catalog's `LANGUAGE_CHARSET`. A project moved between two machines on the same codepage keeps its accents, and one moved across codepages does not.

## Value conventions

Four kinds of value, and the kind decides how `0` reads:

| Kind | Spelling | Notes |
| --- | --- | --- |
| String | verbatim, escaped as above | `CurrentUrl`, `Category`, `UserID`, `Footer`, the `MIMEDefs*` pairs. `Depth`, `MaxRate` and `Sockets` are numbers written as strings, so empty is legal and means unset |
| Checkbox | `1` on, explicit `0` off | Listed in `ini_checkbox_keys[]` in `src/htsserver.c` |
| List | the **0-based** index of the entry, in `LISTDEF_N` order (`lang.def`) | Listed in `ini_list_keys[]` |

The list keys are `CurrentAction`, `Build`, `PrimaryScan`, `Travel`, `GlobalTravel`, `RewriteLinks`, `CheckType`, `FollowRobotsTxt` and `LogType`. WinHTTrack stores the combo box's `GetCurSel()`, which is where 0-based comes from. WebHTTrack numbers the same `<select>` from 1, because id 0 is how its templates spell "no value". It shifts by one at the file boundary rather than renumbering the options, and leaves a value alone that is not a plain number.

That last clause is load-bearing today: WinHTTrack's main save path writes `CheckType`, `FollowRobotsTxt` and `LogType` with `GetDlgItemText`, so those three carry the combo's displayed **text**, not its index, while its own reader takes them with `atoi` (`Shell.cpp:2730, 2788, 2811`). Reported to httrack-windows; until it is settled, a reader has to tolerate both spellings for those three.

`ProxyType` is a tenth 0-based combo index (`0` HTTP, `1` SOCKS5, `2` HTTP CONNECT), written by both GUIs and deliberately **outside** `ini_list_keys[]`: WebHTTrack's proxy page already reads `0` as its own first entry, so the two agree and a shift would break them. Adding an entry to that list in one GUI alone is #1314 over again.

`ProfileFormat=1` marks the current conventions. A file without it is WinHTTrack's own and follows the same ones, so the marker changes no read decision today; it exists so a later format change can be recognized. Both GUIs write it; WinHTTrack does not yet, and rewrites the file from its own key list, so a file it saves loses the marker until it does.

## Changing the format

A new key needs the same value on both sides or it is worse than no key. Adding one to WebHTTrack alone is fine and common (`HostAlias`, `WarcFile`); adding one whose *meaning* differs between the GUIs is what #1314 was.

`tests/322_webhttrack-list-ids.test` and `tests/274_wizard-profile-load.test` hold the two tables in `src/htsserver.c` against the keys `step4.html` writes. Nothing checks either against `Shell.cpp`, so a change to WinHTTrack's side still has to be read across by hand.
