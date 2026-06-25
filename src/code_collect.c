/* code_collect.c: see code_collect.h. */
#include "platform.h" /* AIMEE_POSIX */
#include "code_collect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIMEE_POSIX
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int code_ext_ok(const char *name)
{
   static const char *const exts[] = {".c",    ".h",   ".cc",    ".cpp",  ".cxx", ".m",    ".py",
                                      ".go",   ".rs",  ".ts",    ".tsx",  ".js",  ".jsx",  ".md",
                                      ".yaml", ".yml", ".toml",  ".json", ".sh",  ".bash", ".rb",
                                      ".java", ".kt",  ".swift", NULL};
   const char *dot = strrchr(name, '.');
   if (!dot || dot == name)
      return 0;
   for (int i = 0; exts[i]; i++)
      if (strcmp(dot, exts[i]) == 0)
         return 1;
   return 0;
}

/* .git classification (defined below): 0 = not a repo, 1 = real checkout,
 * 2 = linked worktree. */
static int code_git_kind(const char *path);

static int code_dir_skip(const char *name)
{
   static const char *const skip[] = {"node_modules", "__pycache__", "vendor", "target", "build",
                                      "dist",         "out",         "sdks",   NULL};
   if (name[0] == '.') /* .git, .aimee, .cache, .svn, hidden dirs */
      return 1;
   for (int i = 0; skip[i]; i++)
      if (strcmp(name, skip[i]) == 0)
         return 1;
   return 0;
}

/* Walk root/rel recursively, invoking cb(rel_path, content, ctx) for each
 * indexable file. There is deliberately no file-count cap: the tree shape is
 * bounded by the directory skips and per-file size/extension/binary filters
 * above, and callers that need to bound a single network request stream the
 * files into byte-sized batches rather than relying on a fixed file count.
 * Stops early (returning 1) if cb returns non-zero. */
