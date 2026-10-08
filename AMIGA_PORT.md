# Amiga-Port — Plan und Stand

## Stand (26.09.2026)

Der komplette Prolog läuft auf einem emulierten **A500 mit Kickstart 1.3 und 1 MB**: Intro, Titel,
Charaktererstellung, alle sechs Räume mit Szenen, Kämpfen und Countdown, Automap, Ende, neues
Spiel. Geprüft mit `tools/scenario.py` (Routen aus dnds `emutest.py`), der Renderer zusätzlich
pixelgenau gegen `make_view.py` (`tools/viewcheck.py`), die Musik per Frequenzanalyse
(`tools/audiocheck.py`).

**Gemessen:**

| | Plan | Ist |
|---|---|---|
| Diskette | ~220 KB von 880 KB | **~210 KB**: Exe 104 KB + `dnd.pak` 101 KB (ZX0; ungepackt 292 KB) |
| Laden von Diskette | 15–20 s | Titel ~30 s nach dem Einschalten, inkl. Kickstart-Boot |
| Schritt im Dungeon | ≤ 12 Frames | 9 Frames bei freiem Blick, 15–17 mit Wand direkt davor |
| Erster Blick in einen Raum | 1–2 s | 0,5–0,7 s (nur Sichtbares wird vorberechnet) |
| Freier Speicher nach dem Laden | – | 299 KB Chip, 368 KB insgesamt; der Cache nutzte im ganzen Prolog ~210 KB |

**Abweichungen vom Plan, und warum:**

- **Datenpaket statt Chip-Hunks von Anfang an:** Bebbos Linker setzt das Chip-Flag an Hunks nicht
  zuverlässig. Alles, was Blitter oder Paula brauchen, kommt deshalb aus `dnd.pak` in per
  `AllocMem(MEMF_CHIP)` angeforderten Speicher. Die Exe enthält nur Code.
- **Vorberechnung bei Bedarf statt beim Raumladen:** Eine Wandspalte wird berechnet, wenn ein
  Strahl sie zum ersten Mal zeigt, ein Prop pro Entfernung, wenn er dort zum ersten Mal zu sehen
  ist. Alles auf einmal kostete Sekunden pro Raum.
- **Chunky→Planar nur im Zeilenband mit Wänden und Props:** Decke und Boden liegen als fertige
  Bitplanes vor und kommen per Blitter, parallel zur CPU.
- **Musik mit eigenem Player statt ptplayer/MOD:** Die Songs sind reine PSG-Noten. Paula spielt sie
  direkt: drei Rechteck-Schleifen und eine Rausch-Schleife, Periode und Lautstärke pro 60-Hz-Tick
  (`src/amiga/audio.c`). Soundeffekte laufen auf dem Rauschkanal, das Schlagzeug pausiert dabei.
- **Soundeffekte mit 8,4 kHz** statt 13,3 kHz: 45 KB weniger Chip-RAM.
- **Titelbild, Logo und Porträts werden nach Gebrauch freigegeben** (`res_free_*`, ~75 KB).
- **Paletten:** Farben 0–15 für View, Bilder und Text, 16–31 für Figuren, Büsten und
  Porträts (exakt wie auf dem MD). Die Panel-Avatare (Gefährten und Held) sind auf die
  View-Palette umgerechnet, weil der Amiga Farben nicht pro Bildschirmbereich wechseln kann.
- **„Neues Spiel“ startet den Rechner neu**, und die Diskette bootet das Spiel frisch. Unter
  Kickstart 1.3 gibt es ohne `C:`-Befehle keinen Neustart per Skript.
- **Bildschirm-Schütteln** betrifft die ganzen Zeilen (BPLCON1), also auch das Panel.
- **Taktung:** `SYS_doVBlankProcess` lässt die Spiellogik im Mittel mit 60 Hz laufen, auch die
  Musik und der VBlank-Callback. Alle Frame-Zählungen aus dem MD-Code gelten unverändert.
