# Prototype: where the sidebar footer's marks go (#838)

Throwaway. This branch is never merged. The question: where should the short-lived marks (problem
recording, downloads, the release notice, the extension mark) and Settings go, so the footer's Space
row keeps its room at 220 px?

The variants are drawn in the real sidebar (`src/ui/SpaceOutline.qml`, blocks marked
`PROTOTYPE #838`) and chosen with two UI lab flags. The marks are stubs, so the lab needs no
downloads and no hosted extensions.

- `--footer-variant a`: #838 as written. Settings sits in the top row after collapse and search. A
  status pill sits above the Space row, labelled for one item and icons only for several. The
  extension mark sits at the address box's right end. The Space row is centred.
- `--footer-variant b`: the marks become rows docked at the end of the tab list under a rule, each
  labelled the way a tab is, the extension included. Settings stays at the footer's right end, and
  the Spaces start on the left.
- `--footer-variant c`: every mark, the extension's too, sits as an icon tray at the address box's
  right end, and the address text gives way. Settings sits in the top row. The Space row is centred.
- `--footer-variant a-centred`: A with the pill's items centred. Added after the maintainer picked
  A.
- `--footer-variant d`: A with a capsule in place of the pill, after iOS's Dynamic Island. It is
  centred above the Space row and hugs its items, and its width eases as they come and go (stepped
  under reduced motion). Pressing an item opens the capsule in place into a stub detail; the same
  item again, or Escape, closes it. `--footer-expand downloads` opens it on downloads.
- `--footer-items all|one|none`: every stub mark, downloads alone, or none. A Private window drops
  the recording, the release notice and the extension, as main does.

## Commands

Build the lab with `cmake --preset ui` and `cmake --build build/ui --target omaweb-ui-lab`, then:

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 build/ui/omaweb-ui-lab \
  --data-root "$(mktemp -d)" --tabs --spaces --many-spaces \
  --footer-variant a --footer-items all [--narrow] --capture out.png
```

`--footer-demo <dir>` steps variant d through none, one, all, one, open on downloads, closed and
none, and saves the sidebar every 40 ms as `frame-NNN.png` in that directory. Run it without
`--capture` and with `--footer-items none`; `d-demo-narrow.gif` is that run at 220 px, cropped to
the footer.

Add `--agents` for more Spaces than the row holds, and `--private` for a Private window. Leave out
`--capture` to drive the lab by hand. The `main-*` captures leave out `--footer-variant` and add
`--problem-recording`, so the footer is main's, with its real recording and release marks.

## Captures

`<variant>-<items>-<width>.png`, where `narrow` is 220 px and `default` is 292 px. At 220 px:

| Variant | Every mark, Spaces overflowing            | Private window                |
| ------- | ----------------------------------------- | ----------------------------- |
| main    | `main-all-narrow-overflow.png`: 2 + `+10` | `main-all-narrow-private.png` |
| a       | `a-all-narrow-overflow.png`: 7 + `+5`     | `a-all-narrow-private.png`    |
| b       | `b-all-narrow-overflow.png`: 6 + `+7`     | `b-all-narrow-private.png`    |
| c       | `c-all-narrow-overflow.png`: 7 + `+5`     | `c-all-narrow-private.png`    |

Variant d opened on downloads: `d-all-narrow-expanded.png` and `d-all-default-expanded.png`.