static int code_collect_walk(const char *root, const char *rel, code_collect_file_cb cb, void *ctx,
                             int *count)
{
   char path[4096];
   if (rel && rel[0])
      snprintf(path, sizeof(path), "%s/%s", root, rel);
   else
      snprintf(path, sizeof(path), "%s", root);

   DIR *dir = opendir(path);
   if (!dir)
      return 0;

   int stop = 0;
   struct dirent *ent;
   while (!stop && (ent = readdir(dir)) != NULL)
   {
      if (ent->d_name[0] == '.' &&
          (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
         continue; /* skip . and .. */

      char full[4096];
      snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);

      /* lstat (not stat): never follow symlinks. A self-referential dir symlink
       * (e.g. "src -> .") would otherwise loop until the path-length cap, pushing
       * the same files repeatedly; symlinked files are skipped as likely dups. */
      struct stat st;
      if (lstat(full, &st) != 0 || S_ISLNK(st.st_mode))
         continue;

      char rel_child[4096];
      if (rel && rel[0])
         snprintf(rel_child, sizeof(rel_child), "%s/%s", rel, ent->d_name);
      else
         snprintf(rel_child, sizeof(rel_child), "%s", ent->d_name);

      if (S_ISDIR(st.st_mode))
      {
         if (!code_dir_skip(ent->d_name))
            stop = code_collect_walk(root, rel_child, cb, ctx, count);
      }
      else if (S_ISREG(st.st_mode))
      {
         if (!code_ext_ok(ent->d_name))
            continue;
         if (st.st_size <= 0 || st.st_size > CODE_COLLECT_MAX_FILE_BYTES)
            continue;

         FILE *fp = fopen(full, "rb");
         if (!fp)
            continue;
         size_t sz = (size_t)st.st_size;
         char *buf = malloc(sz + 1);
         if (!buf)
         {
            fclose(fp);
            continue;
         }
         size_t got = fread(buf, 1, sz, fp);
         fclose(fp);
         if (got != sz)
         {
            free(buf);
            continue;
         }
         buf[sz] = '\0';

         /* Skip binary files by scanning for null bytes. */
         int binary = 0;
         for (size_t i = 0; i < sz && !binary; i++)
            if ((unsigned char)buf[i] == '\0')
               binary = 1;
         if (binary)
         {
            free(buf);
            continue;
         }

         if (cb(rel_child, buf, ctx) != 0)
            stop = 1;
         else
            (*count)++;
         free(buf);
      }
   }
   closedir(dir);
   return stop;
}

/* ---- git default-branch source ----
 *
 * Code indexing is sourced from the git DEFAULT branch (e.g. origin/main), not
 * the working tree, so the kb's code view is stable and canonical: a user's WIP
 * on a feature branch, or uncommitted edits, never thrash the graph/embeddings.
 * The working-tree walk above is reserved for non-git dirs and an explicit
 * AIMEE_CODE_INDEX_SOURCE=worktree opt-in. (Edit-time tools that ask "what does
 * THIS change touch" — e.g. blast-radius — read the working tree via their own
 * reader and deliberately do NOT route through here.) */

/* Prefix for every git invocation. This runs in an automated indexing path, so a
 * git command must NEVER block on an interactive credential or SSH-passphrase
 * prompt (GIT_TERMINAL_PROMPT=0 makes git fail fast instead), and a network-bound
 * step like `remote set-head -a` must time out rather than hang the drain
 * (BatchMode=yes + a short ConnectTimeout). Local repos never hit the network. */
#define GIT_SAFE_ENV                                                                               \
   "GIT_TERMINAL_PROMPT=0 GIT_SSH_COMMAND='ssh -o BatchMode=yes -o ConnectTimeout=5' "

/* Quote one argument for /bin/sh: wrap in single quotes, escaping any embedded
 * single quote as '\''. Returns 0 on success, -1 if it doesn't fit. */
static int shquote(const char *in, char *out, size_t outlen)
{
   size_t o = 0;
   if (outlen < 3)
      return -1;
   out[o++] = '\'';
   for (const char *p = in; *p; p++)
   {
      if (*p == '\'')
      {
         if (o + 4 >= outlen)
            return -1;
         out[o++] = '\'';
         out[o++] = '\\';
         out[o++] = '\'';
         out[o++] = '\'';
      }
      else
      {
         if (o + 1 >= outlen)
            return -1;
         out[o++] = *p;
      }
   }
   if (o + 2 > outlen)
      return -1;
   out[o++] = '\'';
   out[o] = '\0';
   return 0;
}

/* Run `git -C <root> <args>` and capture the first output line, trimmed. Returns
 * 0 on a clean exit with non-empty output, -1 otherwise. `args` is appended
 * verbatim (callers pass shell-safe, self-controlled flags/refs). */
static int git_capture_line(const char *root, const char *args, char *out, size_t outlen)
{
   char qroot[4096];
   if (shquote(root, qroot, sizeof(qroot)) != 0)
      return -1;
   char cmd[8192];
   if ((size_t)snprintf(cmd, sizeof(cmd), GIT_SAFE_ENV "git -C %s %s 2>/dev/null", qroot, args) >=
       sizeof(cmd))
      return -1;
   FILE *fp = popen(cmd, "r");
   if (!fp)
      return -1;
   out[0] = '\0';
   if (!fgets(out, (int)outlen, fp))
   {
      pclose(fp);
      return -1;
   }
   /* drain the rest so git never blocks on a full stdout pipe */
   char drain[512];
   while (fread(drain, 1, sizeof(drain), fp) == sizeof(drain))
      ;
   int rc = pclose(fp);
   size_t n = strlen(out);
   while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' '))
      out[--n] = '\0';
   return (rc == 0 && out[0]) ? 0 : -1;
}

/* Resolve the ref to index for `root` into `out` (e.g. "origin/main"). Order:
 *   1. origin/HEAD via symbolic-ref (the remote's advertised default);
 *   2. repair origin/HEAD once (`git remote set-head origin -a`) and retry —
 *      it is unset on repos whose remote was added after clone;
 *   3. first existing of origin/main, origin/master, main, master.
 * There is deliberately NO fall-through to the current HEAD or working tree:
 * that would re-introduce the exact WIP-thrash this design eliminates. Returns 0
 * on success, -1 if no default branch is resolvable (caller skips the repo). */
static int git_resolve_default_ref(const char *root, char *out, size_t outlen)
{
   if (git_capture_line(root, "symbolic-ref --short refs/remotes/origin/HEAD", out, outlen) == 0)
      return 0;
   char tmp[256];
   git_capture_line(root, "remote set-head origin -a", tmp, sizeof(tmp)); /* best-effort repair */
   if (git_capture_line(root, "symbolic-ref --short refs/remotes/origin/HEAD", out, outlen) == 0)
      return 0;
   static const char *const cands[] = {"origin/main", "origin/master", "main", "master", NULL};
   for (int i = 0; cands[i]; i++)
   {
      char args[256], sha[128];
      snprintf(args, sizeof(args), "rev-parse --verify --quiet '%s^{commit}'", cands[i]);
      if (git_capture_line(root, args, sha, sizeof(sha)) == 0)
      {
         snprintf(out, outlen, "%s", cands[i]);
         return 0;
      }
   }
   return -1;
}

