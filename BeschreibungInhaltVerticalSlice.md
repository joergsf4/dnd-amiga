# Gamedesign-Dokument: Nautiloid-Prolog (16-Bit Demake)
**Zielplattform-Stil:** SNES / Mega Drive (Top-Down / 3/4-Isometrie wie *Chrono Trigger* oder *Shadowrun*)  
**Level-Thema:** Der Nautiloid (Tentakelschiff der Gedankenschinder) stürzt brennend durch die Höllenebene Avernus ab.

---

## 1. Übersicht & Raum-Layout (Map-Flow)

Die Navigation erfolgt linear von Raum zu Raum über organische Sphinkter-Türen und fleischige Kletterwände (Arterial Meshes).

```text
[Raum 1: Klonkammer]
         │ (Sphinkter-Tür)
         ▼
[Raum 2: Operationssaal (Myrnath & "Wir")]
         │ (Kletterpassage / Rampe)
         ▼
[Raum 3: Oberdeck (Kampf mit Lae'zel)]
         │ (Sphinkter-Tür)
         ▼
[Raum 4: Kapselsaal (Schattenherz)] ── (Abzweig Ost) ──► [Raum 5: Nebenraum / Labor]
         │ (Sphinkter-Tür Nord)                                (Schlüssel & Rune)
         ▼
[Raum 6: Die Brücke / Steuerkonsole (Bosskampf)]
         │
    [ZIEL: Transponder-Konsole] ──► Flucht / Intro-Abschluss
```

---

## 2. Raum-für-Raum Detaillierung

### Raum 1: Klonkammer (Startpunkt / Tutorial)
* **Visuals:** Bio-mechanische Wände, feuchte Böden, zerbrochene Klonkapseln, Schleimbecken.
* **Layout:** Kleiner Raum. Spieler spawnt aus einer offenen Glaskapsel in der Mitte.
* **Interaktionen & Objekte:**
  * **Larvenbecken (Nursery):** Untersuchen (`A`-Taste). Bei falscher Interaktion kleine Gift-/Säure-Explosion (-2 HP Schaden als Tutorial für Gefahren).
  * **Leiche eines Gedankenschinders:** Loot (Edelstein, etwas Gold).
  * **Knorpelkiste (Cartilaginous Chest):** Enthält Basisausrüstung / Heiltrank.
  * **Restaurierungs-Station (Heilblase):** Regeneriert KP und Zauberplätze voll (wiederverwendbar).
* **Ausgang:** Sphinkter-Tür nach Westen/Nordwesten.

---

### Raum 2: Operationssaal (Der Intellektverschlinger)
* **Visuals:** Erhöhte Plattformen, Treppen/Bio-Aufzug, Vivisektions-Tische.
* **Layout:** Zweistöckig; Aufzug oder Treppe führt zu einem Operationstisch.
* **Interaktionen & Ereignisse:**
  * **NPC Myrnath & "Wir" (Us):**
    * Auf dem Tisch liegt ein Elf mit geöffneter Schädeldecke. Das Gehirn spricht telepathisch zum Spieler.
    * **Entscheidungs-Menü:**
      * *Befreien (Attributs-Check: Stärke oder Medizin):* "Wir" springt heraus und schließt sich als temporärer Begleiter an (Nahkampf-Einheit).
      * *Gehirn verstümmeln (Geschicklichkeits-Check):* Schwächt "Wir" ab (Lobotomie), bleibt aber gehorsam.
      * *Zerstören:* Gehirn stirbt, kein Begleiter.
  * **Schreibtisch/Tafeln:** Lore-Texte über die Illithiden.
* **Ausgang:** Gang führt nach draußen auf das zerstörte Außendeck.

---

