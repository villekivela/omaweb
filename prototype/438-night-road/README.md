# PROTOTYPE: the Start page's night road (#438)

Throwaway. Not built, not tested, not shipped. It answers how the road looks and moves before the
Start page is built.

```sh
./run.sh
```

`run.sh` reads every installed Omarchy theme's `colors.toml` into `themes.js` and opens
`NightRoad.qml` in Qt's `qml` tool. The HUD in the corner lists the keys and the state.
