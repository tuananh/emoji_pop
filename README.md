# emoji pop

A Linux emoji picker that opens instantly.

<p align="center">
  <img src="assets/main_window.png" alt="Emoji Pop main window" />
</p>

## Build

```bash
cmake -B build
cmake --build build
```

Requires OpenGL, FreeType, and HarfBuzz.

## Usage flow

```bash
$ emoji_pop
```

1. **Set up and start** — bind a shortcut in your window manager or desktop environment first (**Super/Mod + .** is recommended to match macOS and Windows), then run `emoji_pop` — or add it to your session startup so it launches automatically. The command returns immediately; the picker stays in the background.
2. **Open** — press your shortcut to show or focus the picker. Press **Escape** to hide it without selecting.
3. **Search** — type a name or keyword (e.g. `shit`, `thumbs up`). Arrow keys and Enter work for keyboard navigation.
4. **Select** — click or press Enter. The emoji is copied to the clipboard and the window dismisses.

### i3

Add to `~/.config/i3/config`:

```
bindsym $mod+period exec --no-startup-id emoji_pop
for_window [class="Emoji Pop"] floating enable, move position center, resize set 640 480
```
