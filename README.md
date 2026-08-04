# Longshot

Longshot takes a "full page" screenshot of one scrollable panel inside any
window — a browser tab's main article, a chat log, a code editor's file, a
PDF viewer — even when that panel is only one of *several* independently
scrollable regions on screen (say, a page with its own sidebar). You point
it at the spot you want, it scrolls that spot from top to bottom, capturing
frames along the way, then stitches them into one continuous tall image.

It works against real applications, not just browsers, and it doesn't need
any browser extension or app cooperation: it drives the mouse wheel and
reads the screen like a person would, so it works on whatever you can see.

Windows-only build for now (Linux/X11 and macOS backends are unimplemented;
`--backend x11` / `--backend macos` return a clean error rather than silently
doing nothing).

## How it works, briefly

Longshot doesn't read the target application's DOM or accessibility tree —
that would mean a different integration for every browser and every native
app. Instead:

1. **Find the region.** You hover the mouse over the scrollable area you
   want. Longshot sends a few scroll-wheel notches at that point and diffs
   the before/after screenshots to see exactly which rectangle of pixels
   actually moved — that's the region, regardless of what app it belongs to
   or what else is on screen.
2. **Scroll and capture.** It scrolls that region to the top, then walks it
   down to the bottom one step at a time, taking a screenshot at each step.
3. **Stitch.** Consecutive screenshots overlap (since each scroll step moves
   less than a full screen), so Longshot cross-correlates each pair to find
   exactly how many new pixel-rows appeared, and concatenates only the new
   part. Sticky headers/footers that don't scroll are detected and included
   exactly once, not once per frame.

The only things Longshot ever sends to the target application are scroll-wheel
notches and the four navigation keys (Page Up/Down, Home, End) — never a
click, never typed text — so it can't accidentally follow a link or submit a
form while it's driving the page.

## Building

Requires CMake >= 3.20 and a C++17 compiler (MSVC / VS2022 Build Tools on
Windows).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

## Usage

### Interactive (default)

```
longshot capture --out page.png
```

Hover the mouse over the scrollable region you want, press ENTER in the
terminal, confirm the detected region (a `<out>-probe.bmp` outline image is
written for reference), and the tool scrolls to the top, then to the bottom,
capturing and stitching as it goes.

### Non-interactive

```
longshot capture --region X,Y,W,H --window-id N --yes --out page.png
```

`--window-id` comes from `longshot list`. Skips the hover/confirm prompts and
the motion-diff probe entirely.

### Other commands

- `longshot list [--backend KIND] [--json]` — enumerate windows visible to a backend.
- `longshot stitch --frames DIR --out PATH` — stitch a directory of `frame_*.bmp`
  files (e.g. produced by `--keep-frames`) without touching the screen.
- `longshot doctor` — checks backend init, permissions, and does a live test
  capture; exit 0 only if a real capture would work.

### Capture flags

| Flag | Meaning |
|---|---|
| `--anchor X,Y` | Use this point instead of hovering interactively |
| `--region X,Y,W,H` | Skip detection entirely; capture exactly this rect |
| `--window-id N` | Window to activate/scroll (from `longshot list`) |
| `--inset L,T,R,B` | Shrink the (detected or explicit) region on each side |
| `--no-probe` | Skip the motion-diff probe (requires `--region`) |
| `--no-scroll-to-top` | Capture from the current scroll position, don't rewind |
| `--keep-frames DIR` | Save every captured `frame_NNN.bmp` for later `longshot stitch` |
| `--max-height N` | Cap the stitched output height (default 30000px) |
| `--yes` | Skip the confirmation prompt |
| `-v` / `-q` | Verbose / quiet logging |

## Permissions

Windows: none required for a normal desktop session. Input injection into an
**elevated** target window from a non-elevated Longshot process fails
silently (Windows UIPI) — `longshot doctor` warns about this when Longshot
itself isn't running elevated.

## Known limitations

- One region per run — to capture a sidebar and main content separately, run
  the tool twice.
- Vertical scrolling only.
- The target window must be on-screen and unobstructed; occluded, minimized,
  or off-virtual-desktop windows are not captured (composited-desktop capture
  only, no per-window capture API).
- Continuously animating content (video, carousels, live tickers) will not
  settle cleanly; the tool warns and continues with a possibly seamed result.
- Never clicks or types into the target window (see "How it works" above).
- Linux (X11) and macOS backends are not yet implemented.
