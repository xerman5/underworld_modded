/* target: ovr142 */
/* opts: -mm -1 -G -O -Y -d */
/* LIGHTING.C: the light level of the 3D view. The whole of UW1's DOS overlay ovr142
   (UW2's ovr152), in original order. Seeded from UW2Decomp's src/map/LIGHTING.C.

   The renderer shades through cLightTabs, 16 colour maps of 256 bytes (the first maps
   colours to themselves, the last to near black). init_lighting fills it from
   DATA\LIGHT.DAT at start-up and fills cXfer, the translation tables in seg003, from
   DATA\XFER.DAT. set_light picks a light level: it reads that level's row of
   DATA\SHADES.DAT (six words: smooth_div, smooth_base, smooth_lowpass, curvrad,
   distpoly, dist8), and swaps in DATA\MONO.DAT's grey maps for level 5. random_light is
   the mushroom effect.

   UW1 against UW2: no lget (each function opens, reads and closes the file itself, by
   its DATA\ path, rather than through our_open); XFER.DAT is 0x600 bytes (UW2: 0x500).

   name: UW2's (the FM Towns names), which also keep the EXE's overlay stub order
   (init_lighting 281, set_light 755, random_light 946).

   Entry points: init_lighting (UWEDIT.C's init_world); set_light (PLAYDATA.C's
   FixPlayerEquips, the light spells and Wizard Eye; PLAYER.C at start-up and for the
   crystal ball); random_light (PLAYDATA.C's set_drugged). Data owned: cur_light_level;
   the tables it fills are the renderer's (cLightTabs, cXfer, the shading globals). */

#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <mem.h>
#include "file.h"
#include "gfx.h"
#include "map.h"
#include "player.h"
#include "sys.h"
#include "view3d.h"

/* The light level set_light last loaded, 5 for mono.dat; 0xFF until the first call. */
/* name: FM Towns _cur_light_level. DS:1C50, the first byte of this file's _DATA. */
unsigned char cur_light_level = 0xFF;

#ifndef __TURBOC__
static void port_light_loaded(void);    /* port only: --enhance long-light, at the end of the file */
static void port_light_tail(void);
static void port_long_light(int level);
static char port_light_drugged;       /* random_light's maps are in: leave them be */
#endif

/* Sets light level lightLevel (SHADES.DAT has eight rows, 0 darkest to 7 brightest).
   Does nothing if that level is already set. Leaving level 5 reloads LIGHT.DAT; entering
   it loads MONO.DAT. Then copies the level's row into the shading globals, has the
   vision grid rebuilt for the new radius (preset_grid) and flags the view for redrawing
   (editchng(2)). */
void far set_light(char lightLevel)
{
    int diValue;
    int16 ShadesDataRow_var_C[6];
    register int handle;
    register int shades;

    if (cur_light_level == lightLevel)
        return;
    if (cur_light_level == 5) {
        if ((handle = open("DATA\\light.dat", O_RDONLY | O_BINARY)) != -1) {
            intoFarBuffer_ovr167_5DA(handle, cLightTabs, 0x1000);
            close(handle);
#ifndef __TURBOC__
            port_light_loaded();
#endif
        }
    } else if (lightLevel == 5) {
        if ((handle = open("DATA\\mono.dat", O_RDONLY | O_BINARY)) != -1) {
            intoFarBuffer_ovr167_5DA(handle, cLightTabs, 0x1000);
            close(handle);
#ifndef __TURBOC__
            port_light_loaded();
#endif
        }
    }
    cur_light_level = lightLevel;
    shades = open("DATA\\shades.dat", O_RDONLY | O_BINARY);
    if (shades < 0)
        return;
    diValue = lightLevel * 12;
    lseek(shades, diValue, 0);
    read(shades, ShadesDataRow_var_C, 12);
    smooth_div = ShadesDataRow_var_C[0];
    if (smooth_div <= 1)
        smooth_div = 1;
    smooth_base = ShadesDataRow_var_C[1];
    smooth_lowpass = ShadesDataRow_var_C[2];
    curvrad = ShadesDataRow_var_C[3];
    distpoly = ShadesDataRow_var_C[4];
    dist8 = ShadesDataRow_var_C[5];
    close(shades);
#ifndef __TURBOC__
    port_long_light(lightLevel);
#endif
    preset_grid(curvrad);
    editchng(2);
}

