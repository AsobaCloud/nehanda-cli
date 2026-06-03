#include "kb_ingest_normalize.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

const char *kb_ingest_detect_format(const char *filename)
{
   if (!filename)
      return "passthrough";

   const char *base = strrchr(filename, '/');
   base = base ? base + 1 : filename;

   const char *dot = strrchr(base, '.');
   if (!dot || dot == base)
      return "passthrough";

   const char *ext = dot;

   if (strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".txt") == 0)
      return "passthrough";

   if (strcasecmp(ext, ".docx") == 0 || strcasecmp(ext, ".pptx") == 0 ||
       strcasecmp(ext, ".xlsx") == 0 || strcasecmp(ext, ".odt") == 0 ||
       strcasecmp(ext, ".epub") == 0 || strcasecmp(ext, ".html") == 0 ||
       strcasecmp(ext, ".htm") == 0 || strcasecmp(ext, ".rtf") == 0)
      return "pandoc";

   if (strcasecmp(ext, ".pdf") == 0)
      return "pdftotext";

   if (strcasecmp(ext, ".c") == 0 || strcasecmp(ext, ".h") == 0 || strcasecmp(ext, ".cpp") == 0 ||
       strcasecmp(ext, ".cc") == 0 || strcasecmp(ext, ".cxx") == 0 ||
       strcasecmp(ext, ".hpp") == 0 || strcasecmp(ext, ".rs") == 0 || strcasecmp(ext, ".go") == 0 ||
       strcasecmp(ext, ".py") == 0 || strcasecmp(ext, ".ts") == 0 || strcasecmp(ext, ".js") == 0 ||
       strcasecmp(ext, ".java") == 0 || strcasecmp(ext, ".cs") == 0 ||
       strcasecmp(ext, ".rb") == 0 || strcasecmp(ext, ".sh") == 0 ||
       strcasecmp(ext, ".swift") == 0 || strcasecmp(ext, ".kt") == 0 ||
       strcasecmp(ext, ".scala") == 0)
      return "code";

   return "passthrough";
}

/* Map a filename to pandoc's input format (`-f`). pandoc must be told the
 * source format explicitly: without `-f` it uses its default *markdown* reader
 * for everything, which (a) never actually converts non-markdown formats and
 * (b) for a binary source (docx/odt/epub are ZIP archives) feeds binary into
 * the markdown parser, producing garbage and pathological slowdowns. Returns a
 * static format string, or NULL if unknown (caller omits `-f`). */
static const char *pandoc_input_format(const char *filename)
{
   if (!filename)
      return NULL;
   const char *base = strrchr(filename, '/');
   base = base ? base + 1 : filename;
   const char *ext = strrchr(base, '.');
   if (!ext)
      return NULL;
   if (strcasecmp(ext, ".docx") == 0)
      return "docx";
   if (strcasecmp(ext, ".odt") == 0)
      return "odt";
   if (strcasecmp(ext, ".epub") == 0)
      return "epub";
   if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
      return "html";
   if (strcasecmp(ext, ".rtf") == 0)
      return "rtf";
   if (strcasecmp(ext, ".pptx") == 0)
      return "pptx";
   return NULL;
}

static int run_subprocess(const char *argv[], const char *input, int input_len, char *out_buf,
                          int out_cap)
{
   int in_pipe[2], out_pipe[2];

   if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0)
      return -1;

   /* CLOEXEC on every pipe fd: the child re-creates stdin/stdout via dup2
    * (which clears CLOEXEC on fd 0/1), so the converter keeps its stdio but
    * inherits nothing else across exec. Without this, two converter forks that
    * overlap cross-inherit each other's stdin write-end, so one converter's
    * stdin never reaches EOF and it hangs forever (also pinning any leaked
    * listening socket). */
   fcntl(in_pipe[0], F_SETFD, FD_CLOEXEC);
   fcntl(in_pipe[1], F_SETFD, FD_CLOEXEC);
   fcntl(out_pipe[0], F_SETFD, FD_CLOEXEC);
   fcntl(out_pipe[1], F_SETFD, FD_CLOEXEC);

   pid_t pid = fork();
   if (pid < 0)
      return -1;

   if (pid == 0)
   {
      close(in_pipe[1]);
      close(out_pipe[0]);
      dup2(in_pipe[0], STDIN_FILENO);
      dup2(out_pipe[1], STDOUT_FILENO);
      close(in_pipe[0]);
      close(out_pipe[1]);
      execvp(argv[0], (char *const *)(const char *const *)argv);
      _exit(127);
   }

   close(in_pipe[0]);
   close(out_pipe[1]);

   if (input && input_len > 0)
   {
      int written = 0;
      while (written < input_len)
      {
         ssize_t n = write(in_pipe[1], input + written, input_len - written);
         if (n <= 0)
            break;
         written += (int)n;
      }
   }
   close(in_pipe[1]);

   int total = 0;
   while (total < out_cap - 1)
   {
      ssize_t n = read(out_pipe[0], out_buf + total, out_cap - 1 - total);
      if (n <= 0)
         break;
      total += (int)n;
   }
   out_buf[total] = '\0';
   close(out_pipe[0]);

   int status = 0;
   waitpid(pid, &status, 0);

   if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      return -1;
   return 0;
}

