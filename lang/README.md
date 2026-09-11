# Translating HTTrack

Interface strings live here, one `.txt` file per language. `English.txt` is the reference: every other file maps each English string to its translation.

**Send a translation only into a language you read natively, and never send machine or LLM output.** Nobody here can check it, and a bad string is worse than the English it would replace.

## File format

Plain text, entries in consecutive pairs of lines:

```
<English string>
<translation>
```

The first line of a pair is the lookup key and must stay identical to the one in `English.txt`; translate only the second line. Missing entries fall back to the English text at runtime, so a partial translation works.

Preserve any `\r\n`, `\n`, `\t` and `printf` placeholders (`%s`, `%d`, ...) in the translation, in the same order: the strings go through `sprintf`, so a `%s` that swaps places with a `%d` picks up the wrong argument. A backslash before anything else is not an escape and loses its backslash, so a `\n` that lost its `n` silently joins two lines.

Line breaks are load-bearing in the dropdown strings, where each `\n` starts a list item. A break lost there shifts every row after it, and the user picks something other than what the row named.

Some words are not translatable text but names on disk or on the wire, and must be copied through unchanged: `hts-cache`, `robots.txt`, `cgi-bin`, and the `web/html` and `web/images` directories in the local-structure list, along with the bare `web/` they sit under. Translating one points the user at a path HTTrack never creates. Example hostnames such as `www.someweb.com` are the opposite: localise them freely. `site_name` and `www.domain.xxx` are placeholders, kept as they are only so every catalog reads alike.

Save the file as UTF-8. The catalogs each named their own legacy charset until 3.50 and several disagreed with their own bytes, Romanian for years, so an editor that rewrote a file silently corrupted it; there is nothing left to disagree with now.

`tests/62_lang-integrity.test` checks most of the above: the placeholder sequence, escapes it does not recognise, the line-break count, the literal names `hts-cache`, `robots.txt`, `cgi-bin`, `web/html` and `web/images`, and text re-encoded from the wrong source. It does not check `\t` or `\r\n` counts, the bare `web/`, or `site_name` and `www.domain.xxx`; follow those rules anyway. Two of its counts are pinned per catalog, in `tests/62_lang-untranslated.counts` and `tests/62_lang-linebreaks.counts`, because the existing files carry long-standing differences not worth churning; if your change moves one of those numbers, look at why before editing the pin to match.

A few `LANGUAGE_*` entries at the top describe the file itself:

| Key | Meaning |
| --- | --- |
| `LANGUAGE_NAME` | Name shown in the language picker, in its own language (`Deutsch`, not `German`) |
| `LANGUAGE_FILE` | The filename without `.txt`, in ASCII (`Portugues-Brasil`). It reaches the update endpoint as `Language=` |
| `LANGUAGE_ISO` | ISO 639 code, with region if needed (`de`, `pt_BR`) |
| `LANGUAGE_AUTHOR` | Your name and contact |
| `LANGUAGE_WINDOWSID` | Windows locale name used by WinHTTrack (`German (Standard)`) |

## Updating a language

Edit `<Language>.txt`: translate each second line and leave the English keys untouched. Then open a pull request, or attach the file to a GitHub issue.

When new strings land in `English.txt` they show up untranslated (as English) until a translator fills them in.

## Adding a language

A translator only has to send the catalog: copy `English.txt` to `<Language>.txt`, translate what you can, and fill in the `LANGUAGE_*` header. A partial file is worth sending. A maintainer does the rest.

## Wiring a new catalog into the build

The suite fails until all of this is done:

- `lang.def`: append the basename and the next `LANGUAGE_<N>`.
- `lang.indexes`: append `<iso>:<N>` for that same N, with the code lowercased (`pt_br`). webhttrack turns the caller's locale into that number, so a wrong one serves another catalog.
- `tests/62_lang-untranslated.counts` and `tests/62_lang-linebreaks.counts`: add one row to each.
- `tests/install-manifest.txt`: add the installed `lang/<Language>.txt` path.
- `greetings.txt`: add a row under `Translations` for the translator, keyed by the English name of the language. `AUTHORS` and `html/contact.html` repeat that roster, so the row goes into all three.
- `tests/373_credits.test`: add a `LABEL` entry when `LANGUAGE_WINDOWSID` is not that English name (`Uzbek Latin`, `FYRO Macedonian`).