/* Skip a tracked path whose any component is a VCS/build/vendor/hidden dir, so a
 * checked-in node_modules/vendor tree is filtered exactly like the worktree walk
 * filters it (code_dir_skip). */
static int code_path_skipped(const char *rel)
{
   const char *seg = rel;
   for (const char *p = rel;; p++)
   {
      if (*p == '/' || *p == '\0')
      {
         size_t len = (size_t)(p - seg);
         char comp[256];
         if (len && len < sizeof(comp))
         {
            memcpy(comp, seg, len);
            comp[len] = '\0';
            if (code_dir_skip(comp))
               return 1;
         }
         if (*p == '\0')
            break;
         seg = p + 1;
      }
   }
   return 0;
}

/* Read exactly n bytes from fp into buf (n known up front). Returns 0 on success. */
static int read_exact(FILE *fp, char *buf, size_t n)
{
   size_t got = 0;
   while (got < n)
   {
      size_t r = fread(buf + got, 1, n - got, fp);
      if (r == 0)
         return -1;
      got += r;
   }
   return 0;
}

/* Discard exactly n bytes from fp (oversized blob we must consume to stay aligned). */
static void skip_exact(FILE *fp, size_t n)
{
   char junk[4096];
   while (n)
   {
      size_t want = n < sizeof(junk) ? n : sizeof(junk);
      size_t r = fread(junk, 1, want, fp);
      if (r == 0)
         return;
      n -= r;
   }
}

/* Collect indexable blobs from `ref` of the repo at `root` via git plumbing:
 * one `ls-tree -r -z` to enumerate (oid, path) pairs and one `cat-file --batch`
 * (fed the wanted oids from a temp file, so our side only ever reads — no
 * bidirectional-pipe deadlock) to stream their content in request order. Pairs
 * content to path by sequence, so newlines in paths can't misattribute content.
 * Returns the cb-accepted file count, or -1 if git enumeration failed. */
