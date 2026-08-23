# Translating HTTrack

Interface strings live here, one `.txt` file per language. `English.txt` is the reference: every other file maps each English string to its translation.

## File format

Plain text, entries in consecutive pairs of lines:

```
<English string>
<translation>
```

The first line of a pair is the lookup key and must stay identical to the one in `English.txt`; translate only the second line. Missing entries fall back to the English text at runtime, so a partial translation works.

Preserve any `\r\n`, `\n`, `\t` and `printf` placeholders (`%s`, `%d`, ...) in the translation, in the same order: the strings go through `sprintf`, so a `%s` that swaps places with a `%d` picks up the wrong argument. A backslash before anything else is not an escape and loses its backslash, so a `\n` that lost its `n` silently joins two lines.

Line breaks are load-bearing in the dropdown strings, where each `\n` starts a list item. A break lost there shifts every row after it, and the user picks something other than what the row named.

Some words are not translatable text but names on disk or on the wire, and must be copied through unchanged: `hts-cache`, `robots.txt`, `cgi-bin`, and the `web/`, `web/html` and `web/images` directories in the local-structure list. Translating one points the user at a path HTTrack never creates. Example hostnames such as `www.someweb.com` are the opposite: localise them freely. `site_name` and `www.domain.xxx` are placeholders, kept as they are only so every catalog reads alike.

`tests/62_lang-integrity.test` enforces all of this. Two of its counts are pinned per catalog, in `tests/62_lang-untranslated.counts` and `tests/62_lang-linebreaks.counts`, because the existing files carry long-standing differences that are not worth churning; if your change moves one of those numbers, look at why before editing the pin to match.

A few `LANGUAGE_*` entries at the top describe the file itself:

| Key | Meaning |
| --- | --- |
| `LANGUAGE_NAME` | Name shown in the language picker, in its own language (`Deutsch`, not `German`) |
| `LANGUAGE_ISO` | ISO 639 code, with region if needed (`de`, `pt_BR`) |
| `LANGUAGE_CHARSET` | Encoding the file is saved in (`ISO-8859-1`, `windows-1251`, `UTF-8`, ...) |
| `LANGUAGE_AUTHOR` | Your name and contact |
| `LANGUAGE_WINDOWSID` | Windows locale name used by WinHTTrack (`German (Standard)`) |

Save the file in exactly its declared `LANGUAGE_CHARSET`; an editor that rewrites it as UTF-8 will corrupt the non-ASCII bytes.

## Adding or updating a language

1. Copy `English.txt` to `<Language>.txt`, or edit the existing file.
2. Translate each second line; leave the English keys untouched.
3. Fill in the `LANGUAGE_*` header for a new file.
4. Open a pull request, or attach the file to a GitHub issue.

When new strings land in `English.txt` they show up untranslated (as English) until a translator fills them in.