### Raum 3: Zerstörtes Außendeck (Erster Kampf)
* **Visuals:** Riss in der Schiffswand mit Blick auf feurigen Höllenhimmel (Parallax-Scrolling im Hintergrund: brennende Felsen, Drachen im Luftkampf).
* **Layout:** Zerstörter Korridor mit Feuerstellen auf dem Boden.
* **Ereignis (Skript-Zwischensequenz):**
  * **Lae'zel** springt von oben herab, zieht ihr Schwert, erkennt die Kaulquappe im Kopf des Spielers und schließt sich der Party an.
* **Gegner:**
  * 3x **Niederer Kobold (Imp)**.
* **Kampfmechanik (16-Bit Runden- oder ATB-System):**
  * Erklärung von Standardangriff, Zauber und Positions-Vorteil (z. B. lila explosive Nautiloid-Tanks können mit Fernangriffen zur Explosion gebracht werden).
* **Ausgang:** Fleischleiter/Rankenwand nach oben zur nächsten Schleuse.

---

### Raum 4: Kapselsaal (Gefangene Schattenherz)
* **Visuals:** Große Halle mit versiegelten Kapseln; pulsierende Konsolen.
* **Layout:** Zentraler Raum mit einem Pod links, einer Haupttür nach Norden und einem Durchgang nach Osten.
* **Ereignis (Quest: Schattenherz befreien):**
  * In einer Kapsel hämmert **Schattenherz** gegen die Scheibe und bittet um Hilfe.
  * **Pod-Konsole:** Benötigt ein Item (*Eldritch Rune*) oder klassenspezifische Interaktion (z. B. Magier/Hexenmeister Arkana-Wurf, Barbar Stärke-Gewalt).
* **Konsolen-Pult (3 Tasten) im Raum:**
  * Taste 1: Keine Wirkung.
  * Taste 2: Lässt feindliche Kapsel-Insassen frei (Zusatzkampf).
  * Taste 3: Tötet Insassen direkt.
* **Ausgänge:**
  * **Ost-Durchgang:** Führt zu Raum 5 (für den Schlüssel/Rune).
  * **Nord-Tor:** Führt direkt zur Brücke (verschlossen oder offen, sobald bereit).

---

