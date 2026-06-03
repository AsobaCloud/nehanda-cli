/* test_delegate_backend_docker.c: registry membership, pure helpers,
 * and fake-docker round trips for the docker backend. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "delegate_backend_docker.h"

/* Forward decls — definitions live further down with the rest of the
 * fixture-using cases; forward refs let us call them from earlier
 * tests. */
static const char *write_fake_docker_fixture(void);
static void teardown_fake_docker(void);
static int fake_container_exists(const char *container_name);

static void test_register_puts_docker_in_registry(void)
{
   delegate_backend_reset_for_test();
   assert(delegate_backend_register_docker() == 0);
   delegate_backend_t *b = delegate_backend_lookup("docker");
   assert(b != NULL);
   assert(b == delegate_backend_docker_get());
   /* Idempotent — second register call rejected by the registry. */
   assert(delegate_backend_register_docker() == -1);
   /* All vtable slots wired (no NULL pointers). */
   assert(b->acquire && b->release && b->exec);
   assert(b->read_file && b->write_file && b->list_dir);
   assert(b->get_cwd && b->set_cwd);
   printf("  PASS: test_register_puts_docker_in_registry\n");
}

static void test_file_ops_reject_null_state(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   char *out = (char *)0x1;
   assert(b->read_file(b, NULL, "x", 0, 0, &out) == -1);
   assert(out == NULL);
   assert(b->write_file(b, NULL, "p", "c") == -1);
   char **entries = (char **)0x1;
   assert(b->list_dir(b, NULL, ".", &entries) == -1);
   assert(entries == NULL);
   printf("  PASS: test_file_ops_reject_null_state\n");
}

/* Override AIMEE_DOCKER_WORKDIR so file ops resolve under a writable
 * /tmp anchor instead of the real /workspace path (which would need
 * root). Combined with the fake-docker fixture's exec mode that runs
 * commands locally, this lets file-op tests round-trip through bash
 * without a real container. */
static void setup_docker_fileio_state(delegate_backend_t *b, const char *task_id, void **state_out)
{
   const char *fixture = write_fake_docker_fixture();
   setenv("AIMEE_DOCKER_BIN", fixture, 1);
   /* Anchor the in-container WORKDIR under /tmp for the duration of
    * the test. mkdir is idempotent; the per-pid suffix avoids cross-
    * test pollution. */
   char workdir[256];
   snprintf(workdir, sizeof(workdir), "/tmp/aimee-docker-workdir-%d", (int)getpid());
   mkdir(workdir, 0700);
   setenv("AIMEE_DOCKER_WORKDIR", workdir, 1);

   delegate_backend_config_t cfg = {0};
   assert(b->acquire(b, task_id, &cfg, state_out) == 0);
}

static void teardown_docker_fileio_state(delegate_backend_t *b, void *state)
{
   if (state)
      b->release(b, state, 0);
   teardown_fake_docker();
   char rm[512];
   snprintf(rm, sizeof(rm), "rm -rf /tmp/aimee-docker-workdir-%d", (int)getpid());
   (void)system(rm);
   unsetenv("AIMEE_DOCKER_BIN");
   unsetenv("AIMEE_DOCKER_WORKDIR");
}

static void test_docker_write_then_read_roundtrip(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   void *state = NULL;
   setup_docker_fileio_state(b, "task-fileio-1", &state);

   assert(b->write_file(b, state, "hello.txt", "hello in container\n") == 0);
   char *content = NULL;
   assert(b->read_file(b, state, "hello.txt", 0, 0, &content) == 0);
   assert(content != NULL);
   assert(strcmp(content, "hello in container\n") == 0);
   free(content);

   teardown_docker_fileio_state(b, state);
   printf("  PASS: test_docker_write_then_read_roundtrip\n");
}

static void test_docker_path_validation_rejects_escapes(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   void *state = NULL;
   setup_docker_fileio_state(b, "task-fileio-2", &state);

   assert(b->write_file(b, state, "/etc/passwd", "x") == -1);
   assert(b->write_file(b, state, "../escape.txt", "x") == -1);
   assert(b->write_file(b, state, "ok/../../escape", "x") == -1);
   char *content = (char *)0x1;
   assert(b->read_file(b, state, "../etc/passwd", 0, 0, &content) == -1);
   assert(content == NULL);

   teardown_docker_fileio_state(b, state);
   printf("  PASS: test_docker_path_validation_rejects_escapes\n");
}

