// Fuzzes zelda3's .ini configuration parser: ParseConfigFile() -> ParseOneConfigFile() ->
// HandleIniConfig / ParseKeyArray (SDL key names) / ParseGamepadArray, `!include` recursion
// and the util.c line/quote/key=value helpers. zelda3 reads zelda3.ini (or --config <file>) at
// start-up, before anything else, so the input is the config file exactly as the game reads it.
//
// src/config.c is compiled INTO this harness (#include) so the harness can reset the parser's
// file-static key/gamepad maps between inputs: upstream parses one config per process, and the
// maps would otherwise grow across inputs (each result must depend only on its input).
#include "src/config.c"
#include "fuzz_common.h"

static void ResetConfigState(void) {
  free(keymap_hash);
  keymap_hash = NULL;
  keymap_hash_size = 0;
  memset(keymap_hash_first, 0, sizeof(keymap_hash_first));
  memset(has_keynameid, 0, sizeof(has_keynameid));
  free(joymap_ents);
  joymap_ents = NULL;
  joymap_size = 0;
  memset(joymap_first, 0, sizeof(joymap_first));
  has_joypad_controls = false;
  // g_config string fields point into the per-file buffers ParseOneConfigFile allocates and keeps
  // for the process lifetime (upstream never frees them); dropping them is a leak, not a reuse.
  memset(&g_config, 0, sizeof(g_config));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ResetConfigState();
  char *path = FuzzWriteFile("zelda3.ini", data, size);
  ParseConfigFile(path);
  // Exercise the lookups the game performs on every key/gamepad event with the parsed maps.
  static const SDL_Keycode kProbe[] = { SDLK_UP, SDLK_RETURN, SDLK_x, SDLK_F1, SDLK_KP_0, SDLK_TAB };
  volatile int sink = 0;
  for (size_t i = 0; i < sizeof(kProbe) / sizeof(kProbe[0]); i++) {
    sink += FindCmdForSdlKey(kProbe[i], KMOD_NONE);
    sink += FindCmdForSdlKey(kProbe[i], KMOD_LSHIFT | KMOD_LCTRL);
  }
  for (int b = 0; b < kGamepadBtn_Count; b++)
    sink += FindCmdForGamepadButton(b, 0);
  free(path);
  return 0;
}
