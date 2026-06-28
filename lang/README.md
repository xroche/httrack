# Translating HTTrack

Interface strings live here, one `.txt` file per language. `English.txt` is the reference: every other file maps each English string to its translation.

## File format

Plain text, entries in consecutive pairs of lines:

```
<English string>
<translation>
```

The first line of a pair is the lookup key and must stay identical to the one in `English.txt`; translate only the second line. Missing entries fall back to the English text at runtime, so a partial translation works.

Preserve any `\r\n`, `\t` and `printf` placeholders (`%s`, `%d`, ...) in the translation.

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