/* Start-up (UWEDIT.C): LIGHT.DAT into cLightTabs and XFER.DAT into cXfer. */
void far init_lighting(void)
{
    register int handle;

    if ((handle = open("DATA\\light.dat", O_RDONLY | O_BINARY)) != -1) {
        intoFarBuffer_ovr167_5DA(handle, cLightTabs, 0x1000);
        close(handle);
#ifndef __TURBOC__
        port_light_loaded();
#endif
    }
    if ((handle = open("DATA\\xfer.dat", O_RDONLY | O_BINARY)) != -1) {
        intoFarBuffer_ovr167_5DA(handle, cXfer, 0x600);
        close(handle);
    }
}

/* The mushroom effect. enabled: replace the 16 light maps with 4 KB of low memory, then
   set the first two entries of each map to 0. The source is segment 0 at an offset
   equal to the byte stored at seg051:28A0 (a value read, not an address), so the maps
   are copied from the interrupt vector table and what follows it. Not enabled: reload
   LIGHT.DAT, or MONO.DAT at level 5. */
void far random_light(char enabled)
{
    register int i;
    register int handle;

    if (enabled != 0) {
        /* match: the EXE reads the byte for the segment too and then discards it
           (mov al,0), which is what the byte times 0 compiles to */
        movedata(seg051_28A0 * 0, seg051_28A0, FP_SEG(cLightTabs), FP_OFF(cLightTabs), 0x1000);
        for (i = 0; i < 16; i++) {
            cLightTabs[i * 256] = 0;
            ((char far *)MK_FP(FP_SEG(cLightTabs), FP_OFF(cLightTabs)))[i * 256 + 1] = 0;
        }
#ifndef __TURBOC__
        port_light_drugged = 1;
#endif
    } else {
        if (cur_light_level != 5)
            handle = open("DATA\\light.dat", O_RDONLY | O_BINARY);
        else
            handle = open("DATA\\mono.dat", O_RDONLY | O_BINARY);
        if (handle != -1) {
            intoFarBuffer_ovr167_5DA(handle, cLightTabs, 0x1000);
            close(handle);
#ifndef __TURBOC__
            port_light_loaded();
#endif
        }
#ifndef __TURBOC__
        port_light_drugged = 0;
        port_light_tail();
#endif
    }
}

#ifndef __TURBOC__
/* Port only: --enhance long-light (docs/ENHANCEMENTS.md), ours. With any light but none, the light
   reaches further (port_reach, within the vision grid's 16 rows) and fades to black at its edge.
   The original cuts there from its darkest map, about a fifth of full brightness, straight to the
   undrawn black beyond. So the distance shading slows (smooth_div, which the renderer's vertices
   and GAMESORT.C's objects use) to reach map 15 at the new radius, and maps 11 to 15 are made
   darker, down to black. The original's radius and shading are kept for the vision grid's cell
   shades (VIEW3D.C's preset_grid) and the automap (GRIDDB.C's port_grdb_elem), so that what is
   mapped, and the experience for it, stay the original's. */
#include <string.h>

int16 port_light_reach0 = 16;           /* the original's radius; 16 until set_light, none wider */
int16 port_light_div0, port_light_base0, port_light_low0;   /* and its shading */
static unsigned char port_light_maps[0x1000];   /* LIGHT.DAT or MONO.DAT as loaded */
static char port_light_have;            /* and they have been */

/* the radius by light level: none (0) as the original, 3; the original's 3 to 7 widened by about
   two thirds */
