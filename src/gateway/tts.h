/* tts.h: optional TTS (text-to-speech) dispatcher for gateway voice replies. */
#pragma once

/* Returns 1 if TTS is enabled (AIMEE_GATEWAY_TTS_PROVIDER is set), 0 otherwise. */
int tts_enabled(void);

/* Synthesize `text` to an audio file. On success, *out_path_out is set to a
 * malloc'd path string (caller must free) pointing to the audio file (wav or
 * mp3 depending on provider). Returns 0 on success, -1 on error. */
int tts_synthesize(const char *text, char **out_path_out);
