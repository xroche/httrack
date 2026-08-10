# Documentation screenshots

`screenshot-walk.py` drives the WebHTTrack UI in headless Chromium and captures one
PNG per documentation screen: the welcome, project and URL panes, the add-URL popup,
all eleven option tabs, the ready-to-start pane, and a mirror in progress and
finished. The crawl is real — the script serves a small site to itself, so the
progress screen carries live counters rather than zeros.

## Replay

Run the **screenshots** workflow (Actions → Run workflow) and download the
`screenshots` artifact. `lang` renders the UI in another locale (the number is the
one in `lang.indexes`), `scale` sets the device pixel ratio.

Locally, against any built tree:

```
pip install playwright && playwright install --with-deps chromium
python3 tools/screenshot-walk.py --htsserver src/htsserver --root . --out shots
```

`--root` is the dist root — the directory holding `lang.def`, `lang.indexes`,
`lang/` and `html/`. A source tree qualifies, so nothing has to be installed first.

## Adding a screen

Nothing to edit for an option page: the tabs are enumerated from the tab bar and
named after their caption, so a new one appears in the set on the next run. A new
wizard pane needs a line in `run()`, anchored on a control that pane's form carries.

Anchor on structure — a control name, a form action — rather than on a caption, or
the walk only works in English.

## Traps

- htsserver block-buffers stdout when it is a pipe, so its `URL=` line would never
  arrive; the walk reads it from a file, as `webhttrack` does.
- Passing `--port 0` is an error, not the auto-pick it used to be (#614). The walk
  picks a free port itself.
- The progress and the finished screen are both served as `refresh.html`. The URL
  cannot tell them apart; the form each carries can.
- The first render of the progress screen has an empty stats table, so the walk
  waits for counters before shooting.
- An existing mirror at the base path finishes from cache in no time, which turns
  the progress shot into a second copy of the finished screen. The walk runs under
  a throwaway `$HOME`, which also keeps the host's own projects out of the shots.
- Shoot on CI, not on a workstation: the pages ask for Trebuchet/Verdana, no runner
  or box has either, and the fallback's metrics decide where every label wraps.
