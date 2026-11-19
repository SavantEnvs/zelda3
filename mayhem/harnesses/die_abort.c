// zelda3's Die() lives in src/main.c next to SDL window/audio setup the harnesses don't need.
// Same message as upstream, but abort() instead of exit(1) so a fatal error the input triggers is
// reported as a crash with a backtrace (docs/rules/c.md: lift exit to abort).
#include <stdio.h>
#include <stdlib.h>
#include "src/types.h"

void NORETURN Die(const char *error) {
  fprintf(stderr, "Error: %s\n", error);
  abort();
}