- **Eingabe:** Tastendrücke und Joystick werden in Interrupts zwischengespeichert, damit auch
  ein Druck während einer längeren Berechnung ankommt.

**Offen:**

- Test auf echter Hardware.
- Texte im Spiel nennen noch die Mega-Drive-Knöpfe („A: BENUTZEN“, „START DRÜCKEN“); die
  Tasten A/B/C und Return funktionieren so, aber Joystick-Spieler lesen „Feuer“ nirgends.
- Die Exe könnte zusätzlich gepackt werden (~50 KB weniger), nötig ist das nicht.
- Optional: Web-Version (vAmigaWeb mit AROS) wie dnds GitHub-Pages-Player.

---

## Kontext

Das Mega-Drive-Spiel `dnd` (Nautiloid-Prolog, 6 Räume) soll vollständig auf den Amiga portiert werden.
Die Toolchain steht bereits in `/Users/joerg/dev/dnd-amiga` (Bebbo-gcc im Docker, FS-UAE, headless
`tools/emutest.sh`, bootfähige ADF). Vorgaben: **Amiga 500 mit 1 MB** (512 KB Chip + 512 KB Erweiterung),
**eine DD-Diskette (880 KB)**, Code wird **einmalig aus dnd kopiert** und dann eigenständig weiterentwickelt.

## Wo der Speicher hingeht – und was davon übrig bleibt

Das MD-ROM belegt 1,28 MB (gemessen über `out/symbol.txt` und `res/`):

| Block | Mega Drive | Amiga (roh) | Amiga (ZX0-gepackt) | Wie gespart |
|---|---|---|---|---|
| Wandspalten `columns.bin` | 394 KB | 14 KB | ~8 KB | Nur die 7 Quelltexturen (64×64, je 2 KB) auf Disk; die Spalten werden beim Raumladen **auf dem Amiga** vorberechnet |
| Props `props.bin` | 344 KB | 86 KB | ~30 KB | Nur die 42 Quellbilder (64×64); Skalierung pro Entfernung beim Raumladen, nur für Props des Raums |
| Wand direkt vor dir `adjacent.bin` | 125 KB | 0 | 0 | Entfällt, wird normal gerendert (optional: beim Laden als Planar-Bild cachen) |
| Boden/Decke `backdrops.bin` | 36 KB | 18 KB | ~6 KB | Spiegelbild wird zur Laufzeit erzeugt |
| Spalten-Tabellen (`viewEvents` + Sampling-Daten) | 5 KB | ~10 KB | ~5 KB | Neu: u/v-Samplingtabellen statt fertiger Pixel |
| Soundeffekte | ~140 KB | ~65 KB | ~60 KB | Paula spielt 8-Bit-Samples nativ; Jingles `sfx_gameover`/`sfx_intro` (40 KB) werden Musik; Rauschlastige auf ~9 kHz |
| Musik (XGM2 + Treiber) | ~45 KB | ~30 KB | ~8 KB | Songs sind reine PSG-Noten → MOD mit winzigen Rechteck-/Rausch-Samples |
| Figuren, Porträts, Busts, Font, Logo | ~55 KB | ~55 KB | ~35 KB | Planar-Format, gleiche Bitmenge |
| Titelbild | 22 KB | 35 KB | ~18 KB | Planar ohne Tile-Deduplizierung, dafür gepackt |
| Code + Texte (15 KB Dialog) | ~150 KB inkl. SGDK | ~100 KB | ~50 KB | Kein SGDK/XGM-Treiber mehr, eigene schlanke Hardware-Schicht |
| **Summe** | **~1,28 MB** | **~415 KB** | **~220 KB** | |

**Ergebnis:** Das Spiel passt sogar ungepackt auf die Diskette. Gepackt belegt es ~25 % der 880 KB,
der Rest bleibt Reserve für Inhalte nach dem Prolog. Der große Hebel ist ein einziger: die 900 KB
vorberechneten View-Daten entstehen aus 100 KB Quelldaten und lassen sich auf dem Amiga beim
Laden erzeugen.