static void test_docker_list_dir_returns_entries(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   void *state = NULL;
   setup_docker_fileio_state(b, "task-list-1", &state);

   assert(b->write_file(b, state, "a.txt", "a") == 0);
   assert(b->write_file(b, state, "b.txt", "b") == 0);

   char **entries = NULL;
   int n = b->list_dir(b, state, ".", &entries);
   assert(n >= 2);
   int saw_a = 0, saw_b = 0;
   for (int i = 0; entries[i]; i++)
   {
      if (strcmp(entries[i], "a.txt") == 0)
         saw_a = 1;
      if (strcmp(entries[i], "b.txt") == 0)
         saw_b = 1;
      free(entries[i]);
   }
   free(entries);
   assert(saw_a && saw_b);

   teardown_docker_fileio_state(b, state);
   printf("  PASS: test_docker_list_dir_returns_entries\n");
}

/* Write a fake-docker fixture script. Honors:
 *   docker start <name>           -> exit 0 if .exists flag present, else 1
 *   docker create --name N ...    -> touch .exists flag, exit 0
 *   docker stop <name>            -> exit 0 (we don't track running state)
 *   docker rm -f <name>           -> remove the .exists flag
 *   docker exec -i N bash -c CMD  -> exec bash -c "CMD" locally so the
 *                                    test exercises the full exec path
 *                                    without a real docker daemon
 * State files live under /tmp/aimee-fake-docker-state-<pid>/. */
static const char *write_fake_docker_fixture(void)
{
   static char path[256];
   snprintf(path, sizeof(path), "/tmp/aimee-fake-docker-%d.sh", (int)getpid());
   char state_dir[256];
   snprintf(state_dir, sizeof(state_dir), "/tmp/aimee-fake-docker-state-%d", (int)getpid());
   mkdir(state_dir, 0700);

   FILE *f = fopen(path, "w");
   assert(f != NULL);
   fprintf(f,
           "#!/bin/bash\n"
           "STATE_DIR=%s\n"
           "case \"$1\" in\n"
           "  start)\n"
           "    name=\"$2\"\n"
           "    [ -f \"$STATE_DIR/$name.exists\" ] && exit 0\n"
           "    exit 1\n"
           "    ;;\n"
           "  create)\n"
           "    shift\n"
           "    name=\"\"\n"
           "    while [ $# -gt 0 ]; do\n"
           "      if [ \"$1\" = \"--name\" ]; then name=\"$2\"; shift 2; continue; fi\n"
           "      shift\n"
           "    done\n"
           "    [ -n \"$name\" ] && touch \"$STATE_DIR/$name.exists\"\n"
           "    exit 0\n"
           "    ;;\n"
           "  stop)\n"
           "    exit 0\n"
           "    ;;\n"
           "  rm)\n"
           "    name=\"${@: -1}\"\n"
           "    rm -f \"$STATE_DIR/$name.exists\"\n"
           "    exit 0\n"
           "    ;;\n"
           "  exec)\n"
           "    # docker exec -i <name> bash -c <cmd>; LAST argv is the\n"
           "    # b64-wrapped command. Run it locally via bash.\n"
           "    cmd=\"${@: -1}\"\n"
           "    exec bash -c \"$cmd\"\n"
           "    ;;\n"
           "  *)\n"
           "    exit 99\n"
           "    ;;\n"
           "esac\n",
           state_dir);
   fclose(f);
   chmod(path, 0700);
   return path;
}

static void teardown_fake_docker(void)
{
   char rm[256];
   snprintf(rm, sizeof(rm), "rm -rf /tmp/aimee-fake-docker-state-%d /tmp/aimee-fake-docker-%d.sh",
            (int)getpid(), (int)getpid());
   (void)system(rm);
}

static int fake_container_exists(const char *container_name)
{
   char p[512];
   snprintf(p, sizeof(p), "/tmp/aimee-fake-docker-state-%d/%s.exists", (int)getpid(),
            container_name);
   struct stat s;
   return stat(p, &s) == 0;
}

static void test_acquire_creates_and_starts_container(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   const char *fixture = write_fake_docker_fixture();
   setenv("AIMEE_DOCKER_BIN", fixture, 1);

   delegate_backend_config_t cfg = {0};
   cfg.image = "ubuntu:22.04";
   void *state = NULL;
   assert(b->acquire(b, "task-acq-1", &cfg, &state) == 0);
   assert(state != NULL);
   /* The fixture's "create" handler should have touched the .exists
    * flag for the canonical container name. */
   assert(fake_container_exists("aimee-delegate-task-acq-1"));

   /* release(hibernate=0) → docker rm -f → flag removed. */
   b->release(b, state, 0);
   assert(!fake_container_exists("aimee-delegate-task-acq-1"));

   teardown_fake_docker();
   unsetenv("AIMEE_DOCKER_BIN");
   printf("  PASS: test_acquire_creates_and_starts_container\n");
}

