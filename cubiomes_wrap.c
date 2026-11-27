#include <string.h>
#include <math.h>
#include <emscripten.h>
#include "generator.h"
#include "finders.h"
#include "util.h"

static Generator g;
static int mc;
static uint64_t sd;
static int cache[128 * 128];
static unsigned char buf[128 * 128];
static unsigned char cols[256][3];
static int sout[2 * 16384];
static float hgrid[33 * 33];
#ifdef HAVE_mapApproxHeight
static SurfaceNoise sn;
#endif

/* Solo se compilan las estructuras que existen en la librería (build.yml define HAVE_xxx) */
static const struct { int t; const char *k; } S[] = {
#ifdef HAVE_Village
    {Village, "Village"},
#endif
#ifdef HAVE_Desert_Pyramid
    {Desert_Pyramid, "Desert_Pyramid"},
#endif
#ifdef HAVE_Jungle_Pyramid
    {Jungle_Pyramid, "Jungle_Pyramid"},
#endif
#ifdef HAVE_Swamp_Hut
    {Swamp_Hut, "Swamp_Hut"},
#endif
#ifdef HAVE_Igloo
    {Igloo, "Igloo"},
#endif
#ifdef HAVE_Monument
    {Monument, "Monument"},
#endif
#ifdef HAVE_Mansion
    {Mansion, "Mansion"},
#endif
#ifdef HAVE_Outpost
    {Outpost, "Outpost"},
#endif
#ifdef HAVE_Ancient_City
    {Ancient_City, "Ancient_City"},
#endif
#ifdef HAVE_Ruined_Portal
    {Ruined_Portal, "Ruined_Portal"},
#endif
#ifdef HAVE_Shipwreck
    {Shipwreck, "Shipwreck"},
#endif
#ifdef HAVE_Ocean_Ruin
    {Ocean_Ruin, "Ocean_Ruin"},
#endif
#ifdef HAVE_Trail_Ruins
    {Trail_Ruins, "Trail_Ruins"},
#endif
#ifdef HAVE_Trial_Chambers
    {Trial_Chambers, "Trial_Chambers"},
#endif
#ifdef HAVE_Treasure
    {Treasure, "Treasure"},
#endif
#ifdef HAVE_Abandoned_Camp
    {Abandoned_Camp, "Abandoned_Camp"},
#endif
    {-1, ""}};
#define NT ((int)(sizeof(S) / sizeof(S[0])) - 1)

EMSCRIPTEN_KEEPALIVE int init(const char *ver, int lo, int hi) {
  mc = strcmp(ver, "newest") == 0 ? MC_NEWEST : str2mc(ver);
  if (mc < 0) return -1;
  sd = ((uint64_t)(uint32_t)hi << 32) | (uint32_t)lo;
  setupGenerator(&g, mc, 0);
  applySeed(&g, DIM_OVERWORLD, sd);
  initBiomeColors(cols);
#ifdef HAVE_mapApproxHeight
  initSurfaceNoise(&sn, DIM_OVERWORLD, sd);
#endif
  return mc;
}
EMSCRIPTEN_KEEPALIVE const char *mc_name() { return mc2str(mc); }
EMSCRIPTEN_KEEPALIVE int st_count() { return NT; }
EMSCRIPTEN_KEEPALIVE const char *st_key(int i) { return S[i].k; }

EMSCRIPTEN_KEEPALIVE unsigned char *tile(int scale, int x, int z) {
  Range r = {scale, x, z, 128, 128, scale == 1 ? 63 : 15, 1};
  genBiomes(&g, cache, r);
  for (int i = 0; i < 128 * 128; i++) buf[i] = (unsigned char)cache[i];
  return buf;
}
EMSCRIPTEN_KEEPALIVE int biome_at(int x, int z) { return getBiomeAt(&g, 1, x, 63, z); }
EMSCRIPTEN_KEEPALIVE const char *bname(int id) { return biome2str(mc, id); }
EMSCRIPTEN_KEEPALIVE unsigned char *cols_ptr() { return &cols[0][0]; }
EMSCRIPTEN_KEEPALIVE int spawn_x() { return getSpawn(&g).x; }
EMSCRIPTEN_KEEPALIVE int spawn_z() { return getSpawn(&g).z; }
EMSCRIPTEN_KEEPALIVE int *find_ptr() { return sout; }

EMSCRIPTEN_KEEPALIVE int find_structs(int idx, int x0, int z0, int x1, int z1) {
  if (idx < 0 || idx >= NT) return 0;
  int st = S[idx].t;
  StructureConfig sc;
  if (!getStructureConfig(st, mc, &sc)) return 0;
  int rb = sc.regionSize * 16, n = 0;
  int rx0 = (int)floor((double)x0 / rb), rx1 = (int)floor((double)x1 / rb);
  int rz0 = (int)floor((double)z0 / rb), rz1 = (int)floor((double)z1 / rb);
  for (int rz = rz0; rz <= rz1; rz++)
    for (int rx = rx0; rx <= rx1; rx++) {
      Pos p;
      if (!getStructurePos(st, mc, sd, rx, rz, &p)) continue;
      if (p.x < x0 || p.x > x1 || p.z < z0 || p.z > z1) continue;
      if (!isViableStructurePos(st, &g, p.x, p.z, 0)) continue;
      if (n < 16384) { sout[2 * n] = p.x; sout[2 * n + 1] = p.z; n++; }
    }
  return n;
}

/* Relieve aproximado: rejilla 33x33 por tile (separación 4*s bloques) */
EMSCRIPTEN_KEEPALIVE int h_available() {
#ifdef HAVE_mapApproxHeight
  return mc >= MC_1_18;
#else
  return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE float *htile(int s, int tx, int tz) {
#ifdef HAVE_mapApproxHeight
  static int ids[33 * 33];
  if (s == 1) {
    mapApproxHeight(hgrid, ids, &g, &sn, tx * 32, tz * 32, 33, 33);
  } else {
    for (int j = 0; j < 33; j++)
      for (int i = 0; i < 33; i++) {
        float y = 63; int id = 0;
        mapApproxHeight(&y, &id, &g, &sn, tx * 32 * s + i * s, tz * 32 * s + j * s, 1, 1);
        hgrid[j * 33 + i] = y;
      }
  }
#else
  for (int i = 0; i < 33 * 33; i++) hgrid[i] = 63;
#endif
  return hgrid;
}
