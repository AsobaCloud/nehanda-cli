/* tests/support/memory_embed_stub.c: stubs for platform_memory_background_embed
 * so unit tests that link posix/memory.o do not need to drag in the
 * kb_client RPC chain. */
#include <stdint.h>

int platform_memory_background_embed_set_suppressed(int suppressed)
{
   (void)suppressed;
   return 0;
}

void platform_memory_background_embed(int64_t memory_id, const char *command)
{
   (void)memory_id;
   (void)command;
}