int kb_ingest_normalize(const char *filename, const char *bytes, int nbytes, char *out_buf,
                        int out_cap, char *converter_out, int conv_cap, char *converter_version_out,
                        int ver_cap)
{
   const char *converter = kb_ingest_detect_format(filename);
   snprintf(converter_out, conv_cap, "%s", converter);

   if (strcmp(converter, "passthrough") == 0)
   {
      int n = nbytes < out_cap - 1 ? nbytes : out_cap - 1;
      memcpy(out_buf, bytes, n);
      out_buf[n] = '\0';
      snprintf(converter_version_out, ver_cap, "%s", "");
      return 0;
   }

   if (strcmp(converter, "pandoc") == 0)
   {
      char ver_buf[256] = {0};
      const char *ver_argv[] = {"pandoc", "--version", NULL};
      if (run_subprocess(ver_argv, "", 0, ver_buf, sizeof(ver_buf)) == 0)
      {
         char *nl = strchr(ver_buf, '\n');
         if (nl)
            *nl = '\0';
         snprintf(converter_version_out, ver_cap, "%s", ver_buf);
      }
      else
      {
         snprintf(converter_version_out, ver_cap, "%s", "unknown");
      }

      /* Tell pandoc the input format explicitly (see pandoc_input_format). */
      const char *fmt = pandoc_input_format(filename);
      const char *argv_fmt[] = {"pandoc", "-f", fmt, "-t", "markdown", NULL};
      const char *argv_nofmt[] = {"pandoc", "-t", "markdown", NULL};
      const char **argv = fmt ? argv_fmt : argv_nofmt;
      if (run_subprocess(argv, bytes, nbytes, out_buf, out_cap) != 0)
      {
         LOG_WARN("kb_ingest_normalize", "pandoc subprocess failed for file: %s",
                  filename ? filename : "(null)");
         return -1;
      }
      return 0;
   }

   if (strcmp(converter, "pdftotext") == 0)
   {
      snprintf(converter_version_out, ver_cap, "%s", "pdftotext");

      char tmppath[] = "/tmp/aimee_ingest_XXXXXX";
      int fd = mkstemp(tmppath);
      if (fd < 0)
      {
         LOG_WARN("kb_ingest_normalize", "mkstemp failed for pdftotext");
         return -1;
      }

      int written = 0;
      while (written < nbytes)
      {
         ssize_t n = write(fd, bytes + written, nbytes - written);
         if (n <= 0)
            break;
         written += (int)n;
      }
      close(fd);

      const char *argv[] = {"pdftotext", tmppath, "-", NULL};
      int ret = run_subprocess(argv, "", 0, out_buf, out_cap);
      unlink(tmppath);

      if (ret != 0)
      {
         LOG_WARN("kb_ingest_normalize", "pdftotext subprocess failed for file: %s",
                  filename ? filename : "(null)");
         return -1;
      }
      return 0;
   }

   if (strcmp(converter, "code") == 0)
   {
      const char *ext = filename ? strrchr(filename, '.') : NULL;
      const char *lang = "text";
      if (ext)
      {
         if (strcasecmp(ext, ".c") == 0 || strcasecmp(ext, ".h") == 0 ||
             strcasecmp(ext, ".cpp") == 0 || strcasecmp(ext, ".cc") == 0 ||
             strcasecmp(ext, ".cxx") == 0 || strcasecmp(ext, ".hpp") == 0)
            lang = "c";
         else if (strcasecmp(ext, ".rs") == 0)
            lang = "rust";
         else if (strcasecmp(ext, ".go") == 0)
            lang = "go";
         else if (strcasecmp(ext, ".py") == 0)
            lang = "python";
         else if (strcasecmp(ext, ".ts") == 0)
            lang = "typescript";
         else if (strcasecmp(ext, ".js") == 0)
            lang = "javascript";
         else if (strcasecmp(ext, ".java") == 0)
            lang = "java";
         else if (strcasecmp(ext, ".cs") == 0)
            lang = "csharp";
         else if (strcasecmp(ext, ".rb") == 0)
            lang = "ruby";
         else if (strcasecmp(ext, ".sh") == 0)
            lang = "bash";
         else if (strcasecmp(ext, ".swift") == 0)
            lang = "swift";
         else if (strcasecmp(ext, ".kt") == 0)
            lang = "kotlin";
         else if (strcasecmp(ext, ".scala") == 0)
            lang = "scala";
      }

      snprintf(converter_version_out, ver_cap, "%s", "");

      int open_len = (int)(strlen(lang) + 4);
      int close_len = 4;
      int overhead = open_len + close_len;
      int code_cap = out_cap - overhead - 1;
      if (code_cap < 0)
         code_cap = 0;
      int code_len = nbytes < code_cap ? nbytes : code_cap;

      int written = snprintf(out_buf, out_cap, "```%s\n", lang);
      if (written < 0 || written >= out_cap)
         written = out_cap - 1;
      if (code_len > 0 && written + code_len < out_cap - 1)
      {
         memcpy(out_buf + written, bytes, code_len);
         written += code_len;
      }
      int remaining = out_cap - written;
      if (remaining > 0)
         snprintf(out_buf + written, remaining, "\n```\n");
      out_buf[out_cap - 1] = '\0';
      return 0;
   }

   if (strcmp(converter, "ocr_stub") == 0)
   {
      snprintf(out_buf, out_cap, "[OCR_REQUIRED]\n\nFile: %s\n", filename ? filename : "unknown");
      snprintf(converter_version_out, ver_cap, "%s", "");
      return 0;
   }

   int n = nbytes < out_cap - 1 ? nbytes : out_cap - 1;
   memcpy(out_buf, bytes, n);
   out_buf[n] = '\0';
   snprintf(converter_version_out, ver_cap, "%s", "");
   return 0;
}
