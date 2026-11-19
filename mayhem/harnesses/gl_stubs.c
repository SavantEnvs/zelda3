// A null OpenGL driver for the GLSL-preset harness and the self-test. zelda3 calls OpenGL through
// the gl_core_3_1 function pointers (third_party/gl_core), which the real program fills from the
// driver after creating a GL context. There is no GPU or display here, so these stubs stand in for
// the driver: every shader compiles and links, every object gets id 1, uniforms are not found.
// They mock the environment only; all zelda3 code (preset/shader/texture parsing) runs unchanged.
#include "third_party/gl_core/gl_core_3_1.h"
#include "mayhem/harnesses/gl_stubs.h"
#include <string.h>

static void CODEGEN_FUNCPTR s_void_u(GLuint a) { (void)a; }
static void CODEGEN_FUNCPTR s_void_e(GLenum a) { (void)a; }
static void CODEGEN_FUNCPTR s_void_uu(GLuint a, GLuint b) { (void)a; (void)b; }
static void CODEGEN_FUNCPTR s_void_eu(GLenum a, GLuint b) { (void)a; (void)b; }
static GLuint CODEGEN_FUNCPTR s_create_program(void) { return 1; }
static GLuint CODEGEN_FUNCPTR s_create_shader(GLenum t) { (void)t; return 1; }
static void CODEGEN_FUNCPTR s_gen(GLsizei n, GLuint *ids) { for (GLsizei i = 0; i < n; i++) ids[i] = 1; }
static void CODEGEN_FUNCPTR s_delete(GLsizei n, const GLuint *ids) { (void)n; (void)ids; }
static void CODEGEN_FUNCPTR s_buffer_data(GLenum t, GLsizeiptr s, const void *d, GLenum u) { (void)t; (void)s; (void)d; (void)u; }
static void CODEGEN_FUNCPTR s_draw_arrays(GLenum m, GLint f, GLsizei c) { (void)m; (void)f; (void)c; }
static void CODEGEN_FUNCPTR s_fb_tex2d(GLenum a, GLenum b, GLenum c, GLuint d, GLint e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
static GLint CODEGEN_FUNCPTR s_get_location(GLuint p, const GLchar *n) { (void)p; (void)n; return -1; }
static void CODEGEN_FUNCPTR s_get_integerv(GLenum p, GLint *d) { (void)p; *d = 0; }
static void CODEGEN_FUNCPTR s_info_log(GLuint o, GLsizei n, GLsizei *len, GLchar *log) {
  (void)o; if (len) *len = 0; if (n > 0) log[0] = 0;
}
static void CODEGEN_FUNCPTR s_get_iv(GLuint o, GLenum p, GLint *v) { (void)o; (void)p; *v = GL_TRUE; }
char g_null_gl_shader_text[8192];
int g_null_gl_tex_w, g_null_gl_tex_h, g_null_gl_tex_channels;

static void CODEGEN_FUNCPTR s_shader_source(GLuint s, GLsizei c, const GLchar *const *str, const GLint *len) {
  // Read every byte the program hands to the driver, as a real driver would, and keep a
  // (truncated) copy of the last source for the self-test.
  size_t n = 0;
  (void)s;
  for (GLsizei i = 0; i < c; i++)
    for (GLint j = 0; j < len[i]; j++)
      if (n + 1 < sizeof(g_null_gl_shader_text)) g_null_gl_shader_text[n++] = str[i][j];
  g_null_gl_shader_text[n] = 0;
}
static void CODEGEN_FUNCPTR s_tex_image2d(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void *px) {
  (void)t; (void)l; (void)i; (void)b; (void)f; (void)ty;
  // Read the pixel buffer the way a driver would (RGB/RGBA/LUMINANCE, 1 byte per channel).
  int ch = f == GL_RGBA ? 4 : f == GL_RGB ? 3 : 1;
  volatile unsigned sum = 0;
  const unsigned char *p = (const unsigned char *)px;
  for (size_t k = 0; k < (size_t)w * h * ch; k++) sum += p[k];
  g_null_gl_tex_w = w, g_null_gl_tex_h = h, g_null_gl_tex_channels = ch;
}
static void CODEGEN_FUNCPTR s_tex_parami(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
static void CODEGEN_FUNCPTR s_uniform1f(GLint l, GLfloat v) { (void)l; (void)v; }
static void CODEGEN_FUNCPTR s_uniform1i(GLint l, GLint v) { (void)l; (void)v; }
static void CODEGEN_FUNCPTR s_uniform2fv(GLint l, GLsizei c, const GLfloat *v) { (void)l; (void)c; (void)v; }
static void CODEGEN_FUNCPTR s_uniform_m4(GLint l, GLsizei c, GLboolean t, const GLfloat *v) { (void)l; (void)c; (void)t; (void)v; }
static void CODEGEN_FUNCPTR s_vattrib(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p) { (void)i; (void)s; (void)t; (void)n; (void)st; (void)p; }
static void CODEGEN_FUNCPTR s_viewport(GLint x, GLint y, GLsizei w, GLsizei h) { (void)x; (void)y; (void)w; (void)h; }

void InstallNullGlDriver(void) {
  _ptrc_glActiveTexture = s_void_e;
  _ptrc_glAttachShader = s_void_uu;
  _ptrc_glBindBuffer = s_void_eu;
  _ptrc_glBindFramebuffer = s_void_eu;
  _ptrc_glBindTexture = s_void_eu;
  _ptrc_glBufferData = s_buffer_data;
  _ptrc_glCompileShader = s_void_u;
  _ptrc_glCreateProgram = s_create_program;
  _ptrc_glCreateShader = s_create_shader;
  _ptrc_glDeleteBuffers = s_delete;
  _ptrc_glDeleteFramebuffers = s_delete;
  _ptrc_glDeleteProgram = s_void_u;
  _ptrc_glDeleteShader = s_void_u;
  _ptrc_glDeleteTextures = s_delete;
  _ptrc_glDisable = s_void_e;
  _ptrc_glDisableVertexAttribArray = s_void_u;
  _ptrc_glDrawArrays = s_draw_arrays;
  _ptrc_glEnable = s_void_e;
  _ptrc_glEnableVertexAttribArray = s_void_u;
  _ptrc_glFramebufferTexture2D = s_fb_tex2d;
  _ptrc_glGenBuffers = s_gen;
  _ptrc_glGenerateMipmap = s_void_e;
  _ptrc_glGenFramebuffers = s_gen;
  _ptrc_glGenTextures = s_gen;
  _ptrc_glGetAttribLocation = s_get_location;
  _ptrc_glGetIntegerv = s_get_integerv;
  _ptrc_glGetProgramInfoLog = s_info_log;
  _ptrc_glGetProgramiv = s_get_iv;
  _ptrc_glGetShaderInfoLog = s_info_log;
  _ptrc_glGetShaderiv = s_get_iv;
  _ptrc_glGetUniformLocation = s_get_location;
  _ptrc_glLinkProgram = s_void_u;
  _ptrc_glShaderSource = s_shader_source;
  _ptrc_glTexImage2D = s_tex_image2d;
  _ptrc_glTexParameteri = s_tex_parami;
  _ptrc_glUniform1f = s_uniform1f;
  _ptrc_glUniform1i = s_uniform1i;
  _ptrc_glUniform2fv = s_uniform2fv;
  _ptrc_glUniformMatrix4fv = s_uniform_m4;
  _ptrc_glUseProgram = s_void_u;
  _ptrc_glVertexAttribPointer = s_vattrib;
  _ptrc_glViewport = s_viewport;
}
