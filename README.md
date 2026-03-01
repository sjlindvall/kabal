# kabal

A Petzold-style Win32 solitaire implementation in plain C.

## Features
- Klondike-style board (stock, waste, 7 tableau columns, 4 foundations).
- Card faces are loaded as **ICON resources** from `solitaire.rc`.
- Difficulty modes:
  - **Easy**: draw 1 card, unlimited stock recycling.
  - **Hard**: draw 3 cards, one stock recycle.
- Save/load current game state from `solitaire_save.dat`.

## Why no binary icons in git?
If your code-host UI says **"Binary files are not supported"**, this repo avoids that by generating icon binaries from source.

- Source of truth: `tools/gen_icons.py` (text, reviewable in PRs)
- Generated output: `icons/*.ico` (ignored by git)

This keeps PR diffs readable while still using ICON resources in the Win32 build.

## Build (MinGW)
```bash
python tools/gen_icons.py --out icons
windres solitaire.rc -O coff -o solitaire_res.o
gcc -std=c11 -Wall -Wextra -municode -mwindows main.c solitaire_res.o -o kabal-solitaire.exe -lcomctl32
```

## Play controls
- Click stock to draw.
- Click waste top card to select it, then click tableau/foundation to place.
- Click top face-down tableau card to flip it.
- Click top face-up tableau card to select, then click a destination tableau/foundation.
- Use the **Game** menu for New, Save State, Load State.
- Use the **Difficulty** menu to switch Easy/Hard.
