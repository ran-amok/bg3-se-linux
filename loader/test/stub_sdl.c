/* Stub libSDL2.so for the smoke test: the "real" SDL_Init behind the interposer. */
#include <stdint.h>

int SDL_Init(uint32_t flags)
{
    (void)flags;
    return 42;
}