static int code_collect_from_git(const char *root, const char *ref, code_collect_file_cb cb,
                                 void *ctx, int *count)
{
   char qroot[4096], qref[1024];
   if (shquote(root, qroot, sizeof(qroot)) != 0 || shquote(ref, qref, sizeof(qref)) != 0)
      return -1;

   /* (1) Enumerate. ls-tree -z: NUL-terminated records "<mode> <type> <oid>\t<path>". */
   char lscmd[10240];
   snprintf(lscmd, sizeof(lscmd), GIT_SAFE_ENV "git -C %s ls-tree -r -z %s 2>/dev/null", qroot,
            qref);
   FILE *ls = popen(lscmd, "r");
   if (!ls)
      return -1;
   char *blob = NULL;
   size_t blen = 0, bcap = 0;
   char chunk[8192];
   size_t r;
   while ((r = fread(chunk, 1, sizeof(chunk), ls)) > 0)
   {
      if (blen + r + 1 > bcap)
      {
         size_t ncap = bcap ? bcap * 2 : 65536;
         while (ncap < blen + r + 1)
            ncap *= 2;
         char *nb = realloc(blob, ncap);
         if (!nb)
         {
            free(blob);
            pclose(ls);
            return -1;
         }
         blob = nb;
         bcap = ncap;
      }
      memcpy(blob + blen, chunk, r);
      blen += r;
   }
   int lsrc = pclose(ls);
   if (lsrc != 0)
   {
      free(blob);
      return -1; /* bad ref / not a repo: let caller decide (skip), don't silently empty-index */
   }
   if (!blob || blen == 0)
   {
      free(blob); /* empty tree: nothing to index, but a valid (no-op) result */
      return 0;
   }

   /* Parse records, keeping the (oid, path) of indexable blobs in request order. */
   char **paths = NULL;
   char **oids = NULL;
   int n = 0, cap = 0;
   for (size_t i = 0; i < blen;)
   {
      char *rec = blob + i;
      size_t reclen = strnlen(rec, blen - i);
      i += reclen + 1;
      char *tab = memchr(rec, '\t', reclen);
      if (!tab)
         continue;
      *tab = '\0';
      const char *path = tab + 1;
      /* header = "<mode> <type> <oid>"; take type (2nd) and oid (3rd) tokens. */
      char *sp1 = strchr(rec, ' ');
      if (!sp1)
         continue;
      char *type = sp1 + 1;
      char *sp2 = strchr(type, ' ');
      if (!sp2)
         continue;
      *sp2 = '\0';
      const char *oid = sp2 + 1;
      if (strcmp(type, "blob") != 0)
         continue;
      if (!code_ext_ok(path) || code_path_skipped(path))
         continue;
      if (n == cap)
      {
         int ncap = cap ? cap * 2 : 256;
         char **np = realloc(paths, (size_t)ncap * sizeof(*np));
         char **no = realloc(oids, (size_t)ncap * sizeof(*no));
         if (!np || !no)
         {
            free(np ? np : paths);
            free(no ? no : oids);
            np = no = NULL;
            n = -1;
            break;
         }
         paths = np;
         oids = no;
         cap = ncap;
      }
      paths[n] = strdup(path);
      oids[n] = strdup(oid);
      if (!paths[n] || !oids[n])
      {
         free(paths[n]);
         free(oids[n]);
         break;
      }
      n++;
   }
   free(blob);
   if (n <= 0)
   {
      for (int k = 0; k < n; k++)
      {
         free(paths[k]);
         free(oids[k]);
      }
      free(paths);
      free(oids);
      return n < 0 ? -1 : 0;
   }

   /* (2) Write the wanted oids to a temp file, then `cat-file --batch < tmp`. */
   const char *tmpdir = getenv("TMPDIR");
   if (!tmpdir || !tmpdir[0])
      tmpdir = "/tmp";
   char tmpl[4096];
   snprintf(tmpl, sizeof(tmpl), "%s/aimee-cc-XXXXXX", tmpdir);
   int fd = mkstemp(tmpl);
   int result = 0;
   if (fd >= 0)
   {
      FILE *tf = fdopen(fd, "w");
      if (tf)
      {
         for (int k = 0; k < n; k++)
            fprintf(tf, "%s\n", oids[k]);
         fclose(tf);
      }
      else
         close(fd);

      char qtmp[4096];
      char catcmd[12288];
      if (shquote(tmpl, qtmp, sizeof(qtmp)) == 0 &&
          (size_t)snprintf(catcmd, sizeof(catcmd),
                           GIT_SAFE_ENV "git -C %s cat-file --batch <%s 2>/dev/null", qroot,
                           qtmp) < sizeof(catcmd))
      {
         FILE *cat = popen(catcmd, "r");
         if (cat)
         {
            int stop = 0;
            for (int k = 0; k < n && !stop; k++)
            {
               char hdr[256];
               if (!fgets(hdr, sizeof(hdr), cat))
                  break;
               /* "<oid> <type> <size>\n" or "<oid> missing\n" */
               char *s1 = strchr(hdr, ' ');
               if (!s1 || strstr(hdr, " missing"))
                  continue;
               char *s2 = strchr(s1 + 1, ' ');
               if (!s2)
                  continue;
               size_t size = (size_t)strtoul(s2 + 1, NULL, 10);
               if (size == 0)
               {
                  skip_exact(cat, 1); /* trailing newline; empty blob = skip */
                  continue;
               }
               if (size > CODE_COLLECT_MAX_FILE_BYTES)
               {
                  skip_exact(cat, size + 1);
                  continue;
               }
               char *content = malloc(size + 1);
               if (!content)
               {
                  skip_exact(cat, size + 1);
                  continue;
               }
               if (read_exact(cat, content, size) != 0)
               {
                  free(content);
                  break;
               }
               skip_exact(cat, 1); /* trailing newline after the blob body */
               content[size] = '\0';
               int binary = 0;
               for (size_t b = 0; b < size && !binary; b++)
                  if (content[b] == '\0')
                     binary = 1;
               if (!binary)
               {
                  if (cb(paths[k], content, ctx) != 0)
                     stop = 1;
                  else
                     (*count)++;
               }
               free(content);
            }
            pclose(cat);
         }
         else
            result = -1;
      }
      else
         result = -1;
      unlink(tmpl);
   }
   else
      result = -1;

   for (int k = 0; k < n; k++)
   {
      free(paths[k]);
      free(oids[k]);
   }
   free(paths);
   free(oids);
   return result;
}