static const char port_reach[8] = { 3, 5, 6, 6, 8, 10, 12, 12 };
/* maps 11 to 15, as percentages of map 0's colours (the original's are about 37 to 14) */
static const unsigned char port_tail_pct[5] = { 34, 25, 16, 8, 2 };

/* after LIGHT.DAT or MONO.DAT is loaded: keep it, and darken its tail for the level set */
static void port_light_loaded(void)
{
    memcpy(port_light_maps, (unsigned char *)cLightTabs, sizeof port_light_maps);
    port_light_have = 1;
    port_light_tail();
}

/* Maps 11 to 15: the loaded ones with no light or the flag off. With it, each colour of map 0
   (itself, or MONO.DAT's grey) scaled to port_tail_pct and matched to the nearest of the colours
   the loaded maps 14 and 15 use: the palette's dark ramps and black, none of its cycling colours.
   PALS.DAT's first palette, the 3D view's, is read the first time. */
static void port_light_tail(void)
{
    static unsigned char pal[768];
    static int have_pal;
    unsigned char *tabs = (unsigned char *)cLightTabs;
    unsigned char cand[256], seen[256];
    int n, k, c, i, best, handle;
    int32 r, g, b, dr, dg, db, dist, bestdist;

    if (port_light_drugged || !port_light_have)   /* init_player's set_light(0) can come first */
        return;
    if (!ENHANCED(ENH_LONG_LIGHT) || cur_light_level == 0 || cur_light_level == 0xFF) {
        memcpy(tabs + 11 * 256, port_light_maps + 11 * 256, 5 * 256);
        return;
    }
    if (!have_pal) {
        if ((handle = open("DATA\\pals.dat", O_RDONLY | O_BINARY)) == -1)
            return;
        have_pal = read(handle, pal, sizeof pal) == sizeof pal;
        close(handle);
        if (!have_pal)
            return;
    }
    memset(seen, 0, sizeof seen);
    for (n = 0, i = 14 * 256; i < 16 * 256; i++)
        if (!seen[port_light_maps[i]]) {
            seen[port_light_maps[i]] = 1;
            cand[n++] = port_light_maps[i];
        }
    for (k = 0; k < 5; k++)
        for (c = 0; c < 256; c++) {
            r = pal[port_light_maps[c] * 3] * port_tail_pct[k];
            g = pal[port_light_maps[c] * 3 + 1] * port_tail_pct[k];
            b = pal[port_light_maps[c] * 3 + 2] * port_tail_pct[k];
            best = cand[0];
            bestdist = 0x7FFFFFFFL;
            for (i = 0; i < n; i++) {   /* weighted for brightness, under 2^31 at most */
                dr = pal[cand[i] * 3] * 100 - r;
                dg = pal[cand[i] * 3 + 1] * 100 - g;
                db = pal[cand[i] * 3 + 2] * 100 - b;
                dist = 3 * dr * dr + 6 * dg * dg + db * db;
                if (dist < bestdist) {
                    bestdist = dist;
                    best = cand[i];
                }
            }
            tabs[(11 + k) * 256 + c] = (unsigned char)best;
        }
}

/* set_light, after reading the level's row: keep the original's radius and shading, then widen
   the radius and slow the shading to reach map 15 at it (the renderer's vertex shade is the
   distance, 8 a tile, times smooth_div / 64, plus smooth_lowpass, then smooth_base) */
static void port_long_light(int level)
{
    int reach;

    port_light_reach0 = curvrad;
    port_light_div0 = smooth_div;
    port_light_base0 = smooth_base;
    port_light_low0 = smooth_lowpass;
    if (ENHANCED(ENH_LONG_LIGHT) && level > 0 && level < 8 && (reach = port_reach[level]) > curvrad) {
        curvrad = reach;
        smooth_div = ((15 - smooth_lowpass - smooth_base) * 8 + reach / 2) / reach;
    }
    port_light_tail();
}
#endif
