#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmd_branch.h"

/* Helper to free branch files in a graph */
static void free_branch_files(conflict_graph_t *g)
{
   branch_free_edges(g);
   for (int i = 0; i < g->branch_count; i++)
      for (int f = 0; f < g->branches[i].file_count; f++)
         free(g->branches[i].files[f]);
}

/* --- Test: conflict graph construction --- */

static void test_conflict_graph_no_conflicts(void)
{
   conflict_graph_t *g = calloc(1, sizeof(*g));
   assert(g);

   strcpy(g->branches[0].name, "feature-a");
   g->branches[0].files[0] = strdup("src/file1.c");
   g->branches[0].file_count = 1;

   strcpy(g->branches[1].name, "feature-b");
   g->branches[1].files[0] = strdup("src/file2.c");
   g->branches[1].file_count = 1;

   g->branch_count = 2;

   branch_build_conflict_graph(g);

   assert(g->edge_count == 0);
   assert(g->branches[0].conflict_count == 0);
   assert(g->branches[1].conflict_count == 0);

   free_branch_files(g);
   free(g);
   printf("  PASS: test_conflict_graph_no_conflicts\n");
}

static void test_conflict_graph_with_conflicts(void)
{
   conflict_graph_t *g = calloc(1, sizeof(*g));
   assert(g);

   strcpy(g->branches[0].name, "feature-a");
   g->branches[0].files[0] = strdup("src/db.c");
   g->branches[0].files[1] = strdup("src/api.c");
   g->branches[0].file_count = 2;

   strcpy(g->branches[1].name, "feature-b");
   g->branches[1].files[0] = strdup("src/db.c");
   g->branches[1].files[1] = strdup("Makefile");
   g->branches[1].file_count = 2;

   strcpy(g->branches[2].name, "feature-c");
   g->branches[2].files[0] = strdup("README.md");
   g->branches[2].file_count = 1;

   g->branch_count = 3;

   branch_build_conflict_graph(g);

   assert(g->edge_count == 1);
   assert(g->edges[0].shared_file_count == 1);
   assert(strcmp(g->edges[0].shared_files[0], "src/db.c") == 0);

   assert(g->branches[0].conflict_count == 1);
   assert(g->branches[1].conflict_count == 1);
   assert(g->branches[2].conflict_count == 0);

   /* Merge order: C first (0 conflicts), then A and B (1 each) */
   assert(g->branches[g->merge_order[0]].conflict_count == 0);
   assert(strcmp(g->branches[g->merge_order[0]].name, "feature-c") == 0);

   free_branch_files(g);
   free(g);
   printf("  PASS: test_conflict_graph_with_conflicts\n");
}

static void test_conflict_graph_merge_order(void)
{
   conflict_graph_t *g = calloc(1, sizeof(*g));
   assert(g);

   /* 4 branches sharing a file, plus 1 clean branch */
   strcpy(g->branches[0].name, "branch-a");
   g->branches[0].files[0] = strdup("shared.c");
   g->branches[0].file_count = 1;

   strcpy(g->branches[1].name, "branch-b");
   g->branches[1].files[0] = strdup("shared.c");
   g->branches[1].files[1] = strdup("common.h");
   g->branches[1].file_count = 2;

   strcpy(g->branches[2].name, "branch-c");
   g->branches[2].files[0] = strdup("shared.c");
   g->branches[2].files[1] = strdup("common.h");
   g->branches[2].file_count = 2;

   strcpy(g->branches[3].name, "branch-d");
   g->branches[3].files[0] = strdup("shared.c");
   g->branches[3].file_count = 1;

   strcpy(g->branches[4].name, "branch-clean");
   g->branches[4].files[0] = strdup("isolated.c");
   g->branches[4].file_count = 1;

   g->branch_count = 5;

   branch_build_conflict_graph(g);

   /* branch-clean should be first in merge order (0 conflicts) */
   assert(strcmp(g->branches[g->merge_order[0]].name, "branch-clean") == 0);
   assert(g->branches[g->merge_order[0]].conflict_count == 0);

   free_branch_files(g);
   free(g);
   printf("  PASS: test_conflict_graph_merge_order\n");
}

static void test_empty_graph(void)
{
   conflict_graph_t *g = calloc(1, sizeof(*g));
   assert(g);
   g->branch_count = 0;

   branch_build_conflict_graph(g);

   assert(g->edge_count == 0);
   free(g);
   printf("  PASS: test_empty_graph\n");
}

/* --- Main --- */

int main(void)
{
   printf("test_cmd_branch:\n");

   test_conflict_graph_no_conflicts();
   test_conflict_graph_with_conflicts();
   test_conflict_graph_merge_order();
   test_empty_graph();

   printf("All cmd_branch tests passed.\n");
   return 0;
}