**Der eigentliche Engpass ist das RAM, nicht die Diskette.** Rechnung für den teuersten Raum (Raum 3/6):

| | Chip-RAM | Slow-RAM |
|---|---|---|
| 2 Bildschirmpuffer 320×224×5 Planes | 90 KB | |
| Soundeffekte + Musik-Samples | ~70 KB | |
| Blitter-Grafiken (Figuren, Busts, Avatare, Font) | ~60 KB | |
| Code + Texte | | ~100 KB |
| Quelldaten View (Texturen, Props, Backdrop) | | ~120 KB |
| Vorberechnete Wände: Wand + Tür (je 56 KB) | | 112 KB |
| Vorberechnete Props des Raums (~12 × 8 KB) | | ~100 KB |
| Chunky-Puffer + C2P-Tabellen | | ~35 KB |
| **Summe** | **~220 KB** | **~470 KB** |

Dazu kommen ~60–80 KB für das Betriebssystem, das zum Laden am Leben bleibt. Das passt in 1 MB,
aber knapp im Slow-RAM (was nicht passt, landet automatisch im Chip-RAM). Deshalb:
- **Animierte Wand (Hüllenbruch, 4 Frames)** wird *nicht* vorberechnet, sonst wären es 4 × 56 KB mehr.
  Sie läuft über einen langsameren Pfad, der die Textur zur Laufzeit skaliert. Er wird nur gebraucht,
  wenn diese Wand im Bild ist.
- Props werden **pro Raum und nur bei Bedarf** vorberechnet, z. B. der zerbrochene Tank erst, wenn er zerbricht.
- Beim Raumwechsel wird der Speicher des alten Raums freigegeben.

## Architektur

**Bildschirm:** PAL-Lowres, Layout 1:1 wie Mega Drive (320×224, vertikal zentriert in 256 Zeilen),
**5 Bitplanes = 32 Farben**, doppelt gepuffert, Copper-Liste pro Puffer.
- Farben 0–15: View-Palette aus `make_view.py` (MD 3 Bit/Kanal → Amiga 4 Bit, verlustfrei).
- Farben 16–31: **gemeinsame Figuren-/UI-Palette** für Text, Panel, Avatare, Figuren und Busts.
  Anders als beim MD mit 4 Paletten, weil der Amiga Farben pro Zeile, aber nicht pro Bildschirmbereich
  wechseln kann. Das Konvertierungstool quantisiert alle Charaktergrafiken auf diese 16 Farben.
  Vollbilder (Titel, Charaktererstellung, Ende) bekommen eigene Paletten.
- View-Animationen über Farbwechsel bleiben möglich (Feuer im Kampf nutzt `PAL_setColors` → Farbregister).

**View-Renderer:** gleiches Prinzip wie auf dem MD, zwei Stufen:
1. **Chunky-Puffer** (112 Byte pro Zeile × 160 Zeilen, 1 Byte = 2 Pixel wie beim MD): Backdrop kopieren,
   Wände und Props hineinzeichnen. `view_draw.s` (`viewCopySpan`/`viewMaskSpan`) läuft praktisch
   unverändert, da ebenfalls 68000-Code in GNU-as-Syntax. Nur die Adressierung ändert sich: Zeilenabstand
   112 statt MD-Tile-Layout (einfacher als vorher).
2. **Chunky→Planar (C2P)** in Assembler: 224×160 Pixel in 4 Bitplanes des Hintergrundpuffers, dann
   Pufferwechsel im Vertical Blank. Das Vorberechnen kann die Bytes gleich in C2P-freundlicher
   Bitreihenfolge ablegen, das spart Arbeit im C2P.
   Zeitziel: **≤ 12 Frames (0,25 s) pro Schritt**, der MD braucht 3–5. Eye of the Beholder auf dem A500
   war ähnlich schnell. Falls es zu langsam ist, übernimmt der Blitter einen Teil des C2P parallel zur CPU.

