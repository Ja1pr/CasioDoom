#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
wad2calc.py - converts a DOOM WAD (IWAD, optionally with PWADs) into the
"calculator WAD" (.CWD) used by CasioDoom on the CASIO fx-9860GIII.

What it does:
  * drops all sound, music and colour data,
  * converts the palette to 64 grey levels (dithered to 1 bit on the device),
  * composes wall textures and shrinks textures / flats / sprites,
  * replaces the status bar, fonts and menu art by tiny 128x64 versions,
  * rebuilds every level into ready-to-use, pointer-free structures, so
    that the calculator can use them straight from flash without loading
    them into its (very small) RAM,
  * writes everything big-endian (SH4) by default.

Usage:
    python3 wad2calc.py doom1.wad                 -> DOOM.CWD
    python3 wad2calc.py doom.wad -o DOOM.CWD --maps E1M1-E1M9
    python3 wad2calc.py doom2.wad --maps MAP01-MAP08 --shift 2
    python3 wad2calc.py doom.wad mylevel.wad --maps E1M1

Only the Python standard library is needed.

Copyright (C) 2026 aurasscz. Written with Claude Code (Anthropic).
Licensed under the GNU GPL, version 2 or later; see README.md.
"""

import argparse
import math
import os
import struct
import sys

VERSION = 3          # must match CWAD_VERSION in src/w_wad.h

# ----------------------------------------------------------------------------
# Tables copied from the DOOM source (info.c, p_spec.c, p_switch.c)
# ----------------------------------------------------------------------------

SPRNAMES = """TROO SHTG PUNG PISG PISF SHTF SHT2 CHGG CHGF MISG
MISF SAWG PLSG PLSF BFGG BFGF BLUD PUFF BAL1 BAL2
PLSS PLSE MISL BFS1 BFE1 BFE2 TFOG IFOG PLAY POSS
SPOS VILE FIRE FATB FBXP SKEL MANF FATT CPOS SARG
HEAD BAL7 BOSS BOS2 SKUL SPID BSPI APLS APBX CYBR
PAIN SSWV KEEN BBRN BOSF ARM1 ARM2 BAR1 BEXP FCAN
BON1 BON2 BKEY RKEY YKEY BSKU RSKU YSKU STIM MEDI
SOUL PINV PSTR PINS MEGA SUIT PMAP PVIS CLIP AMMO
ROCK BROK CELL CELP SHEL SBOX BPAK BFUG MGUN CSAW
LAUN PLAS SHOT SGN2 COLU SMT2 GOR1 POL2 POL5 POL4
POL3 POL1 POL6 GOR2 GOR3 GOR4 GOR5 SMIT COL1 COL2
COL3 COL4 CAND CBRA COL6 TRE1 TRE2 ELEC CEYE FSKU
COL5 TBLU TGRN TRED SMBT SMGT SMRT HDB1 HDB2 HDB3
HDB4 HDB5 HDB6 POB1 POB2 BRS1 TLMP TLP2""".split()

# (is_texture, endname, startname)
ANIMDEFS = [
    (False, "NUKAGE3", "NUKAGE1"), (False, "FWATER4", "FWATER1"),
    (False, "SWATER4", "SWATER1"), (False, "LAVA4", "LAVA1"),
    (False, "BLOOD3", "BLOOD1"), (False, "RROCK08", "RROCK05"),
    (False, "SLIME04", "SLIME01"), (False, "SLIME08", "SLIME05"),
    (False, "SLIME12", "SLIME09"),
    (True, "BLODGR4", "BLODGR1"), (True, "SLADRIP3", "SLADRIP1"),
    (True, "BLODRIP4", "BLODRIP1"), (True, "FIREWALL", "FIREWALA"),
    (True, "GSTFONT3", "GSTFONT1"), (True, "FIRELAVA", "FIRELAV3"),
    (True, "FIREMAG3", "FIREMAG1"), (True, "FIREBLU2", "FIREBLU1"),
    (True, "ROCKRED3", "ROCKRED1"), (True, "BFALL4", "BFALL1"),
    (True, "SFALL4", "SFALL1"), (True, "WFALL4", "WFALL1"),
    (True, "DBRAIN4", "DBRAIN1"),
]

SWITCHES = """BRCOM BRN1 BRN2 BRNGN BROWN COMM COMP DIRT EXIT GRAY GRAY1 METAL
PIPE SLAD STARG STON1 STON2 STONE STRTN BLUE CMT GARG GSTON HOT LION SATYR
SKIN VINE WOOD PANEL ROCK MET2 WDMET BRIK MOD1 ZIM STON6 TEK MARB
SKULL""".split()

SKIES = ["SKY1", "SKY2", "SKY3", "SKY4"]

ML_TWOSIDED = 4

# ----------------------------------------------------------------------------
# Tiny bitmap fonts
# ----------------------------------------------------------------------------

FONT3X5 = {
    '!': ".#. .#. .#. ... .#.", '"': "#.# #.# ... ... ...",
    '#': "#.# ### #.# ### #.#", '$': ".## ##. .#. .## ##.",
    '%': "#.. ..# .#. #.. ..#", '&': ".#. #.# .#. #.# .##",
    "'": ".#. .#. ... ... ...", '(': "..# .#. .#. .#. ..#",
    ')': "#.. .#. .#. .#. #..", '*': "#.# .#. #.# ... ...",
    '+': "... .#. ### .#. ...", ',': "... ... ... .#. #..",
    '-': "... ... ### ... ...", '.': "... ... ... ... .#.",
    '/': "..# ..# .#. #.. #..",
    '0': "### #.# #.# #.# ###", '1': ".#. ##. .#. .#. ###",
    '2': "### ..# ### #.. ###", '3': "### ..# ### ..# ###",
    '4': "#.# #.# ### ..# ..#", '5': "### #.. ### ..# ###",
    '6': "### #.. ### #.# ###", '7': "### ..# ..# .#. .#.",
    '8': "### #.# ### #.# ###", '9': "### #.# ### ..# ###",
    ':': "... .#. ... .#. ...", ';': "... .#. ... .#. #..",
    '<': "..# .#. #.. .#. ..#", '=': "... ### ... ### ...",
    '>': "#.. .#. ..# .#. #..", '?': "### ..# .#. ... .#.",
    '@': "### #.# ### #.. ###",
    'A': ".#. #.# ### #.# #.#", 'B': "##. #.# ##. #.# ##.",
    'C': ".## #.. #.. #.. .##", 'D': "##. #.# #.# #.# ##.",
    'E': "### #.. ##. #.. ###", 'F': "### #.. ##. #.. #..",
    'G': ".## #.. #.# #.# .##", 'H': "#.# #.# ### #.# #.#",
    'I': "### .#. .#. .#. ###", 'J': "..# ..# ..# #.# .#.",
    'K': "#.# #.# ##. #.# #.#", 'L': "#.. #.. #.. #.. ###",
    'M': "#.# ### ### #.# #.#", 'N': "##. #.# #.# #.# #.#",
    'O': ".#. #.# #.# #.# .#.", 'P': "##. #.# ##. #.. #..",
    'Q': ".#. #.# #.# ### .##", 'R': "##. #.# ##. #.# #.#",
    'S': ".## #.. .#. ..# ##.", 'T': "### .#. .#. .#. .#.",
    'U': "#.# #.# #.# #.# ###", 'V': "#.# #.# #.# #.# .#.",
    'W': "#.# #.# ### ### #.#", 'X': "#.# #.# .#. #.# #.#",
    'Y': "#.# #.# .#. .#. .#.", 'Z': "### ..# .#. #.. ###",
    '[': "##. #.. #.. #.. ##.", '\\': "#.. #.. .#. ..# ..#",
    ']': ".## ..# ..# ..# .##", '^': ".#. #.# ... ... ...",
    '_': "... ... ... ... ###",
}

BIGNUM = {
    '0': ".##. #..# #..# #..# #..# #..# .##.",
    '1': "..#. .##. ..#. ..#. ..#. ..#. .###",
    '2': ".##. #..# ...# ..#. .#.. #... ####",
    '3': "###. ...# ...# .##. ...# ...# ###.",
    '4': "...# ..## .#.# #..# #### ...# ...#",
    '5': "#### #... ###. ...# ...# #..# .##.",
    '6': ".##. #... #... ###. #..# #..# .##.",
    '7': "#### ...# ..#. ..#. .#.. .#.. .#..",
    '8': ".##. #..# #..# .##. #..# #..# .##.",
    '9': ".##. #..# #..# .### ...# ...# .##.",
    '%': "##.# ##.# ..#. ..#. .#.. #.## #.##",
    '-': ".... .... .... #### .... .... ....",
}

BLACK = 0
WHITE = 63


class Image:
    """Grey image; pix[y][x] is 0..63 or None (transparent)."""

    def __init__(self, w, h, fill=None):
        self.w = w
        self.h = h
        self.pix = [[fill] * w for _ in range(h)]
        self.left = 0
        self.top = 0

    def blit(self, src, ox, oy):
        for y in range(src.h):
            ty = y + oy
            if ty < 0 or ty >= self.h:
                continue
            row = self.pix[ty]
            srow = src.pix[y]
            for x in range(src.w):
                tx = x + ox
                v = srow[x]
                if v is not None and 0 <= tx < self.w:
                    row[tx] = v


def glyph_image(rows, opaque_bg, extra_w=1, extra_h=1):
    rows = rows.split()
    gw = len(rows[0])
    img = Image(gw + extra_w, len(rows) + extra_h,
                WHITE if opaque_bg else None)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == '#':
                img.pix[y][x] = BLACK
    return img


def text_image(text, opaque_bg=True):
    img = Image(4 * len(text), 6, WHITE if opaque_bg else None)
    for i, ch in enumerate(text.upper()):
        if ch in FONT3X5:
            img.blit(glyph_image(FONT3X5[ch], False), 4 * i, 0)
    return img


def rows_image(rows, mapping=None):
    mapping = mapping or {'#': BLACK, '.': None, 'o': WHITE}
    img = Image(len(rows[0]), len(rows))
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            img.pix[y][x] = mapping[c]
    return img


# ----------------------------------------------------------------------------
# WAD reading
# ----------------------------------------------------------------------------

class Wad:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        magic, n, ofs = struct.unpack_from("<4sii", self.data, 0)
        if magic not in (b"IWAD", b"PWAD"):
            raise SystemExit("%s: not a WAD file" % path)
        self.iwad = magic == b"IWAD"
        self.lumps = []
        for i in range(n):
            o, s, name = struct.unpack_from("<ii8s", self.data, ofs + 16 * i)
            name = name.split(b"\0")[0].decode("ascii", "replace").upper()
            self.lumps.append((name, o, s))

    def read(self, i):
        name, o, s = self.lumps[i]
        return self.data[o:o + s]


class WadSet:
    """IWAD + PWADs with 'last one wins' lookup."""

    def __init__(self, paths):
        self.wads = [Wad(p) for p in paths]
        self.byname = {}
        self.flats = {}        # name -> data (order kept)
        self.sprites = {}
        self.maps = {}         # name -> (wad, index of marker)
        for w in self.wads:
            section = None
            for i, (name, o, s) in enumerate(w.lumps):
                if name in ("F_START", "FF_START"):
                    section = "F"
                elif name in ("S_START", "SS_START"):
                    section = "S"
                elif name in ("F_END", "FF_END", "S_END", "SS_END"):
                    section = None
                elif section == "F" and s == 4096:
                    self.flats[name] = w.read(i)
                elif section == "S" and s > 0:
                    self.sprites[name] = w.read(i)
                if is_map_name(name) and i + 1 < len(w.lumps) \
                        and w.lumps[i + 1][0] == "THINGS":
                    self.maps[name] = (w, i)
                self.byname[name] = (w, i)

    def get(self, name):
        if name not in self.byname:
            return None
        w, i = self.byname[name]
        return w.read(i)

    def has(self, name):
        return name in self.byname


def is_map_name(n):
    if len(n) == 4 and n[0] == 'E' and n[2] == 'M' and n[1].isdigit() \
            and n[3].isdigit():
        return True
    return len(n) == 5 and n.startswith("MAP") and n[3:].isdigit()


# ----------------------------------------------------------------------------
# Graphics helpers
# ----------------------------------------------------------------------------

def decode_patch(data, gray):
    """DOOM patch -> Image (with offsets)."""
    w, h, left, top = struct.unpack_from("<hhhh", data, 0)
    img = Image(w, h)
    img.left = left
    img.top = top
    for x in range(w):
        ofs = struct.unpack_from("<i", data, 8 + 4 * x)[0]
        while True:
            topdelta = data[ofs]
            if topdelta == 0xff:
                break
            length = data[ofs + 1]
            base = ofs + 3
            for j in range(length):
                y = topdelta + j
                if 0 <= y < h:
                    img.pix[y][x] = gray[data[base + j]]
            ofs += length + 4
    return img


def shrink(img, shift):
    """Integer box-filter reduction by 2^shift, grid anchored at (0,0)."""
    if shift == 0:
        return img
    s = 1 << shift
    nw = (img.w + s - 1) >> shift
    nh = (img.h + s - 1) >> shift
    out = Image(nw, nh)
    out.left, out.top = img.left, img.top
    for ny in range(nh):
        y0 = ny << shift
        y1 = min(y0 + s, img.h)
        orow = out.pix[ny]
        for nx in range(nw):
            x0 = nx << shift
            x1 = min(x0 + s, img.w)
            tot = 0
            cnt = 0
            cells = (x1 - x0) * (y1 - y0)
            for y in range(y0, y1):
                row = img.pix[y]
                for x in range(x0, x1):
                    v = row[x]
                    if v is not None:
                        tot += v
                        cnt += 1
            if cnt * 2 >= cells and cnt:
                orow[nx] = (tot + cnt // 2) // cnt
    return out


def resample(img, nw, nh):
    """Area resampling to an arbitrary (smaller) size."""
    out = Image(nw, nh)
    for ny in range(nh):
        y0 = ny * img.h // nh
        y1 = max(y0 + 1, (ny + 1) * img.h // nh)
        for nx in range(nw):
            x0 = nx * img.w // nw
            x1 = max(x0 + 1, (nx + 1) * img.w // nw)
            tot = cnt = 0
            for y in range(y0, min(y1, img.h)):
                row = img.pix[y]
                for x in range(x0, min(x1, img.w)):
                    v = row[x]
                    if v is not None:
                        tot += v
                        cnt += 1
            cells = (y1 - y0) * (x1 - x0)
            if cnt and cnt * 2 >= cells:
                out.pix[ny][nx] = (tot + cnt // 2) // cnt
    return out


def scale_ui(img, fx, fy):
    """Scale a UI patch (keeps its offsets consistent)."""
    nw = max(1, int(round(img.w * fx)))
    nh = max(1, int(round(img.h * fy)))
    out = resample(img, nw, nh)
    out.left = int(round(img.left * fx))
    out.top = int(round(img.top * fy))
    return out


def precompensate(img):
    """The engine stretches greys 3..43 to the full range when it dithers
    (DOOM levels are dark). Pictures get the inverse here, so that they
    come out the way they were drawn."""
    for row in img.pix:
        for x, v in enumerate(row):
            if v is not None:
                row[x] = 3 + (v * 40 + 31) // 63
    return img


def stretch_contrast(img, amount=1.0):
    """Stretch the grey range of a picture to use the full 0..63 range."""
    vals = sorted(v for row in img.pix for v in row if v is not None)
    if not vals:
        return img
    lo = vals[len(vals) * 2 // 100]
    hi = vals[len(vals) * 98 // 100]
    if hi - lo < 8:
        return img
    for row in img.pix:
        for x, v in enumerate(row):
            if v is not None:
                n = (v - lo) * 63.0 / (hi - lo)
                n = v + (n - v) * amount
                row[x] = max(0, min(63, int(round(n))))
    return img


def encode_posts(img, x):
    """Column x of an image as DOOM posts (topdelta,len,pad,data,pad)."""
    out = bytearray()
    y = 0
    h = img.h
    while y < h:
        if img.pix[y][x] is None:
            y += 1
            continue
        y0 = y
        run = bytearray()
        while y < h and img.pix[y][x] is not None and len(run) < 254:
            run.append(img.pix[y][x])
            y += 1
        out.append(y0)
        out.append(len(run))
        out.append(run[0])
        out += run
        out.append(run[-1])
    out.append(0xff)
    return bytes(out)


# ----------------------------------------------------------------------------
# Converter
# ----------------------------------------------------------------------------

class Converter:
    def __init__(self, args):
        self.args = args
        self.big = args.endian == "big"
        self.E = ">" if self.big else "<"
        self.shift = args.shift
        self.ws = WadSet(args.wads)
        self.out = {}          # name -> bytes
        self.log = []
        self.make_gray()

    # -- helpers -----------------------------------------------------------

    def pack(self, fmt, *v):
        return struct.pack(self.E + fmt, *v)

    def add(self, name, data):
        assert len(name) <= 8, name
        if name in self.out:
            raise SystemExit("duplicate lump " + name)
        self.out[name] = bytes(data)

    def make_gray(self):
        pal = self.ws.get("PLAYPAL")
        if pal is None:
            raise SystemExit("no PLAYPAL lump: an IWAD is required")
        g = []
        gamma = self.args.gamma
        for i in range(256):
            r, gr, b = pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]
            # max() keeps saturated reds/blues from turning into mud
            lum = 0.6 * (0.299 * r + 0.587 * gr + 0.114 * b) \
                + 0.4 * max(r, gr, b)
            lum = (lum / 255.0) ** (1.0 / gamma)
            g.append(max(0, min(63, int(round(lum * 63)))))
        self.gray = g

    def encode_patch(self, img, world=False, shift=0):
        """Image -> calculator patch lump.

        world=True: offsets and 'worldwidth' are in map units while the
        pixel data is reduced by 2^shift (sprites)."""
        if world:
            ww, left, top = img.w, img.left, img.top
            data = shrink(img, shift)
        else:
            data = img
            ww, left, top = img.w, img.left, img.top
        cols = [encode_posts(data, x) for x in range(data.w)]
        hdr = self.pack("hhhhhh", data.w, data.h, left, top, ww, 0)
        ofs = 12 + 2 * data.w
        tab = bytearray()
        body = bytearray()
        seen = {}
        for c in cols:
            if c in seen:
                tab += self.pack("H", seen[c])
                continue
            if ofs + len(body) > 0xffff:
                raise SystemExit("patch too big")
            seen[c] = ofs + len(body)
            tab += self.pack("H", ofs + len(body))
            body += c
        return hdr + bytes(tab) + bytes(body)

    # -- maps selection ----------------------------------------------------

    def select_maps(self):
        avail = sorted(self.ws.maps.keys())
        if not avail:
            raise SystemExit("no maps found in the WAD")
        sel = self.args.maps
        if not sel:
            self.maps = avail
        else:
            chosen = []
            for part in sel.upper().split(","):
                part = part.strip()
                if "-" in part:
                    a, b = part.split("-")
                    chosen += [m for m in avail if a <= m <= b
                               and len(m) == len(a)]
                elif part in avail:
                    chosen.append(part)
                else:
                    raise SystemExit("map %s not in WAD" % part)
            self.maps = sorted(set(chosen))
        self.commercial = any(m.startswith("MAP") for m in avail)
        if self.commercial:
            self.gamemode = 2
        elif self.ws.has("E4M1"):
            self.gamemode = 3
        elif self.ws.has("E3M1") or self.ws.has("E2M1"):
            self.gamemode = 1
        else:
            self.gamemode = 0

    # -- textures ----------------------------------------------------------

    def load_textures(self):
        pn = self.ws.get("PNAMES")
        n = struct.unpack_from("<i", pn, 0)[0]
        self.pnames = [pn[4 + 8 * i:12 + 8 * i].split(b"\0")[0]
                       .decode("ascii", "replace").upper() for i in range(n)]
        self.texdefs = []       # (name, w, h, [(ox, oy, patchname)])
        for lump in ("TEXTURE1", "TEXTURE2"):
            d = self.ws.get(lump)
            if d is None:
                continue
            cnt = struct.unpack_from("<i", d, 0)[0]
            for i in range(cnt):
                o = struct.unpack_from("<i", d, 4 + 4 * i)[0]
                name, masked, w, h, cd, pc = struct.unpack_from(
                    "<8sihhih", d, o)
                name = name.split(b"\0")[0].decode("ascii").upper()
                patches = []
                for j in range(pc):
                    ox, oy, p, sd, cm = struct.unpack_from(
                        "<hhhhh", d, o + 22 + 10 * j)
                    patches.append((ox, oy, self.pnames[p]))
                self.texdefs.append((name, w, h, patches))
        self.texindex = {}
        for i, t in enumerate(self.texdefs):
            self.texindex.setdefault(t[0], i)

    def compose_texture(self, tdef):
        name, w, h, patches = tdef
        img = Image(w, h)
        cache = self.patchcache
        for ox, oy, pname in patches:
            if pname not in cache:
                d = self.ws.get(pname)
                cache[pname] = decode_patch(d, self.gray) if d else None
            p = cache[pname]
            if p is not None:
                img.blit(p, ox, oy)
        return img

    def encode_texture(self, tdef):
        name, w, h, patches = tdef
        img = self.compose_texture(tdef)
        masked = any(v is None for row in img.pix for v in row)
        small = shrink(img, self.shift)
        th = 128 >> self.shift
        raw = bytearray()
        for x in range(small.w):
            for y in range(th):
                v = small.pix[y % small.h][x]
                raw.append(0 if v is None else v)
        while len(raw) % 2:
            raw.append(0)
        if masked:
            cols = [encode_posts(small, x) for x in range(small.w)]
            tabofs = 4 + len(raw)
            ofs = tabofs + 2 * small.w
            tab = bytearray()
            body = bytearray()
            for c in cols:
                tab += self.pack("H", ofs + len(body))
                body += c
            if ofs + len(body) > 0xffff:
                raise SystemExit("texture %s too big" % name)
            return self.pack("HH", small.w, tabofs) + bytes(raw) \
                + bytes(tab) + bytes(body), True
        return self.pack("HH", small.w, 0) + bytes(raw), False

    # -- level conversion --------------------------------------------------

    def read_map(self, name):
        w, i = self.ws.maps[name]
        lumps = {}
        for j in range(1, 11):
            if i + j >= len(w.lumps):
                break
            n = w.lumps[i + j][0]
            if n in ("THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
                     "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"):
                lumps[n] = w.read(i + j)
            else:
                break
        for need in ("THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
                     "SSECTORS", "NODES", "SECTORS", "BLOCKMAP"):
            if need not in lumps:
                raise SystemExit("%s: missing %s (build nodes first)"
                                 % (name, need))
        m = {}
        d = lumps["VERTEXES"]
        m["vertexes"] = [struct.unpack_from("<hh", d, 4 * k)
                         for k in range(len(d) // 4)]
        d = lumps["SECTORS"]
        m["sectors"] = []
        for k in range(len(d) // 26):
            fh, ch, fp, cp, light, special, tag = struct.unpack_from(
                "<hh8s8shhh", d, 26 * k)
            m["sectors"].append([fh, ch, lumpname(fp), lumpname(cp), light,
                                 special, tag])
        d = lumps["SIDEDEFS"]
        m["sides"] = []
        for k in range(len(d) // 30):
            xo, yo, top, bot, mid, sec = struct.unpack_from(
                "<hh8s8s8sh", d, 30 * k)
            m["sides"].append([xo, yo, lumpname(top), lumpname(bot),
                               lumpname(mid), sec])
        d = lumps["LINEDEFS"]
        m["lines"] = [list(struct.unpack_from("<hhhhhhh", d, 14 * k))
                      for k in range(len(d) // 14)]
        d = lumps["SEGS"]
        m["segs"] = [struct.unpack_from("<hhhhhh", d, 12 * k)
                     for k in range(len(d) // 12)]
        d = lumps["SSECTORS"]
        m["ssectors"] = [struct.unpack_from("<HH", d, 4 * k)
                         for k in range(len(d) // 4)]
        d = lumps["NODES"]
        m["nodes"] = [struct.unpack_from("<hhhh8hHH", d, 28 * k)
                      for k in range(len(d) // 28)]
        d = lumps["THINGS"]
        m["things"] = [struct.unpack_from("<hhhhh", d, 10 * k)
                       for k in range(len(d) // 10)]
        d = lumps["BLOCKMAP"]
        m["blockmap"] = list(struct.unpack_from("<%dh" % (len(d) // 2), d, 0))
        m["reject"] = lumps.get("REJECT", b"")
        return m

    def convert_map(self, name, m):
        P = self.pack
        nsec = len(m["sectors"])
        sides = m["sides"]

        def side_sector(sn):
            if sn < 0 or sn >= len(sides):
                return -1
            return sides[sn][5]

        # vertexes
        self.add(name + "VTX", b"".join(
            P("ii", x << 16, y << 16) for x, y in m["vertexes"]))

        # lines
        vt = m["vertexes"]
        out = bytearray()
        linesec = []
        for (v1, v2, flags, special, tag, s0, s1) in m["lines"]:
            x1, y1 = vt[v1]
            x2, y2 = vt[v2]
            dx, dy = x2 - x1, y2 - y1
            if dx == 0:
                slope = 1
            elif dy == 0:
                slope = 0
            elif (dy > 0) == (dx > 0):
                slope = 2
            else:
                slope = 3
            if s0 == -1 or s0 == 0xffff - 0x10000:
                s0 = -1
            fs = side_sector(s0)
            bs = side_sector(s1)
            linesec.append((fs, bs))
            out += P("ii", dx << 16, dy << 16)
            out += P("iiii", max(y1, y2) << 16, min(y1, y2) << 16,
                     min(x1, x2) << 16, max(x1, x2) << 16)
            out += P("hhhhhhhhhBB", v1, v2, flags, special, tag, s0, s1,
                     fs, bs, slope, 0)
        self.add(name + "LIN", out)

        # sides
        out = bytearray()
        for (xo, yo, top, bot, mid, sec) in sides:
            out += P("iihhhh", xo << 16, yo << 16, self.texnum(top, name),
                     self.texnum(bot, name), self.texnum(mid, name), sec)
        self.add(name + "SID", out)

        # sectors, with their line lists and blockmap boxes
        bm = m["blockmap"]
        orgx, orgy, bw, bh = bm[0], bm[1], bm[2], bm[3]
        seclines = [[] for _ in range(nsec)]
        for li, (fs, bs) in enumerate(linesec):
            if 0 <= fs < nsec:
                seclines[fs].append(li)
            if 0 <= bs < nsec and bs != fs:
                seclines[bs].append(li)
        out = bytearray()
        sln = bytearray()
        first = 0
        for si, (fh, ch, fp, cp, light, special, tag) in \
                enumerate(m["sectors"]):
            xs = []
            ys = []
            for li in seclines[si]:
                l = m["lines"][li]
                for v in (l[0], l[1]):
                    xs.append(vt[v][0])
                    ys.append(vt[v][1])
            if xs:
                bt = min((max(ys) - orgy + 32) >> 7, bh - 1)
                bb = max((min(ys) - orgy - 32) >> 7, 0)
                br = min((max(xs) - orgx + 32) >> 7, bw - 1)
                bl = max((min(xs) - orgx - 32) >> 7, 0)
            else:
                bt = bb = br = bl = 0
            out += P("hhhhhhhhihhhh", fh, ch, self.flatnum(fp),
                     self.flatnum(cp), light, special, tag,
                     len(seclines[si]), first, bt, bb, bl, br)
            for li in seclines[si]:
                sln += P("H", li)
            first += len(seclines[si])
        self.add(name + "SEC", out)
        self.add(name + "SLN", sln if sln else P("H", 0))

        # segs
        out = bytearray()
        for (v1, v2, angle, linedef, side, offset) in m["segs"]:
            l = m["lines"][linedef]
            sd = l[5 + side]
            fs = side_sector(sd)
            if l[2] & ML_TWOSIDED:
                bs = side_sector(l[5 + (side ^ 1)])
            else:
                bs = -1
            out += P("hhhhhhiI", v1, v2, sd, linedef, fs, bs, offset << 16,
                     (angle & 0xffff) << 16)
        self.add(name + "SEG", out)

        # subsectors: sector comes from the first seg
        out = bytearray()
        for (numsegs, firstseg) in m["ssectors"]:
            s = m["segs"][firstseg]
            l = m["lines"][s[3]]
            sec = side_sector(l[5 + s[4]])
            out += P("hHHH", sec, numsegs, firstseg, 0)
        self.add(name + "SSC", out)

        # nodes
        out = bytearray()
        for nd in m["nodes"]:
            out += P("iiii", nd[0] << 16, nd[1] << 16, nd[2] << 16,
                     nd[3] << 16)
            out += P("8i", *[v << 16 for v in nd[4:12]])
            out += P("HH", nd[12], nd[13])
        self.add(name + "NOD", out)

        # things
        self.add(name + "THG", b"".join(P("hhhhh", *t) for t in m["things"]))

        # blockmap and reject
        self.add(name + "BLK", P("%dh" % len(bm), *bm))
        need = (nsec * nsec + 7) // 8
        rej = m["reject"]
        if len(rej) < need:
            rej = rej + bytes(need - len(rej))
        self.add(name + "REJ", rej[:need])

    def texnum(self, name, mapname=""):
        if name.startswith("-") or name == "":
            return 0
        if name not in self.texmap:
            if name not in self.warned:
                self.warned.add(name)
                self.log.append("warning: %s: unknown texture %s"
                                % (mapname, name))
            return 0
        return self.texmap[name]

    def flatnum(self, name):
        if name not in self.flatmap:
            if name not in self.warned:
                self.warned.add(name)
                self.log.append("warning: unknown flat %s" % name)
            return 0
        return self.flatmap[name]

    # -- main --------------------------------------------------------------

    def run(self):
        args = self.args
        self.select_maps()
        self.load_textures()
        self.patchcache = {}
        self.warned = set()

        maps = {name: self.read_map(name) for name in self.maps}

        # ---- which textures / flats are needed
        usedtex = set()
        usedflat = set(["F_SKY1"])
        for m in maps.values():
            for s in m["sides"]:
                usedtex.update(s[2:5])
            for s in m["sectors"]:
                usedflat.update(s[2:4])
        usedtex.update(SKIES)
        if args.all_textures:
            usedtex.update(t[0] for t in self.texdefs)
            usedflat.update(self.ws.flats.keys())
        # switch partners
        for sw in SWITCHES:
            a, b = "SW1" + sw, "SW2" + sw
            if a in usedtex or b in usedtex:
                usedtex.update((a, b))
        # animation ranges (index ranges, exactly like P_InitPicAnims)
        flatorder = list(self.ws.flats.keys())
        flatpos = {n: i for i, n in enumerate(flatorder)}
        for istex, end, start in ANIMDEFS:
            if istex:
                if start in self.texindex and end in self.texindex:
                    rng = [self.texdefs[i][0] for i in range(
                        self.texindex[start], self.texindex[end] + 1)]
                    if usedtex & set(rng):
                        usedtex.update(rng)
            else:
                if start in flatpos and end in flatpos:
                    rng = flatorder[flatpos[start]:flatpos[end] + 1]
                    if usedflat & set(rng):
                        usedflat.update(rng)

        # ---- textures (index 0 is "no texture")
        self.texmap = {}
        texinfo = bytearray(self.pack("8sHHHH", b"-", 0, 0, 0, 0))
        texlumps = []
        idx = 1
        for i, tdef in enumerate(self.texdefs):
            name = tdef[0]
            if name not in usedtex or self.texindex[name] != i:
                continue
            data, masked = self.encode_texture(tdef)
            self.texmap[name] = idx
            texlumps.append((idx, name, tdef[1], tdef[2], data, masked))
            idx += 1
        numtextures = idx

        # ---- flats
        self.flatmap = {}
        flatlumps = []
        fs = 64 >> self.shift
        for name in flatorder:
            if name not in usedflat:
                continue
            d = self.ws.flats[name]
            img = Image(64, 64)
            for y in range(64):
                row = img.pix[y]
                for x in range(64):
                    row[x] = self.gray[d[y * 64 + x]]
            small = shrink(img, self.shift)
            raw = bytes(small.pix[y][x] for y in range(fs) for x in range(fs))
            self.flatmap[name] = len(flatlumps)
            flatlumps.append((name, raw))
        if "F_SKY1" not in self.flatmap:
            self.flatmap["F_SKY1"] = len(flatlumps)
            flatlumps.append(("F_SKY1", bytes(fs * fs)))

        # ---- levels
        for name in self.maps:
            self.convert_map(name, maps[name])

        # ---- sprites
        sprlumps = []          # (data)
        sprindex = {}
        frames = {}            # sprite -> {frame: [8 x (lump, flip)]}
        for lname, d in self.ws.sprites.items():
            if len(lname) < 6 or lname[:4] not in SPRNAMES:
                continue
            try:
                img = decode_patch(d, self.gray)
            except (struct.error, IndexError):
                self.log.append("warning: bad sprite " + lname)
                continue
            sprindex[lname] = len(sprlumps)
            sprlumps.append(self.encode_patch(img, True, self.shift))
            for k in (4, 6):
                if len(lname) >= k + 2:
                    fr = ord(lname[k]) - ord('A')
                    rot = ord(lname[k + 1]) - ord('0')
                    if fr < 0 or fr > 28 or rot < 0 or rot > 8:
                        continue
                    f = frames.setdefault(lname[:4], {}).setdefault(
                        fr, [None] * 9)
                    f[rot] = (sprindex[lname], 1 if k == 6 else 0)

        # ---- write named lumps: textures "t0000", flats "f0000",
        #      sprites "s0000".  Lowercase names cannot clash with anything.
        for (i, name, w, h, data, masked) in texlumps:
            self.add("t%04d" % i, data)
        for i, (name, raw) in enumerate(flatlumps):
            self.add("f%04d" % i, raw)
        for i, d in enumerate(sprlumps):
            self.add("s%04d" % i, d)

        self.build_ui()
        self.build_misc()

        # ---- directory order is sorted, so lump numbers are known now
        names = sorted(self.out.keys(), key=lambda n: n.encode("ascii"))
        lumpnum = {n: i for i, n in enumerate(names)}

        for (i, name, w, h, data, masked) in texlumps:
            mask = 1
            while mask * 2 <= w:
                mask *= 2
            texinfo += self.pack("8sHHHH", name.encode("ascii"), w, h,
                                 lumpnum["t%04d" % i],
                                 (1 if masked else 0) | ((mask - 1) << 1))
        flatinfo = bytearray()
        for i, (name, raw) in enumerate(flatlumps):
            flatinfo += self.pack("8sHH", name.encode("ascii"),
                                  lumpnum["f%04d" % i], 0)
        sprdefs = bytearray()
        sprframes = bytearray()
        nframes = 0
        for spr in SPRNAMES:
            fr = frames.get(spr)
            if not fr:
                sprdefs += self.pack("HH", 0, 0)
                continue
            count = max(fr.keys()) + 1
            sprdefs += self.pack("HH", count, nframes)
            for f in range(count):
                rots = fr.get(f)
                lumps = [0xffff] * 8
                flip = 0
                rotate = 0
                if rots:
                    if rots[0] is not None:
                        lumps = [lumpnum["s%04d" % rots[0][0]]] * 8
                        flip = 0xff if rots[0][1] else 0
                    if any(r is not None for r in rots[1:]):
                        rotate = 1
                        for r in range(8):
                            if rots[r + 1] is not None:
                                lumps[r] = lumpnum["s%04d" % rots[r + 1][0]]
                                if rots[r + 1][1]:
                                    flip |= 1 << r
                                else:
                                    flip &= ~(1 << r)
                sprframes += self.pack("BB8H", rotate, flip & 0xff, *lumps)
                nframes += 1

        # These four are added after lump numbers were assigned, so they
        # must not change the sort order of anything referenced by number:
        # all of them have names that sort *after* every other lump
        # ('~' is the last printable ASCII character).
        info = self.pack("HBBHHHHHH", VERSION, self.gamemode, self.shift,
                         numtextures, len(flatlumps), len(SPRNAMES), nframes,
                         len(self.maps), self.flatmap["F_SKY1"])
        self.add("~CDINFO", info)
        self.add("~TEXINFO", texinfo)
        self.add("~FLTINFO", flatinfo)
        self.add("~SPRDEFS", sprdefs)
        self.add("~SPRFRAM", sprframes if sprframes else b"\0\0")

        self.write()
        return 0

    # -- colormap, demos, texts --------------------------------------------

    def build_misc(self):
        cm = bytearray()
        for level in range(32):
            for g in range(64):
                cm.append((g * (32 - level) * 2 + 32) // 64)
        for g in range(64):          # invulnerability
            cm.append(63 - g)
        cm += bytes(64)              # all black
        self.add("COLORMAP", cm)

        for i in range(1, 5):
            d = self.ws.get("DEMO%d" % i)
            if d is None or len(d) < 14 or d[0] != 109:
                continue
            if self.commercial:
                mapname = "MAP%02d" % d[3]
            else:
                mapname = "E%dM%d" % (d[2], d[3])
            if mapname in self.maps:
                self.add("DEMO%d" % i, d)

    # -- user interface art -------------------------------------------------

    def ui_patch(self, name, img):
        self.add(name, self.encode_patch(img))

    def fullscreen(self, name, src=None, contrast=1.0):
        d = self.ws.get(src or name)
        if d is None:
            return False
        try:
            img = decode_patch(d, self.gray)
        except (struct.error, IndexError):
            return False
        if img.w != 320:
            out = scale_ui(img, 0.4, 0.32)
        else:
            out = resample(img, 128, 64)
        stretch_contrast(out, contrast)
        for row in out.pix:
            for x, v in enumerate(row):
                if v is None:
                    row[x] = 0
        precompensate(out)
        out.left = out.top = 0
        self.ui_patch(name, out)
        return True

    def build_ui(self):
        # HUD / menu font: opaque 4x6 cells (white box, black glyph)
        for ch, rows in FONT3X5.items():
            self.ui_patch("STCFN%03d" % ord(ch), glyph_image(rows, True))
        # big status bar numbers (transparent background)
        for ch in "0123456789":
            self.ui_patch("STTNUM" + ch, glyph_image(BIGNUM[ch], False, 1, 0))
        self.ui_patch("STTMINUS", glyph_image(BIGNUM['-'], False, 1, 0))
        self.ui_patch("STTPRCNT", glyph_image(BIGNUM['%'], False, 1, 0))
        # small numbers: "owned" digits and a dot for "not owned"
        for ch in "0123456789":
            self.ui_patch("STYSNUM" + ch,
                          glyph_image(FONT3X5[ch], False, 1, 0))
            self.ui_patch("STGNUM" + ch,
                          glyph_image("... ... .#. ... ...", False, 1, 0))
            self.ui_patch("AMMNUM" + ch, glyph_image(FONT3X5[ch], True))
        # keys: 0-2 cards, 3-5 skulls
        for i in range(3):
            self.ui_patch("STKEYS%d" % i, rows_image(["####", "####"]))
            self.ui_patch("STKEYS%d" % (i + 3),
                          rows_image(["#..#", "####"]))
        # status bar background, 128x10
        bar = Image(128, 10, WHITE)
        for x in range(128):
            bar.pix[0][x] = BLACK
        for x in (17, 40, 66, 80, 103, 110):
            for y in range(1, 10):
                bar.pix[y][x] = BLACK
        self.ui_patch("STBAR", bar)
        # faces
        for name in list(self.ws.byname.keys()):
            if name.startswith("STF") and len(name) > 4:
                d = self.ws.get(name)
                try:
                    img = decode_patch(d, self.gray)
                except (struct.error, IndexError):
                    continue
                if img.w > 64 or img.h > 64:
                    continue
                # paste on the 24x29-ish cell, then scale to 11x9
                cell = Image(35, 31)
                cell.blit(img, 5 - img.left, 0 - img.top)
                small = resample(cell, 12, 9)
                stretch_contrast(small, 1.0)
                precompensate(small)
                self.ui_patch(name, small)
        # full-screen pictures
        for name in ("TITLEPIC", "INTERPIC", "WIMAP0", "WIMAP1",
                     "WIMAP2", "BOSSBACK", "VICTORY2", "ENDPIC", "PFUB1",
                     "PFUB2"):
            self.fullscreen(name)
        if not self.ws.has("INTERPIC"):
            pass
        for i in range(7):
            d = self.ws.get("END%d" % i)
            if d:
                self.ui_patch("END%d" % i, scale_ui(
                    decode_patch(d, self.gray), 0.4, 0.32))
        d = self.ws.get("M_DOOM")
        if d:
            logo = scale_ui(decode_patch(d, self.gray), 0.4, 0.32)
            stretch_contrast(logo, 1.0)
            precompensate(logo)
            logo.left = logo.top = 0
            self.ui_patch("M_DOOM", logo)
        # menu cursor (two frames)
        self.ui_patch("M_SKULL1", rows_image(
            ["oooooo", "o#oooo", "o###oo", "o#####", "o###oo", "o#oooo"]))
        self.ui_patch("M_SKULL2", rows_image(
            ["oooooo", "oo#ooo", "oo###o", "oo####", "oo###o", "oo#ooo"]))
        # intermission markers
        self.ui_patch("WISPLAT", rows_image(
            ["#o.o#", "o#o#o", ".o#o.", "o#o#o", "#o.o#"]))
        self.ui_patch("WIURH0", rows_image(
            ["ooooooo", "o#####o", "o#ooo#o", "o#o#o#o", "o#ooo#o",
             "o#####o", "ooooooo"]))
        # the credits page is text, unreadable when scaled: retype it
        credit_lines = [
            "          D O O M",
            "    (C) 1993 ID SOFTWARE",
            "",
            "PROGRAMMING: JOHN CARMACK,",
            " JOHN ROMERO, DAVE TAYLOR",
            "ART: ADRIAN CARMACK, K. CLOUD",
            "DESIGN: SANDY PETERSEN ...",
            "",
            "CASIODOOM (C) 2026 AURASSCZ",
            "WRITTEN WITH CLAUDE CODE",
        ]
        img = Image(128, 64, WHITE)
        for i, line in enumerate(credit_lines):
            img.blit(text_image(line, False), 1, 2 + 6 * i)
        self.ui_patch("CREDIT", img)

        # help screen: the key bindings
        help_lines = [
            "CASIODOOM - CONTROLS",
            "ARROWS  MOVE / TURN",
            "SHIFT   FIRE",
            "ALPHA   USE / OPEN",
            "OPTN    STRAFE  VARS RUN",
            "F5 F6   STEP LEFT/RIGHT",
            "1-7     WEAPONS",
            "F1 MAP  F2 SAVE F3 LOAD",
            "+ -     SCREEN / MAP ZOOM",
            "EXIT    MENU    EXE OK",
        ]
        img = Image(128, 64, WHITE)
        for i, line in enumerate(help_lines):
            img.blit(text_image(line, False), 1, 2 + 6 * i)
        self.ui_patch("HELP1", img)

    # -- output ------------------------------------------------------------

    def write(self):
        names = sorted(self.out.keys(), key=lambda n: n.encode("ascii"))
        body = bytearray()
        dirent = bytearray()
        base = 16
        for n in names:
            d = self.out[n]
            while (base + len(body)) % 4:
                body.append(0)
            dirent += n.encode("ascii").ljust(8, b"\0")
            dirent += self.pack("II", base + len(body), len(d))
            body += d
        while (base + len(body)) % 16:
            body.append(0)
        hdr = b"CDWD" + self.pack("III", 0x01020304, len(names),
                                  base + len(body))
        with open(self.args.output, "wb") as f:
            f.write(hdr)
            f.write(body)
            f.write(dirent)
        total = len(hdr) + len(body) + len(dirent)

        def group(pred):
            return sum(len(self.out[n]) for n in names if pred(n))
        for l in self.log:
            print(l)
        print("wrote %s: %d bytes, %d lumps" % (self.args.output, total,
                                                 len(names)))
        print("  maps (%d): %s" % (len(self.maps), " ".join(self.maps)))
        print("  levels   %7d bytes" % group(
            lambda n: any(n.startswith(m) and len(n) == len(m) + 3
                          for m in self.maps)))
        print("  textures %7d bytes" % group(lambda n: n[0] == 't'))
        print("  flats    %7d bytes" % group(lambda n: n[0] == 'f'))
        print("  sprites  %7d bytes" % group(lambda n: n[0] == 's'))
        if total > 2800 * 1024:
            print("WARNING: the fx-9860GIII has about 3 MB of storage; "
                  "use --maps and/or --shift 2 to make the file smaller.")


def lumpname(b):
    return b.split(b"\0")[0].decode("ascii", "replace").upper()


def main():
    ap = argparse.ArgumentParser(
        description="Convert a DOOM WAD for CasioDoom (fx-9860GIII).")
    ap.add_argument("wads", nargs="+", help="IWAD followed by optional PWADs")
    ap.add_argument("-o", "--output", default="DOOM.CWD")
    ap.add_argument("--maps", default="",
                    help="maps to include, e.g. E1M1-E1M9 or MAP01,MAP02 "
                         "(default: all)")
    ap.add_argument("--shift", type=int, default=1, choices=(0, 1, 2),
                    help="texture/sprite reduction: 0 = full size, "
                         "1 = half (default), 2 = quarter")
    ap.add_argument("--gamma", type=float, default=1.3,
                    help="brightness correction of the grey ramp "
                         "(default 1.3; higher = brighter)")
    ap.add_argument("--endian", choices=("big", "little"), default="big",
                    help="big = calculator (default), little = PC test build")
    ap.add_argument("--all-textures", action="store_true",
                    help="keep textures/flats not used by the chosen maps")
    args = ap.parse_args()
    return Converter(args).run()


if __name__ == "__main__":
    sys.exit(main())