static void test_release_hibernate_keeps_container(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   const char *fixture = write_fake_docker_fixture();
   setenv("AIMEE_DOCKER_BIN", fixture, 1);

   delegate_backend_config_t cfg = {0};
   void *state = NULL;
   assert(b->acquire(b, "task-hib-1", &cfg, &state) == 0);
   assert(fake_container_exists("aimee-delegate-task-hib-1"));

   /* hibernate=1 → docker stop, container persists. */
   b->release(b, state, 1);
   assert(fake_container_exists("aimee-delegate-task-hib-1"));

   /* Re-acquire → docker start (fixture sees the .exists flag) →
    * resumes the same container, no second create. */
   void *state2 = NULL;
   assert(b->acquire(b, "task-hib-1", &cfg, &state2) == 0);
   b->release(b, state2, 0);
   assert(!fake_container_exists("aimee-delegate-task-hib-1"));

   teardown_fake_docker();
   unsetenv("AIMEE_DOCKER_BIN");
   printf("  PASS: test_release_hibernate_keeps_container\n");
}

static void test_docker_exec_runs_through_fake(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   const char *fixture = write_fake_docker_fixture();
   setenv("AIMEE_DOCKER_BIN", fixture, 1);

   delegate_backend_config_t cfg = {0};
   void *state = NULL;
   assert(b->acquire(b, "task-exec-1", &cfg, &state) == 0);

   char out[4096] = {0}, err[4096] = {0};
   delegate_exec_result_t r = {0, 0, out, sizeof(out), err, sizeof(err)};
   assert(b->exec(b, state, "echo hello-from-docker", 5000, &r) == 0);
   assert(r.exit_code == 0);
   assert(strcmp(out, "hello-from-docker\n") == 0);
   b->release(b, state, 0);

   teardown_fake_docker();
   unsetenv("AIMEE_DOCKER_BIN");
   printf("  PASS: test_docker_exec_runs_through_fake\n");
}

static void test_docker_exec_propagates_nonzero_exit(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   const char *fixture = write_fake_docker_fixture();
   setenv("AIMEE_DOCKER_BIN", fixture, 1);

   delegate_backend_config_t cfg = {0};
   void *state = NULL;
   assert(b->acquire(b, "task-exec-2", &cfg, &state) == 0);

   char out[256] = {0}, err[256] = {0};
   delegate_exec_result_t r = {0, 0, out, sizeof(out), err, sizeof(err)};
   assert(b->exec(b, state, "exit 9", 5000, &r) == 0);
   assert(r.exit_code == 9);
   b->release(b, state, 0);

   teardown_fake_docker();
   unsetenv("AIMEE_DOCKER_BIN");
   printf("  PASS: test_docker_exec_propagates_nonzero_exit\n");
}

static void test_docker_exec_set_cwd_prefixes_subsequent_calls(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   const char *fixture = write_fake_docker_fixture();
   setenv("AIMEE_DOCKER_BIN", fixture, 1);

   delegate_backend_config_t cfg = {0};
   void *state = NULL;
   assert(b->acquire(b, "task-exec-cwd", &cfg, &state) == 0);

   /* Default get_cwd reflects the container's WORKDIR (or the
    * AIMEE_DOCKER_WORKDIR override if set; this test leaves it
    * unset so it lands on the production default). */
   char *cwd = NULL;
   assert(b->get_cwd(b, state, &cwd) == 0);
   assert(strcmp(cwd, "/workspace") == 0);
   free(cwd);

   /* set_cwd to /tmp and exec pwd → "/tmp\n". */
   assert(b->set_cwd(b, state, "/tmp") == 0);
   char out[512] = {0}, err[512] = {0};
   delegate_exec_result_t r = {0, 0, out, sizeof(out), err, sizeof(err)};
   assert(b->exec(b, state, "pwd", 5000, &r) == 0);
   assert(strcmp(out, "/tmp\n") == 0);

   b->release(b, state, 0);
   teardown_fake_docker();
   unsetenv("AIMEE_DOCKER_BIN");
   printf("  PASS: test_docker_exec_set_cwd_prefixes_subsequent_calls\n");
}

