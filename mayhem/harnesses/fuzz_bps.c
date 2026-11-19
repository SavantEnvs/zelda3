// Fuzzes zelda3's BPS patch decoder ApplyBps() (src/util.c). At start-up, when zelda3_assets.dat is
// absent, LoadAssets() applies the user-supplied zelda3_assets.bps to the user-supplied zelda3.sfc:
// both files are untrusted, so the fuzzer controls both the source and the patch.
//
// Input layout: [u16 LE source length][source bytes][patch bytes]. The source length is clamped to
// what is available.
//
// CRC fields: a BPS patch ends with three CRC32s (source, target, patch). ApplyBps() rejects the
// patch unless the source CRC and the patch CRC match BEFORE it decodes anything. Whoever crafts the
// two files controls both and can always make them match, so the harness writes those two CRCs into
// the trailer (the target CRC, checked only after decoding, is left as fuzzed). Without this, a
// mutation-based fuzzer never gets past the integrity check and the decoder is never reached.
//
// Minimum patch size: ApplyBps() has no length check at all, so a patch shorter than its own 4-byte
// magic + 12-byte trailer over-reads at once (memcmp/CRC reads before any decoding). That shallow
// bug is real upstream (LoadAssets() in src/main.c passes any zelda3_assets.bps size straight to
// ApplyBps()) and is reported in the PR; the harness rejects such inputs (-1) so the fuzzer spends
// its time in the command decoder instead of re-finding it on every few mutations, which also kills
// Mayhem's start-up sanity probe. The seeds are large patches whose output buffer has slack, so most
// mutations stay in bounds; the decoder's missing bounds checks are still reachable. One seed
// (exact.bin) fills dst_size exactly and carries the right target CRC, so the decoder also runs to
// the post-decode target-CRC check and its success return; mutations of it hit the mismatch path.
//
// Coverage: ApplyBps + BpsDecodeInt + crc32 have only 24 real edges (17 + 3 + 4, unsanitized
// build). Fuzzing covers all of them except the two pre-decode CRC-mismatch returns (removed by the
// CRC fix-up above, by design) and malloc() failing; the rest of the sanitized binary's edges are
// ASan/UBSan check-failure paths. A low edge count for this target is saturation, not a stall.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "src/util.h"

static uint32_t Crc32(const uint8_t *p, size_t n) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ ((crc & 1) * 0xEDB88320u);
  }
  return crc ^ 0xFFFFFFFF;
}

static void PutLE32(uint8_t *p, uint32_t v) {
  p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2) return -1;
  size_t src_len = data[0] | (data[1] << 8);
  data += 2, size -= 2;
  if (src_len > size) src_len = size;
  // Exact-size heap copies so ASan sees any over-read past either file's end.
  uint8_t *src = (uint8_t *)malloc(src_len ? src_len : 1);
  memcpy(src, data, src_len);
  size_t bps_len = size - src_len;
  if (bps_len < 16) { free(src); return -1; }
  uint8_t *bps = (uint8_t *)malloc(bps_len ? bps_len : 1);
  memcpy(bps, data + src_len, bps_len);
  PutLE32(bps + bps_len - 12, Crc32(src, src_len));
  PutLE32(bps + bps_len - 4, Crc32(bps, bps_len - 4));
  size_t out_len = 0;
  uint8_t *out = ApplyBps(src, src_len, bps, bps_len, &out_len);
  free(out);
  free(bps);
  free(src);
  return 0;
}
