/* stt.c: speech-to-text dispatcher */
#include "stt.h"
#include "aimee.h"
#include "log.h"
#include "agent_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *hallucination_phrases[] = {"thanks for watching", "subscribe to",
                                              "thank you for watching", "please like and subscribe",
                                              "see you in the next"};
static const int hallucination_count =
    sizeof(hallucination_phrases) / sizeof(hallucination_phrases[0]);

int stt_is_hallucination(const char *text)
{
   if (!text)
      return 0;

   /* case-insensitive substring check */
   size_t text_len = strlen(text);
   for (int i = 0; i < hallucination_count; i++)
   {
      const char *phrase = hallucination_phrases[i];
      size_t phrase_len = strlen(phrase);

      for (size_t j = 0; j + phrase_len <= text_len; j++)
      {
         size_t k;
         for (k = 0; k < phrase_len; k++)
         {
            if (tolower((unsigned char)text[j + k]) != tolower((unsigned char)phrase[k]))
               break;
         }
         if (k == phrase_len)
            return 1;
      }
   }
   return 0;
}

/* read up to n bytes from FILE* into buf, returns bytes read */
static size_t read_full(FILE *fp, char *buf, size_t n)
{
   size_t total = 0;
   while (total + 1 < n)
   {
      size_t r = fread(buf + total, 1, n - total - 1, fp);
      total += r;
      if (r == 0)
         break;
   }
   buf[total] = '\0';
   return total;
}

/* local: invoke whisper CLI via popen */
static int stt_transcribe_local(const char *path, char *out_text, size_t out_size)
{
   const char *model = getenv("AIMEE_GATEWAY_STT_MODEL");
   if (!model)
      model = "base";

   /* build command: whisper <path> --model <model> --output-format txt */
   size_t cmd_len =
       snprintf(NULL, 0, "whisper \"%s\" --model %s --output-format txt 2>/dev/null", path, model);
   char *cmd = malloc(cmd_len + 1);
   if (!cmd)
      return -1;
   snprintf(cmd, cmd_len + 1, "whisper \"%s\" --model %s --output-format txt 2>/dev/null", path,
            model);

   FILE *fp = popen(cmd, "r");
   free(cmd);

   if (!fp)
      return -1;

   (void)read_full(fp, out_text, out_size);
   int status = pclose(fp);

   if (status != 0)
   {
      LOG_WARN("stt", "whisper CLI exited with status %d", status);
      return -1;
   }

   /* strip trailing newline */
   size_t len = strlen(out_text);
   while (len > 0 && (out_text[len - 1] == '\n' || out_text[len - 1] == '\r'))
   {
      out_text[--len] = '\0';
   }

   return 0;
}

/* API-based transcription: POST audio file to provider */
static int stt_transcribe_api(const char *provider, const char *endpoint, const char *api_key_env,
                              const char *path, char *out_text, size_t out_size)
{
   const char *api_key = getenv(api_key_env);
   if (!api_key)
   {
      LOG_WARN("stt", "%s API key not set (%s)", provider, api_key_env);
      return -1;
   }

   /* build multipart form: model=whisper-1, file=@<path> */
   size_t body_len = snprintf(NULL, 0, "model=whisper-1&file=@%s", path);
   char *body = malloc(body_len + 1);
   if (!body)
      return -1;
   snprintf(body, body_len + 1, "model=whisper-1&file=@%s", path);

   char auth_header[256];
   snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

   char *response = NULL;
   int http_status = agent_http_post_content_type(
       endpoint, auth_header, "application/x-www-form-urlencoded", body, &response, 30000, NULL);
   free(body);

   if (http_status != 200 || !response)
   {
      LOG_WARN("stt", "%s transcription HTTP %d", provider, http_status);
      free(response);
      return -1;
   }

   /* parse {"text": "..."} */
   snprintf(out_text, out_size, "%s", response);
   free(response);

   /* strip trailing newline */
   size_t len = strlen(out_text);
   while (len > 0 && (out_text[len - 1] == '\n' || out_text[len - 1] == '\r'))
   {
      out_text[--len] = '\0';
   }

   return 0;
}

int stt_transcribe_file(const char *path, char *out_text, size_t out_size)
{
   if (!path || !out_text || out_size == 0)
   {
      LOG_WARN("stt", "invalid arguments to stt_transcribe_file");
      return -1;
   }

   const char *provider = getenv("AIMEE_GATEWAY_STT_PROVIDER");
   if (!provider)
      provider = "local";

   if (strcmp(provider, "local") == 0)
   {
      return stt_transcribe_local(path, out_text, out_size);
   }
   else if (strcmp(provider, "openai") == 0)
   {
      return stt_transcribe_api(provider, "https://api.openai.com/v1/audio/transcriptions",
                                "OPENAI_API_KEY", path, out_text, out_size);
   }
   else if (strcmp(provider, "groq") == 0)
   {
      return stt_transcribe_api(provider, "https://api.groq.com/openai/v1/audio/transcriptions",
                                "GROQ_API_KEY", path, out_text, out_size);
   }
   else
   {
      LOG_WARN("stt", "unknown STT provider: %s", provider);
      return -1;
   }
}