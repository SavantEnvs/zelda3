// Shared helpers for the zelda3 harnesses: zelda3 reads every untrusted input from a FILE path
// (config .ini, .glslp preset, shader sources, PNG textures), so the harnesses stage the fuzz input
// in a private scratch directory under $TMPDIR and call the project's own file-based entry point.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

static char g_scratch_dir[4096];

static const char *FuzzScratchDir(void) {
  if (!g_scratch_dir[0]) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(g_scratch_dir, sizeof(g_scratch_dir), "%s/zelda3-fuzz-XXXXXX", tmp);
    if (!mkdtemp(g_scratch_dir)) { perror("mkdtemp"); abort(); }
  }
  return g_scratch_dir;
}

// Writes data to <scratch>/<name>; returns a malloc'd path.
static char *FuzzWriteFile(const char *name, const uint8_t *data, size_t size) {
  const char *dir = FuzzScratchDir();
  size_t n = strlen(dir) + strlen(name) + 2;
  char *path = (char *)malloc(n);
  snprintf(path, n, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) { perror(path); abort(); }
  if (size && fwrite(data, 1, size, f) != size) { perror("fwrite"); abort(); }
  fclose(f);
  return path;
}
