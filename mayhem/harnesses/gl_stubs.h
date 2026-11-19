#pragma once
// Points the gl_core_3_1 function pointers at a null driver (see gl_stubs.c).
void InstallNullGlDriver(void);
// What the null driver last received (read by the self-test).
extern char g_null_gl_shader_text[8192];
extern int g_null_gl_tex_w, g_null_gl_tex_h, g_null_gl_tex_channels;