**Vorberechnung auf dem Amiga (Kern der Einsparung):** `tools/make_view_amiga.py` (aus `make_view.py`)
exportiert statt fertiger Pixel nur Texturen, Prop-Quellbilder, Backdrop sowie Sampling-Tabellen:
pro View-Event die Werte `u0, u1, band, vstart, vstep` (liegen in `column_events()` schon als
Ganzzahlen vor), pro Prop-Entfernung die u/v-Indizes. Damit ist das Vorberechnen auf dem Amiga reine
Tabellenarbeit ohne Fließkomma, ein C-Port von `bake()`/`bake_prop()`.
Geschätzt ~0,15 s pro Textur und ~0,1 s pro Prop, also 1–2 s pro Raumwechsel.

**Figuren und Busts:** Blitter-Objekte (Cookie-Cut mit Maske) auf Plane 0–4 über dem View,
mit Hintergrundsicherung für Verstecken/Blinken und für den Flügelschlag der Imps.
Die `figures_*`-API (`src/figures.c`) bleibt gleich, nur das Innenleben ist neu.
Ein Unterschied bleibt: Beim „View-Schütteln“ (`figures_shakeView`) wackeln die Panel-Zeilen mit,
weil BPLCON1 pro Zeile gilt. Alternativ schüttelt der View vertikal.

**Panel-Avatare:** ebenfalls Blits (24×24) in Farben 16–31.

**Text:** `font_de.png` als 1-Bit-Font. `text_draw` behält die Umlaut-Abbildung aus `src/text.c` und
schreibt 8×8-Zeichen direkt in die Planes. `textbox.c`/`ui_panel.c` behalten ihre Logik.

**Sound:** **ptplayer** von Frank Wille (Public Domain, 68000-Asm, vasm-Syntax, Musik + SFX auf Paula).
- Musik: `tools/make_mod.py` übernimmt die Songdefinitionen aus `generate_music.py` (`song_title()` usw.)
  und schreibt ProTracker-MODs mit PSG-artigen Samples (Rechteck in 2–3 Pulsbreiten, Rauschen).
  Tempo auf 50 Hz umgerechnet.
- SFX: `generate_sfx.py` liefert schon 8-Bit-Mono mit 13,3 kHz, das Paula direkt spielen kann.
  Konvertiert wird nur auf signed, gerade Länge, rauschlastige auf ~9 kHz.

**Eingabe:** MD-Knöpfe → Amiga

| MD | Joystick (Port 2) | Tastatur |
|---|---|---|
| Steuerkreuz | Richtungen | Cursortasten |
| A (Aktion) | Feuer | Return / Space |
| B (Party-Menü) | 2. Feuerknopf (Pin 9, 2-Knopf-/CD32-/Sega-Pad) | Esc / Backspace |
| C (Karte) | – | M / Tab |
| Start | – | P / F1 |

Wie beim MD per Latch (`src/input.c`), damit kurze Drücke während eines Redraws nicht verloren gehen.
Tastatur über CIA-A-Interrupt.

**Laden:** AmigaDOS-Diskette (OFS), wie jetzt schon aufgesetzt. Alle Daten liegen ZX0-gepackt in einer
Datei `data.pak` und werden **einmal beim Start geladen**, während das OS noch läuft und das Intro/RCD-Logo
zu sehen ist. Das dauert ca. 15–20 s von Diskette. Danach übernimmt das Spiel die Hardware; im Spiel gibt
es kein Nachladen, kein Diskgeräusch und keinen Diskwechsel. Einen eigenen Trackloader brauchen wir nicht,
weil der Platz reicht. Als Nebeneffekt bleibt das Spiel auf Festplatte kopierbar.
Entpacker: `unzx0_68000.s` (Emmanuel Marty), Packer: `salvador` (C, wird im Docker-Image gebaut).