### Raum 5: Transformations-Labor (Optionaler Erkundungsraum)
* **Visuals:** Unheimlicher Brutsaal, tote Klerikerin am Boden, Pod mit einer Frau.
* **Layout:** Kleines Nebenzimmer.
* **Interaktionen & Beute:**
  * **Tote Klerikerin:** Trägt die **Eldritch Rune** (öffnet Schattenherz' Kapsel) und den **Verzierten Schlüssel (Gold Key)**.
  * **Verzierte Truhe:** Enthält Gold, Schriftrolle und Onyx (wird mit dem Schlüssel geöffnet).
  * **Transformations-Schalter:** Verwandelt die Frau im Pod vor den Augen der Gruppe in einen Mindflayer (illustriert die Bedrohung).
* **Pfad:** Zurück nach Raum 4, um Schattenherz zu rekrutieren, danach zur Brücke.

---

### Raum 6: Die Brücke (Finale / Fluchtsequenz)
* **Visuals:** Riesige Halle, Panorama-Blick auf Höllenschlund, herabfallende Trümmer, Tentakelkonsolen.
* **Layout:** Langer, schmaler Raum, gespickt mit Hindernissen, Tanks und Gegnern. Am nördlichen Ende befindet sich das **Transponder-Steuerpult**.
* **Die Szene:**
  * Boss **Kommandant Zhalk** (Cambion-Dämon mit Flammenschwert *Everburn Blade*) kämpft gegen einen **Gedankenschinder**.
* **Missionsziel:**
  * Erreiche das Transponder-Pult am Ende des Raumes innerhalb von **X Runden** (z. B. 10-Runden-Countdown vor Schiffskollision).
* **Gegnergruppen im Raum:**
  * 3–4 Kobolde und Höllenhunde blockieren den Weg.
  * Nach Runde 5: Zwei weitere Cambion-Verstärkungen betreten die Brücke von hinten.
* **Taktische Optionen für das Demake:**
  * **Flucht-Strategie (Standard):** Gegner umgehen, explosive Fässer nutzen, geradewegs zum Pult sprinten.
  * **Kill-Strategie (High-Risk/Reward):** Zhalk mit vereinten Kräften und Säuretanks besiegen, um das mächtige **Flammenschwert (Everburn Blade)** als seltenen Drop für den Krieger zu looten.
* **Trigger am Transponder:**
  * Interaktion löst End-Cutscene aus: Schiff teleportiert sich, bricht auseinander und stürzt an der Schwertküste ab.

---

## 3. Begleiter & Trupp-Mechanik (Demake)

| Charakter | Klasse | Start-Rolle im Demake |
| :--- | :--- | :--- |
| **Spieler-Charakter** | Wählbar | Allrounder / Anführer |
| **Wir (Us)** | Gehirnwanderer | Leichte Nahkampf-Drohne (Schwache HP, hohe Beweglichkeit) |
| **Lae'zel** | Kriegerin | Robuste Nahkämpferin mit Zweihänder |
| **Schattenherz** | Klerikerin | Supporterin (Heilzauber *Wunden heilen*, *Befehl* / *Führung*) |

---

## 4. 16-Bit Gimmicks & Technische Kniffe
* **Umgebungs-Interaktion:** Aufnehmbare und werfbare Knorpelkisten (können enge Gänge für nachrückende Verstärkung blockieren).
* **Restaurations-Schreine:** Fungieren als die typischen JRPG-"Save/Heal-Points" vor den Kampfräumen.
* **Attributsprüfungen:** Als klassisches "Würfel rollt über den Textbox-Bildschirm"-Element umgesetzt.

# 16-Bit Demake: Visual & Sprite-Design Guide (Nautiloid Prologue)

**Grafikstil-Referenz:** SNES / Sega Mega Drive (32×32 bis 64×64 Pixel Sprites, begrenzte Farbpaletten pro Sprite, z. B. 15 Farben + Transparenz).

---

## 1. Spieler-Begleiter

### Lae'zel (Githyanki-Kriegerin)
* **Körper & Gesicht:**
  * Hautton: Blasses Olivgrün bis Gelbgrün.
  * Gesicht: Keine Nasenbeinkontur (Stupsnase/zwei Nasenlöcher als dunkle Pixel), mandelförmige schwarze Augen, spitze Elfenohren.
  * Haare: Dunkelbraunes Haar, streng nach hinten geflochten zu einem Zopf mit metallenen Haarbändern.
* **Kleidung & Rüstung:**
  * Bronzefarbene Halbplattenrüstung mit markanten, scharfkantigen Schulterstücken (Pausche).
  * Dunkelrotes Leder unter den Platten, braune Stiefel mit Eisenbeschlag.
* **Bewaffnung:**
  * Großes Zweihandschwert (silberne Klinge, Ledergriff), im Idle-Zustand oft schräg über den Rücken gelehnt.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 32×48 Pixel.
  * **Silhouette:** Breiter Oberkörper durch die Schulterplatten, athletische Statur.
  * **Farbpalette:** Gelbgrün (Haut), Bronze/Braun (Rüstung), Rostrot (Leder), Stahlgrau (Klinge).

---

### Schattenherz / Shadowheart (Halbelfen-Klerikerin)
* **Körper & Gesicht:**
  * Hautton: Helle, leicht aschige Hautfarbe.
  * Gesicht: Zarte Konturen, dezent spitze Ohren.
  * Haare: Tiefschwarzes Haar mit geradem Pony über der Stirn und einem langen, geflochtenen Pferdeschwanz, der von einer silbernen Sharraner-Haarspange gehalten wird.
* **Kleidung & Rüstung:**
  * Düstere Kleriker-Kettenrüstung in Schiefergrau und Stahlblau mit stilisierten Scheiben- und Stachelemblemen der Göttin Shar.
  * Untergewand aus dunklem Stoff.
* **Bewaffnung:**
  * Eiserner Streitkolben in der Haupthand, runder Metallschild mit Shar-Symbolik in der Nebenhand.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 32×48 Pixel.
  * **Silhouette:** Schlanker als Lae'zel; markant ist der gerade Haarpony und der runde Schild am Arm.
  * **Farbpalette:** Schwarz/Dunkelblau (Haare & Stoff), Blassrosa (Haut), Blaugrau/Silber (Rüstung).

---

### "Wir" / Us (Intellektverschlinger)
* **Körper:**
  * Ein freiliegendes, fleischig-rosafarbenes Gehirn, das direkt auf vier reptilien- bzw. hundeartigen Beinen aus Sehnen und Horn sitzt.
  * Keine sichtbaren Augen oder Münder; Oberfläche mit feuchten Gehirnwindungen überzogen.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 24×16 Pixel (sehr niedrig, wuselt am Boden).
  * **Animation:** Krabbelndes Trippeln mit leicht wippendem Gehirnkörper; bei Psionik-Angriffen pulsiert das Gehirn kurz lila auf.
  * **Farbpalette:** Fleischrosa, Dunkelrot (Falten/Schattierung), Hornweiß/Beige (Klauen).

---

## 2. Verbündete & Neutrale NPCs

### Myrnath (Der verletzte Elf)
* **Körper & Pose:**
  * Männlicher Elf in sitzender Pose auf einer fleischigen Illithiden-Liege.
  * Aufgesägte, offene Schädeldecke, aus der ein pulsierendes Gehirn hervorragt.
  * Bleiches, schmerzverzerrtes Gesicht, nackter Torso mit Verbänden oder zerrissener Tunika.
* **16-Bit Sprite-Richtlinie:**
  * Statisches Objekt-Sprite (32×32 Pixel) mit 2-Frame-Pulsieren am Kopf.

---

### Gedankenschinder / Mindflayer (Illithid)
* **Körper & Kopf:**
  * Große, hagere Gestalt mit purpurner/mauvefarbener, feucht glänzender Haut.
  * Oktopusartiger Kopf mit vier dicken Tentakeln um den Schlund, weiße pupillenlose Augen.
  * Dreifingrige Hände mit langen Klauen.
* **Kleidung:**
  * Aufwendige, dunkelviolette Seidenrobe mit riesigem Stehkragen hinter dem Kopf.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 32×56 Pixel (deutlich größer als Menschen).
  * **Animation:** Sich wellenartig bewegende Tentakel im Idle-State; Hände leuchten blau-violett bei Gedankenstößen.
  * **Farbpalette:** Violett/Mauve (Haut), Tiefes Indigo/Schwarz (Robe), Gold (Borten).

---

## 3. Gegner

### Niedere Kobolde (Imps)
* **Körper:**
  * Winzige, feuerrote Dämonen mit kleinen Ziegenhörnern, ledrigen Fledermausflügeln und einem dünnen Pfeilschwanz.
* **Ausrüstung:**
  * Schwingen kleine Eisendreizacke oder Miniatur-Armbrüste.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 16×24 Pixel (schwebt permanent 4 Pixel über dem Boden).
  * **Animation:** Schnelles 3-Frame-Flügelschlagen im Schwebeflug.
  * **Farbpalette:** Karminrot, Dunkelbraun/Schwarz (Flügelhaut), Gelb (Augen).

---

### Höllenschwein (Hellsboar)
* **Körper:**
  * Massives, monströses Wildschwein mit verbrannter, rissiger Haut, aus deren Rissen Magma glüht.
  * Riesige, nach oben gekrümmte Hauer, borstiger Rückenkamm steht stellenweise in Flammen.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 48×32 Pixel (breiter Block).
  * **Farbpalette:** Holzkohlenschwarz, Glutfarben (Gelb, Orange, Rot).

---

### Boss: Kommandant Zhalk (Cambion-Teufel)
* **Körper:**
  * Hüne mit muskulöser Statur, blutroter Haut, zwei gewaltigen, nach hinten geschwungenen Hörnern und riesigen Lederflügeln auf dem Rücken.
  * Glühende rote Augen und hämisches Grinsen mit Reißzähnen.
* **Rüstung & Waffe:**
  * Schwere, gezackte Plattenrüstung aus schwarzem Hölleneisen mit Schädel-Ornamenten.
  * **Immerbrand-Klinge (Everburn Blade):** Gigantisches Breitschwert, dessen gesamte Klinge permanent von animiertem Feuer umhüllt ist.
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 48×64 Pixel (Boss-Format, dominiert das Kampffeld).
  * **Effekt-Layer:** Separates Sprite-Overlay für das lodernde Flammenschwert (Orange/Gelb-Cycling).
  * **Farbpalette:** Blutrot (Haut), Vulkanschwarz (Rüstung), Leuchtendes Feuerrot/Gelb (Waffe).

---

### Cambion-Verstärkung (2 Zusatz-Teufel)
* **Körper:**
  * Kleinere Versionen von Zhalk (gleicher Körperbau mit Hörnern und Flügeln).
  * Tragen etwas leichtere Panzerung und führen Standard-Gleven oder Dreizacke (ohne Flammeneffekt).
* **16-Bit Sprite-Richtlinie:**
  * **Größe:** 32×52 Pixel. Dient als klarer visueller Unterschied zu Zhalks massiver Boss-Präsenz.

---

## 4. Interaktive Objekte & Kapseln

| Objekt | Optische Beschreibung | Animations-Trigger |
| :--- | :--- | :--- |
| **Mindflayer-Kapsel** | Vertikaler, eiförmiger Behälter aus Chitin und Sehnen mit transparent-türkisem Schleimglas. Im Inneren ist die dunkle Silhouette des Opfers sichtbar. | Glas splittert auf / öffnet sich mit Dampfwolke. |
| **Transformations-Opfer** | Gefangene Frau im Labor-Pod, trägt zerrissene Zivilkleidung. | Verwandelt sich über Morphen/Pixel-Dissolve in einen Gedankenschinder. |
| **Restaurierungs-Station** | Große, blau leuchtende Bio-Tentakelblase auf einer Säule. | Zieht sich bei Berührung zusammen und stößt glitzernde Heilpartikel aus. |
| **Nautiloid-Säuretanks** | Organische, lila gefüllte Knorpelfässer mit Venen. | Platzt bei Treffer in eine 3×3-Kachel große Säurelache auf. |


# Nautiloid-Prolog: Dialogsystem & Skript-Ablauf

Dieses Dokument enthält:
1. Den **Original-Dialogbaum** (vollständige Optionen & Skill-Checks aus *Baldur's Gate 3*).
2. Die **16-Bit-Adaption** (auf 2–3 prägnante Textbox-Zeilen und Menüauswahlen eingekürzt, ideal für SNES/Mega-Drive-Textboxen mit Würfel-Roll-Animation).

---

## Szene 1: Die Larven-Brutstätte (Raum 1)

### Original (BG3)
* **Trigger:** Untersuchen des pulsierenden Beckens.
* **Optionen:**
  1. Hand nach dem Becken ausstrecken.
  2. `[NACHFORSCHUNG]` Das Becken untersuchen. *(SG 10: Erfolg enthüllt instabile Hülle)*
  3. Gehen.
* **Konsequenz:** Berühren ohne Vorsicht löst eine kleine Säure-Explosion aus.

### 16-Bit Demake (Kompakt)
* **Textbox:** *„Ein Becken voller zuckender Kaulquappen. Die Hülle scheint brüchig.“*
* **Menü:**
  * `[A] Hineinfassen` ➔ *BOOM! -3 KP Schaden.*
  * `[B] Untersuchen [INT]` ➔ *Erfolg: „Gefahr erkannt.“ Becken wird markiert.*
  * `[C] Weggehen` ➔ *Abbruch.*

---

## Szene 2: Myrnath & "Wir" (Raum 2)

### Original (BG3)
* **Trigger:** Interaktion mit Myrnath auf dem OP-Tisch.
* **Gehirn (telepathisch):** *„Befreie uns! Wir müssen zum Steuer!“*
* **Optionen (Befreiung):**
  1. `[NACHFORSCHUNG]` Das Gehirn untersuchen. *(Schaltet Medizin-Vorteil frei)*
  2. `[STÄRKE]` Den Schädel aufbrechen. *(SG 10)*
  3. `[GESCHICKLICHKEIT]` Das Gehirn vorsichtig heraushebeln. *(SG 10)*
  4. `[MEDIZIN]` Eine gezielte Schädelentnahme durchführen. *(SG 10 mit Vorteil)*
  5. Gehirn zerstören.
  6. Gehen.
* **Folge-Dialog (Nach Befreiung):**
  * Das Gehirn springt heraus und wartet auf Befehle.
  1. `[GESCHICKLICHKEIT]` Gehirn verstümmeln / schwächen. *(SG 15: Zustand „Lobotomiert“)*
  2. Das Wesen verschonen.
  3. Angreifen / Töten.

### 16-Bit Demake (Kompakt)
* **Textbox:** *„Aus dem geöffneten Schädel ertönt eine Stimme im Kopf: ‚Befreie uns! Zum Ruder!‘“*
* **Menü 1:**
  * `[A] Schädel aufbrechen [STR]`
  * `[B] Vorsichtig entnehmen [DEX]`
  * `[C] Gehirn zerquetschen` ➔ *Gehirn stirbt.*
  * `[D] Ignorieren`
* *(Bei Erfolg springt „Wir“ heraus)*
* **Menü 2:**
  * `[A] Lobotomieren [DEX]` ➔ *„Wir“ wird schwächer, gehorcht aber blind.*
  * `[B] Als Begleiter aufnehmen` ➔ *„Wir“ schließt sich der Party an.*

---

## Szene 3: Außendeck & Lae'zel (Raum 3)

### Original (BG3)
* **Trigger:** Betreten des zerstörten Außendecks.
* **Cutscene:** Lae'zel greift aus dem Hinterhalt an, hält das Schwert an die Kehle des Spielers, spürt das Parasiten-Zecken und senkt die Waffe.
* **Lae'zel:** *„Abomination. This is not the end I was promised... Wait. You are whole. Not turned yet.“*
* **Optionen:**
  1. „Wer bist du?“
  2. „Greif mich nicht an, ich bin kein Feind!“
  3. Waffe ziehen und drohen.
* **Lae'zel:** *„We are infected. Unless we find a creche, we will transform. Together, we reach the helm, or we die here.“*

### 16-Bit Demake (Kompakt)
* **Auto-Cutscene:** Lae'zel springt ins Bild, Klingenkreuzen-Soundeffekt.
* **Lae'zel:** *„Ein Überlebender! Halt still... Dein Kopf pulsiert. Du bist infiziert, genau wie ich!“*
* **Menü:**
  * `[A] „Gemeinsam kämpfen!“`
  * `[B] „Wer bist du überhaupt?“`
* **Lae'zel:** *„Ich bin Lae'zel von den Githyanki. Diskutiert wird später – erst schlagen wir uns zum Steuerpult durch!“*
* *(Banner: „Lae'zel tritt der Gruppe bei!“ – Kampf startet direkt gegen 3 Imps)*

---

## Szene 4: Kapselsaal & Schattenherz (Raum 4)

### Original (BG3)
* **Trigger:** Anklopfen an die Kapsel von Schattenherz.
* **Schattenherz:** *„Get me out of here! The console – find a way to open it!“*
* **Optionen an der Kapsel:**
  1. Nach einem Riegel oder Schalter suchen.
  2. `[ARKANA / ZAUBERER]` Die Runenmagie untersuchen. *(SG 10)*
  3. „Keine Zeit, ich muss hier raus!“
* **Konsole daneben:**
  1. `[RUNE EINSETZEN]` *(Setzt die Eldritch Rune aus Raum 5 ein)*
  2. Hand auflegen und Willenskraft nutzen `[ILLITHID / WEISHEIT]` *(SG 2)*
  3. Mit Gewalt auf die Konsole schlagen.
* **Nach der Befreiung:**
  * Schattenherz bedankt sich: *„I thought I was done for. We need to reach the helm together.“*

### 16-Bit Demake (Kompakt)
* **Textbox:** *(Schattenherz hämmert gegen die Scheibe)* *„Hol mich hier raus! Die Konsole daneben braucht einen Schlüssel!“*
* **Interaktion Konsole (Ohne Rune):**
  * *„Ein runder Sockel fehlt. Vielleicht im Nebenraum?“*
* **Interaktion Konsole (Mit Rune aus Raum 5):**
  * `[A] Rune einsetzen [Aktion]` ➔ *Puff! Kapsel öffnet sich.*
  * `[B] Gewalt anwenden [STR]` ➔ *Konsole funkelt, nichts passiert.*
* **Nach Befreiung:**
  * **Schattenherz:** *„Danke. Ich dachte schon, das wäre mein Ende. Lass uns diesen Höllenort verlassen!“*
  * *(Banner: „Schattenherz tritt der Gruppe bei!“)*

---

## Szene 5: Das Labor & Transformations-Schalter (Raum 5)

### Original (BG3)
* **Trigger:** Interaktion mit dem zentralen Bedienpult im Nebenraum (vor einer bewohnten Kapsel).
* **Die 3 Knöpfe:**
  * Taste 1 (Freilassen): Lässt die Pod-Subjekte erwachen.
  * Taste 2 (Aggression): Schaltet Insassen feindlich.
  * Taste 3 (Vernichten): Tötet Insassen sofort.
* **Pod-Sequenz:** Ein Hebel aktiviert die Ceremorphose – die Frau verwandelt sich in Sekundenschnelle in einen Mindflayer.

### 16-Bit Demake (Kompakt)
* **Pult-Menü:**
  * `[Taste 1: Auslösen]` ➔ *Lila Nebel füllt den Pod. Die Frau morpht in einen Mindflayer!*
  * `[Taste 2: Vernichten]` ➔ *Blitz zuckt durch den Raum, Kapsel erlischt.*
  * `[Schreibtisch durchsuchen]` ➔ *Gefunden: [Eldritch-Rune] & [Verzierter Schlüssel].*

---

## Szene 6: Die Brücke & Flucht-Sequenz (Raum 6)

### Original (BG3)
* **Mindflayer (im Kampf mit Zhalk):** *„Connect the transponder! We are falling!“*
* **Zhalk:** *„Your skull will be my trophy, squids!“*
* **Runden-Count:** 10 Runden bis zum Crash.
* **Interaktion Transponder:**
  1. Nervenstränge verbinden.
  2. Zusehen / Zögern.

### 16-Bit Demake (Kompakt)
* **Brücken-Betreten (Skript-Intro):**
  * **Mindflayer:** *„Sklaven! Lauft zum Transponder und verbindet die Nerven! Beeilt euch!“*
  * **Zhalk:** *„Niemand verlässt diese Ebene lebend!“*
  * *(HUD-Timer erscheint: „10 RUNDEN BIS ZUM ABSTURZ“)*
* **Ziel-Interaktion (Transponder-Konsole):**
  * `[A] Nervenstränge verbinden!` ➔ *Löst Endsequenz & Flucht aus.*
  * `[B] Zurückweichen` ➔ *Bleibt im Kampf.*