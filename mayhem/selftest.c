// Known-answer self-test for the zelda3 code the fuzz targets reach: the .ini config parser
// (src/config.c), the BPS patch decoder (src/util.c ApplyBps) and the GLSL shader-preset loader
// (src/glsl_shader.c, with the null GL driver from harnesses/gl_stubs.c). Every check compares a
// parsed/decoded value against the value the fixture in mayhem/testdata/ (or the BPS patch built
// below) defines. Output: one "PASS <name>" / "FAIL <name>" line per check, then a summary line
// "SELFTEST passed=<n> failed=<n>" that mayhem/test.sh parses.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "src/config.h"
#include "src/features.h"
#include "src/util.h"
#include "src/glsl_shader.h"
#include "mayhem/harnesses/gl_stubs.h"

static int passed, failed;
#define CHECK(name, cond) do { \
    if (cond) { passed++; printf("PASS %s\n", name); } \
    else { failed++; printf("FAIL %s (%s)\n", name, #cond); } } while (0)

static uint32 Crc32(const uint8 *p, size_t n) {
  uint32 crc = 0xFFFFFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ ((crc & 1) * 0xEDB88320u);
  }
  return crc ^ 0xFFFFFFFF;
}
static size_t PutVarint(uint8 *p, uint64 v) {  // BPS number encoding
  size_t n = 0;
  for (;;) {
    uint8 x = v & 0x7f;
    v >>= 7;
    if (v == 0) { p[n++] = 0x80 | x; return n; }
    p[n++] = x;
    v--;
  }
}
static void PutLE32(uint8 *p, uint32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

static void TestConfig(const char *dir) {
  char *path = StrFmt("%s/main.ini", dir);
  ParseConfigFile(path);
  free(path);
  CHECK("config.autosave", g_config.autosave == true);
  CHECK("config.extended_aspect_16_9", g_config.extended_aspect_ratio == 71);
  CHECK("config.features_widescreen", (g_config.features0 & kFeatures0_ExtendScreen64) &&
                                      (g_config.features0 & kFeatures0_WidescreenVisualFixes));
  CHECK("config.window_size", g_config.window_width == 800 && g_config.window_height == 600);
  CHECK("config.window_scale_trailing_comment", g_config.window_scale == 4);
  CHECK("config.output_method", g_config.output_method == kOutputMethod_OpenGL);
  CHECK("config.shader_path", g_config.shader && strcmp(g_config.shader, "presets/crt.glslp") == 0);
  CHECK("config.audio_freq", g_config.audio_freq == 32000);
  CHECK("config.audio_channels", g_config.audio_channels == 1);
  CHECK("config.msu_volume", g_config.msuvolume == 75);
  CHECK("config.enable_msu_opuz", g_config.enable_msu == kMsuEnabled_Opuz);
  CHECK("config.include_audio_samples", g_config.audio_samples == 1024);
  CHECK("config.features_rupees", (g_config.features0 & kFeatures0_CarryMoreRupees) != 0);
  CHECK("config.key_w_is_up", FindCmdForSdlKey(SDLK_w, KMOD_NONE) == kKeys_Controls + 0);
  CHECK("config.key_d_is_right", FindCmdForSdlKey(SDLK_d, KMOD_NONE) == kKeys_Controls + 3);
  CHECK("config.key_shift_q", FindCmdForSdlKey(SDLK_q, KMOD_LSHIFT) == kKeys_Controls + 4);
  CHECK("config.key_unshifted_q_unbound", FindCmdForSdlKey(SDLK_q, KMOD_NONE) == 0);
}

static void TestBps(void) {
  // source "Hello, world" -> target "Hello, BPS world!": SourceRead 7, TargetRead "BPS ",
  // SourceCopy 5 from offset 7, TargetRead "!".
  static const uint8 src[] = "Hello, world";
  static const char want[] = "Hello, BPS world!";
  size_t src_len = 12, dst_len = 17;
  uint8 bps[128];
  size_t n = 0;
  memcpy(bps, "BPS1", 4), n = 4;
  n += PutVarint(bps + n, src_len);
  n += PutVarint(bps + n, dst_len);
  n += PutVarint(bps + n, 0);                       // metadata size
  n += PutVarint(bps + n, ((7 - 1) << 2) | 0);      // SourceRead 7
  n += PutVarint(bps + n, ((4 - 1) << 2) | 1);      // TargetRead 4
  memcpy(bps + n, "BPS ", 4), n += 4;
  n += PutVarint(bps + n, ((5 - 1) << 2) | 2);      // SourceCopy 5
  n += PutVarint(bps + n, (7 << 1) | 0);            //   relative offset +7
  n += PutVarint(bps + n, ((1 - 1) << 2) | 1);      // TargetRead 1
  bps[n++] = '!';
  PutLE32(bps + n, Crc32(src, src_len)), n += 4;
  PutLE32(bps + n, Crc32((const uint8 *)want, dst_len)), n += 4;
  PutLE32(bps + n, Crc32(bps, n)), n += 4;

  size_t out_len = 0;
  uint8 *out = ApplyBps(src, src_len, bps, n, &out_len);
  CHECK("bps.apply", out != NULL);
  CHECK("bps.output", out && out_len == dst_len && memcmp(out, want, dst_len) == 0);
  free(out);

  bps[11] ^= 1;  // corrupt the payload: patch CRC must reject it
  CHECK("bps.reject_bad_patch_crc", ApplyBps(src, src_len, bps, n, &out_len) == NULL);
  bps[11] ^= 1;
  static const uint8 other_src[] = "Hello, World";
  CHECK("bps.reject_wrong_source", ApplyBps(other_src, src_len, bps, n, &out_len) == NULL);
}

static GlslParam *FindParam(GlslShader *gs, const char *id) {
  for (GlslParam *p = gs->first_param; p; p = p->next)
    if (!strcmp(p->id, id)) return p;
  return NULL;
}

static void TestGlsl(const char *dir) {
  InstallNullGlDriver();
  char *path = StrFmt("%s/shader/test.glslp", dir);
  GlslShader *gs = GlslShader_CreateFromFile(path, false);
  free(path);
  CHECK("glsl.load", gs != NULL);
  if (!gs) return;
  CHECK("glsl.passes", gs->n_pass == 2);
  CHECK("glsl.pass0", !strcmp(gs->pass[1].filename, "pass0.glsl") && gs->pass[1].scale_x == 2.0f &&
                      gs->pass[1].scale_type_x == GLSL_SOURCE && gs->pass[1].filter == 0x2601 /*GL_LINEAR*/);
  CHECK("glsl.pass1", !strcmp(gs->pass[2].filename, "pass1.glsl") && gs->pass[2].scale_type_x == GLSL_VIEWPORT &&
                      gs->pass[2].wrap_mode == 0x2901 /*GL_REPEAT*/);
  CHECK("glsl.texture", gs->first_texture && !strcmp(gs->first_texture->id, "lut") &&
                        !strcmp(gs->first_texture->filename, "lut.png") && gs->first_texture->filter == 0x2601);
  CHECK("glsl.png_decoded", g_null_gl_tex_w == 3 && g_null_gl_tex_h == 2 && g_null_gl_tex_channels == 3);
  GlslParam *strength = FindParam(gs, "STRENGTH"), *gamma = FindParam(gs, "GAMMA");
  CHECK("glsl.param_preset_override", strength && strength->value == 0.25f && strength->max == 1.0f);
  CHECK("glsl.param_from_include", gamma && gamma->value > 2.19f && gamma->value < 2.21f &&
                                   gamma->min == 1.0f && gamma->max == 3.0f);
  GlslShader_Destroy(gs);
  // The last compiled source is pass1's fragment shader: default #version + FRAGMENT prefix.
  CHECK("glsl.fragment_source", strstr(g_null_gl_shader_text, "#version 330\n#define FRAGMENT\n") ==
                                g_null_gl_shader_text && strstr(g_null_gl_shader_text, "void main() {}"));
  path = StrFmt("%s/shader/pass0.glsl", dir);
  gs = GlslShader_CreateFromFile(path, false);  // a bare .glsl is a one-pass shader
  free(path);
  CHECK("glsl.single_file", gs && gs->n_pass == 1 && strstr(g_null_gl_shader_text, "#version 130\n") ==
                            g_null_gl_shader_text && strstr(g_null_gl_shader_text, "uniform float GAMMA;"));
  if (gs) GlslShader_Destroy(gs);
}

static void TestUtil(void) {
  char buf[] = "  \"quoted name\"  next";
  char *p = buf;
  char *q = NextPossiblyQuotedString(&p);
  CHECK("util.quoted", !strcmp(q, "quoted name") && !strcmp(p, "next"));
  char line[] = "  Key Name\t = value  # comment\r\nsecond";
  char *s = line;
  char *l = NextLineStripComments(&s);
  char *v = SplitKeyValue(l);
  CHECK("util.key_value", !strcmp(l, "Key Name") && v && !strcmp(v, "value") && !strcmp(s, "second"));
  bool b = false;
  CHECK("util.parse_bool", ParseBool("yes", &b) && b && ParseBool("Off", &b) && !b && !ParseBool("maybe", &b));
  char *r = ReplaceFilenameWithNewPath("a/b/c.ini", "d.ini");
  CHECK("util.replace_filename", !strcmp(r, "a/b/d.ini"));
  free(r);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "mayhem/testdata";
  TestUtil();
  TestConfig(dir);
  TestBps();
  TestGlsl(dir);
  printf("SELFTEST passed=%d failed=%d\n", passed, failed);
  return failed != 0;
}
