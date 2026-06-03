#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db1.h"

int main(void)
{
   printf("local_resolution: ");
   assert(db1_init(":memory:") == 0);

   {
      assert(db1_local_operator_upsert("secret://work", "op-work", 0, "Work") == 0);
      assert(db1_local_operator_upsert("secret://personal", "op-personal", 1, "Personal") == 0);

      db1_local_operator_t row;
      assert(db1_local_operator_get("secret://personal", &row) == 0);
      assert(strcmp(row.operator_uuid, "op-personal") == 0);
      assert(row.active == 1);
      assert(strcmp(row.display_hint, "Personal") == 0);

      assert(db1_local_operator_get_active(&row) == 0);
      assert(strcmp(row.secret_ref, "secret://personal") == 0);

      assert(db1_local_operator_set_active("secret://work") == 0);
      assert(db1_local_operator_get("secret://work", &row) == 0);
      assert(row.active == 1);
      assert(db1_local_operator_get("secret://personal", &row) == 0);
      assert(row.active == 0);

      db1_local_operator_t rows[8];
      int count = db1_local_operator_list(rows, 8);
      assert(count == 2);
      assert(strcmp(rows[0].secret_ref, "secret://work") == 0);
      assert(rows[0].active == 1);

      assert(db1_local_operator_delete("secret://personal") == 0);
      assert(db1_local_operator_get("secret://personal", &row) != 0);
   }

   {
      assert(db1_project_clone_upsert("/tmp/a", "proj-1", "https://example.com/a",
                                      "git@example.com:a.git", "") == 0);
      assert(db1_project_clone_upsert("/tmp/b", "proj-1", "https://example.com/a",
                                      "git@example.com:b.git", "git@example.com:up.git") == 0);
      assert(db1_project_clone_upsert("/tmp/c", "proj-2", "", "", "") == 0);

      db1_project_clone_t row;
      assert(db1_project_clone_get("/tmp/a", &row) == 0);
      assert(strcmp(row.project_uuid, "proj-1") == 0);
      assert(strcmp(row.canonical_url, "https://example.com/a") == 0);

      db1_project_clone_t rows[8];
      int count = db1_project_clone_list(rows, 8);
      assert(count == 3);

      count = db1_project_clone_list_by_project("proj-1", rows, 8);
      assert(count == 2);
      assert(strcmp(rows[0].project_uuid, "proj-1") == 0);
      assert(strcmp(rows[1].project_uuid, "proj-1") == 0);

      assert(db1_project_clone_upsert("/tmp/a", "proj-3", "https://example.com/z", "", "") == 0);
      assert(db1_project_clone_get("/tmp/a", &row) == 0);
      assert(strcmp(row.project_uuid, "proj-3") == 0);
      assert(strcmp(row.canonical_url, "https://example.com/z") == 0);

      assert(db1_project_clone_delete("/tmp/b") == 0);
      assert(db1_project_clone_get("/tmp/b", &row) != 0);
   }

   {
      assert(db1_tool_local_availability_set("tool-1", 1, "/usr/bin/tool-1") == 0);
      assert(db1_tool_local_availability_set("tool-2", 0, "") == 0);

      db1_tool_local_availability_t row;
      assert(db1_tool_local_availability_get("tool-1", &row) == 0);
      assert(row.usable == 1);
      assert(strcmp(row.binary_path, "/usr/bin/tool-1") == 0);

      assert(db1_tool_local_availability_set("tool-1", 0, "/opt/tool-1") == 0);
      assert(db1_tool_local_availability_get("tool-1", &row) == 0);
      assert(row.usable == 0);
      assert(strcmp(row.binary_path, "/opt/tool-1") == 0);

      db1_tool_local_availability_t rows[8];
      int count = db1_tool_local_availability_list(rows, 8);
      assert(count == 2);

      assert(db1_tool_local_availability_delete("tool-2") == 0);
      assert(db1_tool_local_availability_get("tool-2", &row) != 0);
   }

   db1_shutdown();
   printf("PASS\n");
   return 0;
}