static void test_acquire_rejects_invalid_args(void)
{
   delegate_backend_reset_for_test();
   delegate_backend_register_docker();
   delegate_backend_t *b = delegate_backend_docker_get();
   /* Empty task_id rejected. */
   void *state = (void *)0x1;
   assert(b->acquire(b, "", NULL, &state) == -1);
   assert(state == NULL);
   /* NULL state_out rejected. */
   assert(b->acquire(b, "task", NULL, NULL) == -1);
   printf("  PASS: test_acquire_rejects_invalid_args\n");
}

static void test_container_name_basic(void)
{
   char name[128] = {0};
   assert(delegate_backend_docker_container_name("task-abc-123", name, sizeof(name)) == 0);
   assert(strcmp(name, "aimee-delegate-task-abc-123") == 0);
   printf("  PASS: test_container_name_basic\n");
}

static void test_container_name_sanitises_invalid_chars(void)
{
   /* Spaces, slashes, colons all replaced with '_'; alnum + _.- kept. */
   char name[128] = {0};
   assert(delegate_backend_docker_container_name("foo bar:baz/qux.v1-2", name, sizeof(name)) == 0);
   assert(strcmp(name, "aimee-delegate-foo_bar_baz_qux.v1-2") == 0);
   printf("  PASS: test_container_name_sanitises_invalid_chars\n");
}

static void test_container_name_rejects_invalid(void)
{
   char name[128] = {0};
   assert(delegate_backend_docker_container_name(NULL, name, sizeof(name)) == -1);
   assert(delegate_backend_docker_container_name("", name, sizeof(name)) == -1);
   assert(delegate_backend_docker_container_name("task", NULL, 128) == -1);
   assert(delegate_backend_docker_container_name("task", name, 0) == -1);
   /* Buffer too small. */
   char tiny[8];
   assert(delegate_backend_docker_container_name("very-long-task", tiny, sizeof(tiny)) == -1);
   printf("  PASS: test_container_name_rejects_invalid\n");
}

static void test_build_exec_command_basic(void)
{
   char *cmd = NULL;
   assert(delegate_backend_docker_build_exec_command("aimee-delegate-task1", "echo hello", &cmd) ==
          0);
   assert(cmd != NULL);
   assert(strstr(cmd, "docker exec -i aimee-delegate-task1 bash -c") != NULL);
   assert(strstr(cmd, "base64 -d | bash") != NULL);
   /* Raw user command b64-encoded — must NOT appear directly. */
   assert(strstr(cmd, "echo hello") == NULL);
   free(cmd);
   printf("  PASS: test_build_exec_command_basic\n");
}

static void test_build_exec_command_handles_special_chars(void)
{
   const char *evil = "echo 'with quotes' && echo `backticks` && echo $HOME";
   char *cmd = NULL;
   assert(delegate_backend_docker_build_exec_command("c", evil, &cmd) == 0);
   /* Raw evil string MUST NOT appear; the b64 envelope hides it. */
   assert(strstr(cmd, "with quotes") == NULL);
   assert(strstr(cmd, "backticks") == NULL);
   free(cmd);
   printf("  PASS: test_build_exec_command_handles_special_chars\n");
}

static void test_build_exec_command_rejects_invalid(void)
{
   char *cmd = (char *)0x1;
   assert(delegate_backend_docker_build_exec_command(NULL, "x", &cmd) == -1);
   assert(cmd == NULL);
   cmd = (char *)0x1;
   assert(delegate_backend_docker_build_exec_command("c", NULL, &cmd) == -1);
   assert(cmd == NULL);
   cmd = (char *)0x1;
   assert(delegate_backend_docker_build_exec_command("", "x", &cmd) == -1);
   assert(cmd == NULL);
   assert(delegate_backend_docker_build_exec_command("c", "x", NULL) == -1);
   printf("  PASS: test_build_exec_command_rejects_invalid\n");
}

int main(void)
{
   printf("delegate_backend_docker:\n");
   test_register_puts_docker_in_registry();
   test_file_ops_reject_null_state();
   test_container_name_basic();
   test_container_name_sanitises_invalid_chars();
   test_container_name_rejects_invalid();
   test_build_exec_command_basic();
   test_build_exec_command_handles_special_chars();
   test_build_exec_command_rejects_invalid();
   test_acquire_creates_and_starts_container();
   test_release_hibernate_keeps_container();
   test_docker_exec_runs_through_fake();
   test_docker_exec_propagates_nonzero_exit();
   test_docker_exec_set_cwd_prefixes_subsequent_calls();
   test_acquire_rejects_invalid_args();
   test_docker_write_then_read_roundtrip();
   test_docker_path_validation_rejects_escapes();
   test_docker_list_dir_returns_entries();
   printf("ok\n");
   return 0;
}
