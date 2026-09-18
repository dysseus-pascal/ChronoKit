#!/usr/bin/env python3
"""Store-Symbole: die Stoppuhr.

Aufruf: make_store_icon.py <zielordner>   -> icon-144.png, icon-48.png

WARUM DIESE DATEI ANDERS HEISST ALS BEI DEN GESCHWISTERN. Die anderen fuenf
Apps haben ein make_app_icon.py, das BEIDES erzeugt: das 25x25 fuer die Uhr und
die Store-Kacheln, aus einer einzigen Formbeschreibung. ChronoKit hatte nie ein
solches Werkzeug - sein resources/images/system_icon.png ist von Hand gesetzt.

Es nachtraeglich als Geometrie nachzubauen hiesse, ein ausgeliefertes Symbol
aufs Spiel zu setzen: trifft die Nachbildung auch nur einen Punkt daneben,
aendert sich das Bild im Starter auf jeder Uhr, auf der die App schon liegt.
Diese Datei erzeugt deshalb NUR die Store-Kacheln und laesst die Bitmap in
Ruhe. Der Preis ist ehrlich zu nennen: die Stoppuhr steht damit zweimal da,
einmal als Punkte und einmal als Geometrie hier. Wer eine aendert, muss an die
andere denken.

DIE VORLAGE IST DIE BITMAP, Punkt fuer Punkt ausgelesen: ein Ring um (12, 14)
mit Aussenradius 10 und zwei Punkten Staerke, Krone oben, Striche bei 3, 6 und
9 Uhr, in der Mitte die Nabe mit einem Zeiger nach oben. Ein Zeiger, nicht
zwei: bei 25 Punkten war der zweite nicht unterzubringen, und was die Stoppuhr
lesbar macht, ist die Krone, nicht das Zifferblatt.

DER STORE NIMMT NICHTS AUS DER .pbw. Im Entwicklerportal liegen zwei eigene
Bilder, `icon_large` und `icon_small`; angefordert werden sie in festen Massen
(gross 80 und 144, klein 28 und 48), jeweils mit `exact` in der Adresse, also
erzwungen statt eingepasst - etwas Nicht-Quadratisches kommt verzogen zurueck.
Darum eine gefuellte Kachel: das grosse Symbol legt der Store fuer sein
Teilen-Bild durch eine abgerundete Maske, und ueber einer durchsichtigen
Strichzeichnung taete die nichts.
"""
import os
import struct
import sys
import zlib

RASTER = 25                      # Bezugsraster, auf dem alle Masse gelten
SS = 4                           # Ueberabtastung je Achse

CX, CY = 12.0, 14.0              # Mitte des Zifferblatts
R_AUSSEN = 10.0                  # Aussenradius des Rings
RING = 2.0                       # Staerke des Rings

KRONE_HALB = 4.5                 # halbe Breite der Krone
KRONE_OBEN, KRONE_UNTEN = 0.0, 2.0
STIEL_HALB = 0.75               # halbe Breite des Kronenstiels

STRICH_HALB = 0.75               # halbe Staerke der Striche und des Zeigers
STRICH_LANG = 3.5                # wie weit ein Strich nach innen reicht
NABE = 1.8                       # Radius der Nabe
ZEIGER_BIS = 7.5                 # Laenge des Zeigers ab der Mitte

# Store-Kachel. Der Wert stammt aus src/c/theme.h (ZM_COLOR_ACCENT),
# nachgeschlagen in gcolor_definitions.h des SDK - nicht aus dem Gedaechtnis.
GRUND = (0x00, 0xAA, 0x55)       # GColorJaegerGreen
STRICH = (0xFF, 0xFF, 0xFF)      # weiss
FUELL = 0.72                     # wie viel der Kachel die Stoppuhr einnimmt
STORE_GROESSEN = (144, 48)


def png(path, w, h, rows):
    """Minimaler PNG-Schreiber, 8 Bit RGBA, ohne Fremdbibliothek."""
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


def raster(test, n):
    """Vierfach ueberabtasten, bei halber Deckung schneiden. Harte Kanten."""
    grid = []
    for py in range(n):
        row = []
        for px in range(n):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    if test(px + (sx + 0.5) / SS, py + (sy + 0.5) / SS):
                        hits += 1
            row.append(hits * 2 >= SS * SS)
        grid.append(row)
    return grid


def bar(x, y, x0, y0, x1, y1):
    """Ein gerades Balkenstueck, Ecken eingeschlossen."""
    return x0 <= x <= x1 and y0 <= y <= y1


def pruefer(s):
    """Der Formtest, auf den Massstab s gebracht."""
    cx, cy = CX * s, CY * s
    ra, ring = R_AUSSEN * s, RING * s
    kh, ko, ku, sh = KRONE_HALB * s, KRONE_OBEN * s, KRONE_UNTEN * s, STIEL_HALB * s
    th, tl, nabe, zb = STRICH_HALB * s, STRICH_LANG * s, NABE * s, ZEIGER_BIS * s

    def inside(x, y):
        dx, dy = x - cx, y - cy
        d = (dx * dx + dy * dy) ** 0.5
        # Ring
        if ra - ring <= d <= ra:
            return True
        # Krone oben, mit Stiel bis an den Ring
        if bar(x, y, cx - kh, ko, cx + kh, ku):
            return True
        if bar(x, y, cx - sh, ku, cx + sh, cy - ra + ring):
            return True
        # Striche bei 3, 6 und 9 Uhr, von innen an den Ring
        if bar(x, y, cx + ra - ring - tl, cy - th, cx + ra - ring, cy + th):
            return True
        if bar(x, y, cx - ra + ring, cy - th, cx - ra + ring + tl, cy + th):
            return True
        if bar(x, y, cx - th, cy + ra - ring - tl, cx + th, cy + ra - ring):
            return True
        # Nabe und ein Zeiger nach oben
        if d <= nabe:
            return True
        if bar(x, y, cx - th, cy - zb, cx + th, cy):
            return True
        return False
    return inside


def schreibe_store(dest):
    for gross in STORE_GROESSEN:
        innen = int(round(gross * FUELL))
        grid = raster(pruefer(innen / float(RASTER)), innen)
        rand = (gross - innen) // 2
        rows = []
        for y in range(gross):
            r = []
            for x in range(gross):
                iy, ix = y - rand, x - rand
                treffer = 0 <= iy < innen and 0 <= ix < innen and grid[iy][ix]
                farbe = STRICH if treffer else GRUND
                r += [farbe[0], farbe[1], farbe[2], 255]
            rows.append(r)
        name = "icon-%d.png" % gross
        png(os.path.join(dest, name), gross, gross, rows)
        print("%s: Kachel %s, Stoppuhr weiss" % (name, "#%02X%02X%02X" % GRUND))


def main():
    dest = sys.argv[1] if len(sys.argv) > 1 else "store"
    os.makedirs(dest, exist_ok=True)
    schreibe_store(dest)


if __name__ == "__main__":
    main()
