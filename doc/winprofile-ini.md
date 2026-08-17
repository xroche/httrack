# hts-cache/winprofile.ini

The project settings a HTTrack front end saves beside a mirror, so reopening the
project restores the wizard. Three programs touch it, in two repositories:

- **WinHTTrack** (httrack-windows, `WinHTTrack/Shell.cpp`) writes it from the
  dialog fields and reads it back. It is the original writer, and its
  conventions are the ones below.
- **WebHTTrack** (`src/htsserver.c`, `html/server/step[24].html`) writes it from
  the posted form and reads it into the wizard's session state.
- The **engine** reads one key, `Category`, to group projects in the top index
  (`src/htstools.c`).

Neither GUI needs the file to run a mirror: the command line the wizard builds
is what the engine takes, and `hts-cache/doit.log` records it. Getting the file
wrong loses settings on reopen, it does not corrupt a mirror.

## Syntax

`key=value`, one per line, CRLF, no `[section]` header despite the extension.
The first `=` separates; later ones belong to the value. A reader ignores keys
it does not know, and neither GUI writes the other's full key set, so a file
carries whichever keys its last writer knew.

Two divergences to keep in mind, both older than this document:

- **Duplicate keys resolve differently.** WebHTTrack takes the last occurrence,
  WinHTTrack the first (`MyGetProfileStringFile` returns on its first match).
  Write each key once.
- **The escapes are not the same set.** WebHTTrack encodes `%` as `%%` and any
  byte under 32 as `%xx`, and also emits `&lt; &gt; &amp; &#39;` for those four
  characters. WinHTTrack decodes `%%`, `%0d`, `%0a`, `%09` and `%3d`, and turns
  **every other** `%xx` into a space (`profile_decode`). A value holding an
  escaped byte outside that set survives a WebHTTrack round trip and degrades in
  WinHTTrack.

## Value conventions

Four kinds of value, and the kind decides how `0` reads:

| Kind | Spelling | Notes |
| --- | --- | --- |
| String | verbatim, escaped as above | `CurrentUrl`, `Category`, `UserID`, `Footer`, the `MIMEDefs*` pairs |
| Number | decimal | `Depth`, `MaxRate`, `Sockets`. `0` is the user's answer, never "unset" |
| Checkbox | `1` on, `0` or absent off | Listed in `ini_checkbox_keys[]` in `src/htsserver.c` |
| List | the **0-based** index of the entry, in `LISTDEF_N` order (`lang.def`) | Listed in `ini_list_keys[]` |

The list keys are `CurrentAction`, `Build`, `PrimaryScan`, `Travel`,
`GlobalTravel`, `RewriteLinks`, `CheckType`, `FollowRobotsTxt` and `LogType`.
WinHTTrack stores the combo box's `GetCurSel()` directly, which is where 0-based
comes from. WebHTTrack numbers the same `<select>` from 1, because id 0 is how
its templates spell "no value", so it shifts by one at the file boundary rather
than renumbering the options.

`ProfileFormat=1` marks a file written by WebHTTrack since #1314. A file
without it is WinHTTrack's own and follows the same conventions, so the marker
changes no read decision today; it exists so a later change to the format can be
told apart. WinHTTrack rewrites the file from its own key list and drops the
marker, which is correct, not a loss.

## Changing the format

A new key needs the same value on both sides or it is worse than no key. Adding
one to WebHTTrack alone is fine and common (`HostAlias`, `WarcFile`); adding one
whose *meaning* differs between the GUIs is what #1314 was.

`tests/322_webhttrack-list-ids.test` and `tests/274_wizard-profile-load.test`
hold the two tables in `src/htsserver.c` against the keys `step4.html` writes.
Nothing checks either against `Shell.cpp`, so a change to WinHTTrack's side
still has to be read across by hand.