**Takt:** MD 60 Hz → Amiga PAL 50 Hz. Frame-Zählungen (`figures_wait(4)`, Animation alle 8 Frames)
werden über ein zentrales Makro umgerechnet, damit das Timing gleich bleibt.

## Umsetzung in Meilensteinen

Jeder Meilenstein endet mit einem Screenshot-Nachweis über `tools/emutest.sh`.

### M0 – Code übernehmen
- `dnd/src`, `dnd/tools`, `dnd/res` (Quellen, ohne `out/`) nach `dnd-amiga` kopieren und die
  dnd-Commit-ID in `AMIGA_PORT.md` festhalten (Abschnitt unten) (Basis für späteres manuelles Nachziehen).
- Aufteilung: `src/` enthält die Spiellogik (kopiert), `src/amiga/` die neue Hardware-Schicht.
- Die Dateien ohne Hardware-Aufrufe bleiben unverändert: `room1–6.c`, `combat.c` (bis auf 19 Stellen),
  `dungeon_map.c`, `dungeon_objects.c`, `encounter.c`, `party.c`, `inventory.c`, `equipment.c`,
  `abilities.c`, `skill_check.c`, `countdown.c`, `dice.c`, `automap.c` (Logik).
- Die bisherige Testdatei `src/main.c` in `src/amiga/hw.c` überführen (Takeover/Restore).

### M1 – Hardware-Schicht und „SGDK-Ersatz“
- `src/amiga/genesis.h`: Typen (`u8`…`s32`, `bool`, `TRUE`), `BUTTON_*`, `JOY_readJoypad`,
  `SYS_doVBlankProcess` (auf VBlank warten, Eingabe-Latch, Audio-Tick), `random`,
  `sprintf`/`memcpy`/`memset` (libnix). Ziel: Die Logikdateien kompilieren ohne Änderung.
- Takeover/Restore, VBlank-Interrupt, Copper-Listen, Doppelpuffer, Tastatur- und Joystick-Treiber.
- Speicherverwaltung mit Chip/Slow-Aufteilung und Budget-Ausgabe über die serielle Schnittstelle
  (FS-UAE schreibt sie in eine Datei, dient als Debug-Log).
- **Nachweis:** Das Spiel startet bis in `main()` und die Logs zeigen, dass die Eingabe ankommt.

### M2 – Text, Textbox, Panel
- Font-Konverter, `text.c`/`textbox.c`/`ui_panel.c` auf Planar.
- `tools/make_planar.py`: PNG → Planar + Maske, Quantisierung auf die gemeinsame Palette 16–31.
- **Nachweis:** Charaktererstellung mit Klassenwahl und Porträt (`char_create.c`).

### M3 – View-Renderer (größter Brocken)
- `tools/make_view_amiga.py` exportiert Quelldaten und Sampling-Tabellen (Import von `make_view.py`
  als Modul, damit Texturen und Props nicht doppelt gepflegt werden).
- `src/amiga/view_bake.c`: Wände und Props beim Raumladen vorberechnen.
- `src/amiga/view_draw.s`: aus dem MD übernommen, Adressierung angepasst.
- `src/amiga/c2p.s`: Chunky→Planar.
- `dungeon_view.c`: Tile-Puffer und DMA durch Chunky, C2P und Pufferwechsel ersetzen, sonst gleiche Logik.
- Laufzeit-Skalierungspfad für den animierten Hüllenbruch.
- **Nachweis:** Screenshots von Raum 1 aus denselben Blickwinkeln wie dnds `emutest.py look/tour`,
  Vergleich mit `dnd/out/emutest`. Frames pro Redraw im seriellen Log (Ziel ≤ 12).