int code_collect_files_cb(const char *root, code_collect_file_cb cb, void *ctx)
{
   if (!root || !root[0] || !cb)
      return 0;

   /* Source selection (AIMEE_CODE_INDEX_SOURCE): "worktree" forces the working
    * tree (a user who wants their WIP indexed); "default"/"auto"/unset index the
    * git default branch for a git repo. Non-git dirs always use the working
    * tree. A git repo with no resolvable default branch is SKIPPED rather than
    * silently indexing unstable code. */
   const char *mode = getenv("AIMEE_CODE_INDEX_SOURCE");
   if (!mode || !mode[0])
      mode = "auto";

   int count = 0;
   if (strcmp(mode, "worktree") != 0 && code_git_kind(root) != 0)
   {
      char ref[1024];
      if (git_resolve_default_ref(root, ref, sizeof(ref)) == 0)
      {
         int rc = code_collect_from_git(root, ref, cb, ctx, &count);
         if (rc >= 0)
            return count;
         /* git enumeration failed unexpectedly: fall through to the working
          * tree so a transient git error doesn't drop the project entirely. */
         fprintf(stderr, "[code_collect] %s: git read of %s failed; using working tree\n", root,
                 ref);
      }
      else
      {
         fprintf(stderr,
                 "[code_collect] %s: no default branch resolvable (origin/HEAD unset, no "
                 "main/master); skipping. Set AIMEE_CODE_INDEX_SOURCE=worktree to index the "
                 "working tree.\n",
                 root);
         return 0;
      }
   }

   code_collect_walk(root, NULL, cb, ctx, &count);
   return count;
}

/* Array-append callback: collect every file into a cJSON array (no cap). Used by
 * the server-side local scan, which pushes the result in a single request. */
static int code_collect_append_cb(const char *rel_path, const char *content, void *ctx)
{
   cJSON *files_arr = (cJSON *)ctx;
   cJSON *entry = cJSON_CreateObject();
   if (!entry)
      return 0; /* skip this file, keep walking */
   cJSON_AddStringToObject(entry, "rel_path", rel_path);
   cJSON_AddStringToObject(entry, "content", content);
   cJSON_AddItemToArray(files_arr, entry);
   return 0;
}

int code_collect_files(const char *root, cJSON *files_arr)
{
   if (!root || !root[0] || !files_arr)
      return 0;
   return code_collect_files_cb(root, code_collect_append_cb, files_arr);
}

/* .git classification: 0 = not a repo, 1 = real checkout (.git dir),
 * 2 = linked worktree (.git regular file: "gitdir: <path>"). Forward-declared
 * above so code_collect_files_cb can choose its source before this definition. */
static int code_git_kind(const char *path)
{
   char g[4096];
   snprintf(g, sizeof(g), "%s/.git", path);
   struct stat st;
   if (stat(g, &st) != 0)
      return 0;
   return S_ISDIR(st.st_mode) ? 1 : 2;
}

static void code_discover_walk(const char *dir, int depth, code_collect_repo_cb cb, void *ctx,
                               int *n)
{
   if (depth > 10)
      return;

   /* Register real checkouts always; honor a linked worktree only when it is the
    * explicit scan root (depth 0), never as a sibling found during descent. */
   int gk = code_git_kind(dir);
   if (gk == 1 || (gk == 2 && depth == 0))
   {
      char abs[4096];
      cb(realpath(dir, abs) ? abs : dir, ctx);
      (*n)++;
   }

   DIR *d = opendir(dir);
   if (!d)
      return;
   struct dirent *ent;
   while ((ent = readdir(d)) != NULL)
   {
      if (ent->d_name[0] == '.' || code_dir_skip(ent->d_name))
         continue;
      char sub[4096];
      snprintf(sub, sizeof(sub), "%s/%s", dir, ent->d_name);
      /* lstat: never follow symlinks (cycle guard, e.g. "src -> ."). */
      struct stat st;
      if (lstat(sub, &st) != 0 || S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode))
         continue;
      code_discover_walk(sub, depth + 1, cb, ctx, n);
   }
   closedir(d);
}

int code_collect_discover_repos(const char *root, code_collect_repo_cb cb, void *ctx)
{
   if (!root || !root[0] || !cb)
      return 0;
   int n = 0;
   code_discover_walk(root, 0, cb, ctx, &n);
   return n;
}

#else /* !AIMEE_POSIX */

int code_collect_files_cb(const char *root, code_collect_file_cb cb, void *ctx)
{
   (void)root;
   (void)cb;
   (void)ctx;
   return 0;
}

int code_collect_files(const char *root, cJSON *files_arr)
{
   (void)root;
   (void)files_arr;
   return 0;
}

int code_collect_discover_repos(const char *root, code_collect_repo_cb cb, void *ctx)
{
   (void)root;
   (void)cb;
   (void)ctx;
   return 0;
}

#endif /* AIMEE_POSIX */
