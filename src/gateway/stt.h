#pragma once
#include <stddef.h>

int stt_transcribe_file(const char *path, char *out_text, size_t out_size);
int stt_is_hallucination(const char *text);