### M4 – Figuren, Kampf, Animationen
- Blitter-Objekte für `figures.c`, Imp-Flügelschlag, Blinken, Pfeil, Zhalk-Frames.
- Farbzyklus für das Feuer in `combat.c`.
- **Nachweis:** Kampf in Raum 3 (Lae'zel, Imps).

### M5 – Sound
- ptplayer einbinden, `make_mod.py` für die 5 Songs, SFX-Konvertierung, `sfx.c` auf ptplayer umstellen.
- **Nachweis:** Audio-Aufnahme über FS-UAE (`--audio_output_file` o. ä.), Musik + SFX gleichzeitig.

### M6 – Übrige Bildschirme und Datenpaket
- Intro (RCD-Logo), Titel, Automap, Ende.
- `data.pak` mit ZX0, Ladebildschirm beim Start.
- Die Größenprüfung in `build.sh` wechselt von „Exe-Größe“ auf „ADF-Belegung“ (über `xdftool info`)
  und bricht über 880 KB ab. Zusätzlich gibt der Build ein RAM-Budget pro Raum aus.
- **Nachweis:** Kompletter Durchlauf aller 6 Räume als emutest-Szenarien (Pendant zu `room1`…`room45`).

### M7 – Härtung
- Test mit echtem **Kickstart 1.3** (`roms/kick13.rom`), cycle-exact A500 mit 512K + 512K.
- Sauberer Abbruch mit Meldung bei nur 512 KB.
- Optional: Web-Version (vAmigaWeb mit AROS) analog zu dnds GitHub-Pages-Player.

## Kritische Dateien

- Neu: `src/amiga/{genesis.h,hw.c,input.c,screen.c,blit.c,view_bake.c,c2p.s,audio.c,ptplayer.asm,unzx0.s}`,
  `tools/{make_view_amiga.py,make_planar.py,make_mod.py,make_pak.py}`.
- Angepasst (aus dnd kopiert): `dungeon_view.c`, `view_draw.s`, `text.c`, `textbox.c`, `ui_panel.c`,
  `figures.c`, `sfx.c`, `intro.c`, `title.c`, `ending.c`, `char_create.c`, `main.c`, `input.c`,
  `combat.c` (nur Sprite-/Paletten-Stellen).
- Wiederverwendet: `make_view.py` (`TEXTURES`, `PROPS`, `column_events`, `backdrop`, `SHADE`),
  `generate_music.py` (Songdefinitionen), `generate_sfx.py`, `make_figures.py`/`make_portraits.py`
  (PNG-Ausgabe), `text.c` (Umlaut-Mapping).
- Build: `Makefile` (GNU-as für `.s` aus dnd, vasm für ptplayer), `Dockerfile` (+ salvador), `build.sh`.

## Verifikation

- `./build.sh` baut die ADF und meldet Diskbelegung (Ziel < 300 KB von 880 KB) und RAM-Budget.
- `tools/emutest.sh` bekommt Tastatureingabe per xdotool und benannte Szenarien (`create`, `look`,
  `room1` … `room6`), Screenshots in `out/emutest/`, Vergleich mit den MD-Screenshots.
- Serielles Log mit Frames pro Redraw, freiem Chip- und Slow-RAM pro Raum.
- Abschlusstest mit Kickstart 1.3 und einem vollständigen Durchspielen in `./run.sh`.

## Übernommen aus dnd

Einmalige Kopie (keine automatische Synchronisation) von
[joergsf4/dnd](https://github.com/joergsf4/dnd) Commit **`3da59e7`** (2026-09-26,
„More BG3 dialogue options across the prologue“): `src/`, `tools/`, `res/`,
`BeschreibungInhaltVerticalSlice.md`, per `git archive`, also nur versionierte Dateien.

Nicht übernommen, weil nur für den Mega Drive: `src/boot/` (ROM-Header), `res/view/*.bin` (die
vorberechneten 900 KB, siehe oben), `res/music/*.vgm` (XGM2), `res/resources.res`/`.h` (rescomp),
`tools/emutest.py` (BlastEm), `tools/make_web.sh`, `.github/`.

Spätere Änderungen in dnd nachziehen: `git -C ../dnd diff 3da59e7..HEAD -- src tools res` und die
Commit-ID hier aktualisieren.
