// Fuzzes zelda3's MSU-1 audio track reader (src/audio.c): MsuPlayer_Open (file tag, repeat point,
// .opuz range table) and MsuPlayer_Mix (range-table seeks, packet framing, the bundled Opus
// decoder in third_party/opus-1.3.1-stripped, PCM looping, volume mixing). With `EnableMSU` set in
// the config, the game opens `<MSUPath><track>.opuz` / `.pcm` whenever a music track starts; MSU
// packs are downloaded by users, so the track file is untrusted.
//
// Driven the way the game drives it: ZeldaEnableMsu() (what main.c does for EnableMSU),
// ZeldaPlayMsuAudioTrack(track) (what the game's music code calls), then the mixing call that the
// audio callback ZeldaRenderAudio() makes, MsuPlayer_Mix(&g_msu_player, ...), for a bounded number
// of 1024-sample buffers. src/audio.c is compiled into this harness so it can reach that static
// player and reset it between inputs; the SPC/DSP half of ZeldaRenderAudio needs the game's ROM
// assets and is not called.
//
// Input: byte 0 bit 0 = format (1 = .opuz Opus, 0 = .pcm), bit 1 = MSU-Deluxe track remapping;
// byte 1 = music track (1..47); the rest is the track file.
#include "src/audio.c"
#include "fuzz_common.h"

static char *g_msu_prefix;

int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc; (void)argv;
  const char *dir = FuzzScratchDir();
  size_t n = strlen(dir) + 16;
  g_msu_prefix = (char *)malloc(n);
  snprintf(g_msu_prefix, n, "%s/msu-", dir);
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2) return -1;
  bool opuz = data[0] & 1;
  uint8 enable = (opuz ? kMsuEnabled_Opuz : kMsuEnabled_Msu) | (data[0] & 2 ? kMsuEnabled_MsuDeluxe : 0);
  uint8 track = 1 + data[1] % 47;
  // Reset all state an earlier input could have left: the player, game RAM (resume info, music
  // module/area variables) and the config fields audio.c reads.
  memset(&g_msu_player, 0, sizeof(g_msu_player));
  memset(g_ram, 0, sizeof(g_ram));
  memset(&g_config, 0, sizeof(g_config));
  g_config.audio_freq = opuz ? 48000 : 44100;
  g_config.msuvolume = 100;
  g_config.msu_path = g_msu_prefix;

  char name[32];
  snprintf(name, sizeof(name), "msu-%d.%s", track, opuz ? "opuz" : "pcm");
  char *path = FuzzWriteFile(name, data + 2, size - 2);

  ZeldaEnableMsu(enable);
  ZeldaPlayMsuAudioTrack(track);
  static int16 audio[1024 * 2];
  for (int i = 0; i < 64 && g_msu_player.f != NULL; i++) {
    memset(audio, 0, sizeof(audio));
    MsuPlayer_Mix(&g_msu_player, audio, 1024);
  }
  MsuPlayer_CloseFile(&g_msu_player);
  remove(path);
  free(path);
  return 0;
}
