# emoji pop

A fast Linux emoji picker. Bind a hotkey to run `emoji_pop` — it stays in the background and pops up when you need it.

## Build

```bash
cmake -B build
cmake --build build
```

Requires OpenGL, FreeType, and HarfBuzz.

## Usage

```bash
./build/emoji_pop
```

Running it again while the picker is already open just focuses the window. Press **Escape** to hide it.

Search by name or keyword (e.g. `shit`, `thumbs up`). Arrow keys and Enter work for keyboard navigation.

Settings are saved to `~/.config/emoji_pop/settings` (skin tone and recents).

## Hotkey

Map your window manager or desktop environment to run `emoji_pop`. The command returns instantly; the picker runs as a background process.
