// The definitions src/main.c provides to the rest of the game, for harnesses that link the game
// code without main.c (which opens the SDL window/audio device and needs the ROM-derived
// zelda3_assets.dat). The asset tables stay empty: the MSU path the harness drives reads none.
#include "src/types.h"
#include "src/assets.h"
#include "src/util.h"

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];
// main.c serializes the game thread and the SDL audio callback with a mutex; harnesses are
// single-threaded.
void ZeldaApuLock(void) {}
void ZeldaApuUnlock(void) {}

// Same as src/main.c: look up entry idx of an index-table asset.
MemBlk FindInAssetArray(int asset, int idx) {
  return FindIndexInMemblk((MemBlk) { g_asset_ptrs[asset], g_asset_sizes[asset] }, idx);
}
