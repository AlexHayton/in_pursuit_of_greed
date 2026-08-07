"""Which lump is which.

Classification is driven by the archive's own marker lumps wherever they exist,
never by sniffing the data.  Sniffing does not work here: decode_sprite accepts
plenty of lumps that are really pics (any lump whose first two int16s happen to
look like a plausible leftoffset/width), so a heuristic classifier silently
mangles the weapon art.  The markers are authoritative and R_public.c:120-126
already trusts them.

Layout of GREED.BLO, established by walking the markers and validating every
lump against its decoder:

       0        'soundeffects' marker
       1..90    sound effects                      (not graphics)
      91        'palette'      768 bytes, the game palette, 6-bit VGA levels
      92        'lights'       64 colormaps x 256  (not graphics)
      93..158   'map' / level layouts / 'sux' scripts
     159        'transparency' 255 x 256 blend table
     160        'warplights'   another 64 colormaps
     161..164   backdrop / backdrop2 and one alternate each: 256x128 pics
     165..167   font1..font3
     168..171   statbar1..statbar4                 pics
     172..225   weapon frames                      pics
     226        'STARTDEMAND'
     227..1364  demand-loaded monster sprites
     1365       'ENDDEMAND'
     1366       'startsprites'
     1367..1736 resident sprites
     1737       'endsprites'
     1738       'startflats'
     1739..1970 flats, 64x64 raw
     1971       'endflats'
     1972       'startwalls'
     1973..2234 walls, 64 wide (door_1..door_7 are the last seven)
     2235       'endwalls'
     2236..2331 menu / briefing / logo art

The last region has a wrinkle worth knowing before quantizing anything: every
320x200 full-screen image is immediately followed by a 768-byte palette lump of
its own.  Those screens are drawn under their own palette, not the game's, so
they must be requantized against the palette that follows them -- using lump 91
would wreck them.
"""

PALETTE_SIZE = 768

# Everything before the first graphics is sounds, maps, tables.
FIRST_GRAPHIC = 161

# Regions the markers do not cover, as (start, stop_exclusive, class).
STATIC_REGIONS = [
    (161, 165, "pic"),      # backdrops
    (165, 168, "font"),
    (168, 226, "pic"),      # status bar + weapons
    (2236, None, "pic"),    # menus, briefings, logos (plus loose palettes)
]


def classify(archive):
    """-> {class: [lump index, ...]} covering every graphic lump.

    Classes: 'wall', 'flat', 'sprite', 'pic', 'font'.  Marker lumps, sounds,
    maps, colormaps and the loose palettes are all left out.
    """
    groups = {c: [] for c in ("wall", "flat", "sprite", "pic", "font")}

    groups["wall"] = list(archive.range("startwalls", "endwalls"))
    groups["flat"] = list(archive.range("startflats", "endflats"))
    groups["sprite"] = (list(archive.range("STARTDEMAND", "ENDDEMAND"))
                        + list(archive.range("startsprites", "endsprites")))

    for start, stop, cls in STATIC_REGIONS:
        stop = len(archive) if stop is None else stop
        for i in range(start, stop):
            lump = archive[i]
            # The loose per-screen palettes live in the last region; they are
            # data, not art, and are carried through untouched.
            if cls == "pic" and len(lump.data) == PALETTE_SIZE:
                continue
            if not lump.data:
                continue
            groups[cls].append(i)

    return groups


def screen_palettes(archive):
    """-> {pic lump index: palette lump index} for art with its own palette.

    A 768-byte lump immediately after a pic is that pic's palette; this is how
    the briefing screens, character portraits and logos are stored.  Anything
    not in this map uses the game palette (lump 'palette').
    """
    out = {}
    for i in range(FIRST_GRAPHIC, len(archive) - 1):
        if len(archive[i + 1].data) != PALETTE_SIZE:
            continue
        if len(archive[i].data) <= PALETTE_SIZE:
            continue
        out[i] = i + 1
    return out


def demand_range(archive):
    """The monster sprites, which LoadTextures frees and reloads per level."""
    return archive.range("STARTDEMAND", "ENDDEMAND")
