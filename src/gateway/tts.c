/* tts.c: optional text-to-speech dispatcher for voice-reply gateway responses.
 *
 * Disabled by default. Enabled by setting AIMEE_GATEWAY_TTS_PROVIDER to a
 * supported backend name.  The produced file is a temporary audio clip that
 * callers must delete after sending.
 *
 * Supported providers (AIMEE_GATEWAY_TTS_PROVIDER):
 *   "local"  (default) — espeak-ng CLI, output to /tmp/aimee-tts-<pid>.wav
 *   "openai" — POST to /v1/audio/speech; requires OPENAI_API_KEY
 *   "kokoro" — local Kokoro TTS server (AIMEE_GATEWAY_TTS_BASE_URL)
 */
#include "tts.h"
#include "aimee.h"
#include "agent_exec.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int tts_enabled(void)
{
   const char *provider = getenv("AIMEE_GATEWAY_TTS_PROVIDER");
   return provider && provider[0] ? 1 : 0;
}

/* Build an espeak-ng command.  Returns 0 on success.
 * out_path receives the path of the produced wav file (caller must free). */
static int tts_local(const char *text, char **out_path)
{
   char tmp[256];
   snprintf(tmp, sizeof(tmp), "/tmp/aimee-tts-%d.wav", (int)getpid());

   /* espeak-ng -w <path> "<text>" */
   size_t cmd_len = strlen(text) * 2 + 64;
   char *cmd = malloc(cmd_len);
   if (!cmd)
      return -1;

   /* Basic shell-safe: replace ' with '\'' */
   char *safe = malloc(strlen(text) * 4 + 4);
   if (!safe)
   {
      free(cmd);
      return -1;
   }
   const char *s = text;
   char *d = safe;
   *d++ = '\'';
   while (*s)
   {
      if (*s == '\'')
      {
         *d++ = '\'';
         *d++ = '\\';
         *d++ = '\'';
         *d++ = '\'';
      }
      else
         *d++ = *s;
      s++;
   }
   *d++ = '\'';
   *d = '\0';

   snprintf(cmd, cmd_len, "espeak-ng -w %s %s 2>/dev/null", tmp, safe);
   free(safe);

   int rc = system(cmd);
   free(cmd);
   if (rc != 0)
   {
      LOG_WARN("tts", "espeak-ng failed (exit %d) — is espeak-ng installed?", rc);
      return -1;
   }

   *out_path = strdup(tmp);
   return *out_path ? 0 : -1;
}

static int tts_openai(const char *text, char **out_path)
{
   const char *api_key = getenv("OPENAI_API_KEY");
   if (!api_key || !api_key[0])
   {
      LOG_WARN("tts", "OPENAI_API_KEY not set");
      return -1;
   }

   const char *model = getenv("AIMEE_GATEWAY_TTS_MODEL");
   if (!model || !model[0])
      model = "tts-1";
   const char *voice = getenv("AIMEE_GATEWAY_TTS_VOICE");
   if (!voice || !voice[0])
      voice = "alloy";

   /* Build request body */
   char body[4096];
   snprintf(body, sizeof(body), "{\"model\":\"%s\",\"voice\":\"%s\",\"input\":\"%s\"}", model,
            voice, text);

   char auth[256];
   snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);

   char *response = NULL;
   int status = agent_http_post_content_type("https://api.openai.com/v1/audio/speech", auth,
                                             "application/json", body, &response, 30000, NULL);
   if (status != 200 || !response)
   {
      LOG_WARN("tts", "OpenAI TTS HTTP %d", status);
      free(response);
      return -1;
   }

   char tmp[256];
   snprintf(tmp, sizeof(tmp), "/tmp/aimee-tts-%d.mp3", (int)getpid());
   FILE *f = fopen(tmp, "wb");
   if (!f)
   {
      free(response);
      return -1;
   }
   fwrite(response, 1, strlen(response), f);
   fclose(f);
   free(response);

   *out_path = strdup(tmp);
   return *out_path ? 0 : -1;
}

static int tts_kokoro(const char *text, char **out_path)
{
   const char *base = getenv("AIMEE_GATEWAY_TTS_BASE_URL");
   if (!base || !base[0])
      base = "http://localhost:8880";

   char url[512];
   snprintf(url, sizeof(url), "%s/v1/audio/speech", base);

   const char *voice = getenv("AIMEE_GATEWAY_TTS_VOICE");
   if (!voice || !voice[0])
      voice = "af_sky";

   char body[4096];
   snprintf(body, sizeof(body), "{\"model\":\"kokoro\",\"voice\":\"%s\",\"input\":\"%s\"}", voice,
            text);

   char *response = NULL;
   int status =
       agent_http_post_content_type(url, NULL, "application/json", body, &response, 30000, NULL);
   if (status != 200 || !response)
   {
      LOG_WARN("tts", "Kokoro TTS HTTP %d", status);
      free(response);
      return -1;
   }

   char tmp[256];
   snprintf(tmp, sizeof(tmp), "/tmp/aimee-tts-%d.wav", (int)getpid());
   FILE *f = fopen(tmp, "wb");
   if (!f)
   {
      free(response);
      return -1;
   }
   fwrite(response, 1, strlen(response), f);
   fclose(f);
   free(response);

   *out_path = strdup(tmp);
   return *out_path ? 0 : -1;
}

int tts_synthesize(const char *text, char **out_path_out)
{
   if (!text || !text[0] || !out_path_out)
      return -1;

   const char *provider = getenv("AIMEE_GATEWAY_TTS_PROVIDER");
   if (!provider || !provider[0])
      provider = "local";

   if (strcmp(provider, "openai") == 0)
      return tts_openai(text, out_path_out);
   if (strcmp(provider, "kokoro") == 0)
      return tts_kokoro(text, out_path_out);
   return tts_local(text, out_path_out);
}
