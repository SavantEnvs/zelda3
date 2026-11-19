// Fuzzes zelda3's GLSL shader loading: GlslShader_CreateFromFile() (src/glsl_shader.c), what the
// game runs for the `Shader = <file>` config key with OutputMethod=OpenGL. It parses the .glslp
// preset (passes, scale/wrap/filter keys, textures, parameters), reads each pass's shader source
// with GlslShader_ReadShaderFile (#include, #pragma parameter), prepares the source for the GL
// compiler (LengthOfInitialComments, #version handling) and decodes each texture PNG (stb_image).
//
// A shader pack is a directory of files that name each other, so the one fuzz input is staged as
// all three kinds of file: preset.glslp, s.glsl and t.png in a scratch directory. A preset that says
// `shader0 = s.glsl` or `tex_filename = t.png` (see the seeds) makes the same bytes feed the
// shader-source and PNG paths too. OpenGL itself is replaced by a null driver (gl_stubs.c).
#include "fuzz_common.h"
#include "mayhem/harnesses/gl_stubs.h"
#include "src/glsl_shader.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc; (void)argv;
  InstallNullGlDriver();
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  free(FuzzWriteFile("s.glsl", data, size));
  free(FuzzWriteFile("t.png", data, size));
  char *path = FuzzWriteFile("preset.glslp", data, size);
  GlslShader *gs = GlslShader_CreateFromFile(path, false);
  if (gs) GlslShader_Destroy(gs);
  free(path);
  return 0;
}
