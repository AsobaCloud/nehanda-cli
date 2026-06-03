/* test_sketch.c: unit tests for live sketch primitives. */
#include "sketch.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char *read_fixture(const char *rel)
{
   char path[512];
   FILE *f;
   long len;
   char *buf;

   snprintf(path, sizeof(path), "../benchmarks/sketch/%s", rel);
   f = fopen(path, "rb");
   if (!f)
   {
      snprintf(path, sizeof(path), "benchmarks/sketch/%s", rel);
      f = fopen(path, "rb");
   }
   assert(f != NULL);
   assert(fseek(f, 0, SEEK_END) == 0);
   len = ftell(f);
   assert(len >= 0);
   assert(fseek(f, 0, SEEK_SET) == 0);
   buf = malloc((size_t)len + 1);
   assert(buf != NULL);
   assert(fread(buf, 1, (size_t)len, f) == (size_t)len);
   buf[len] = '\0';
   fclose(f);
   return buf;
}

static void test_fnv_deterministic(void)
{
   uint64_t a = sketch_fnv1a("hello", 5);
   uint64_t b = sketch_fnv1a("hello", 5);
   uint64_t c = sketch_fnv1a("world", 5);
   assert(a == b);
   assert(a != c);
}

static void test_seeded_hash_changes_seed(void)
{
   uint64_t a = sketch_fnv1a_seeded("key", 3, 1);
   uint64_t b = sketch_fnv1a_seeded("key", 3, 2);
   assert(a != b);
}

static void test_bloom_hash_add_and_test(void)
{
   sketch_bloom_t b;
   sketch_bloom_init(&b);

   uint64_t h = sketch_fnv1a("key1", 4);
   assert(sketch_bloom_test_hash(&b, h) == 0);

   sketch_bloom_add_hash(&b, h);
   assert(sketch_bloom_test_hash(&b, h) == 1);
   assert(b.item_count == 1);
}

static void test_bloom_multiple_hashes(void)
{
   sketch_bloom_t b;
   sketch_bloom_init(&b);

   const char *keys[] = {"a", "b", "c"};
   for (int i = 0; i < 3; i++)
      sketch_bloom_add_hash(&b, sketch_fnv1a(keys[i], strlen(keys[i])));

   assert(b.item_count == 3);
   for (int i = 0; i < 3; i++)
      assert(sketch_bloom_test_hash(&b, sketch_fnv1a(keys[i], strlen(keys[i]))) == 1);
}

static void test_minhash_similarity_and_lsh(void)
{
   sketch_minhash_t a, b, c;
   sketch_minhash_init(&a);
   sketch_minhash_init(&b);
   sketch_minhash_init(&c);

   sketch_minhash_add_text(&a, "alpha beta gamma delta epsilon", 5);
   sketch_minhash_add_text(&b, "alpha beta gamma delta epsilon", 5);
   sketch_minhash_add_text(&c, "zulu yankee xray whiskey victor", 5);

   assert(sketch_minhash_jaccard(&a, &b) == 1.0);
   assert(sketch_minhash_jaccard(&a, &c) < 0.5);
   assert(sketch_lsh_band_hash(&a, 0) == sketch_lsh_band_hash(&b, 0));
   assert(sketch_lsh_band_hash(&a, -1) == 0);
   assert(sketch_lsh_band_hash(&a, SKETCH_LSH_BANDS) == 0);

   sketch_minhash_t empty, short_text;
   sketch_minhash_init(&empty);
   sketch_minhash_init(&short_text);
   sketch_minhash_add_text(&empty, "", 5);
   sketch_minhash_add_text(&short_text, "abc", 5);
   assert(sketch_minhash_jaccard(&empty, &short_text) == 0.0);
}

static void test_count_min_frequency_estimates(void)
{
   static sketch_count_min_t cm;
   uint64_t apple = sketch_fnv1a("apple", 5);
   uint64_t banana = sketch_fnv1a("banana", 6);
   sketch_count_min_init(&cm);

   sketch_count_min_add_hash(&cm, apple, 5);
   sketch_count_min_add_hash(&cm, banana, 2);
   sketch_count_min_add_hash(&cm, apple, 3);

   assert(cm.item_count == 10);
   assert(sketch_count_min_estimate_hash(&cm, apple) >= 8);
   assert(sketch_count_min_estimate_hash(&cm, banana) >= 2);
   assert(sketch_count_min_estimate_hash(&cm, sketch_fnv1a("missing", 7)) == 0);

   sketch_count_min_add_hash(&cm, apple, 0);
   assert(cm.item_count == 10);

   cm.item_count = UINT64_MAX - 1;
   sketch_count_min_add_hash(&cm, apple, 5);
   assert(cm.item_count == UINT64_MAX);
}

static void test_hll_distinct_estimate(void)
{
   sketch_hll_t hll;
   char key[32];
   sketch_hll_init(&hll);

   for (int i = 0; i < 1000; i++)
   {
      snprintf(key, sizeof(key), "id-%d", i);
      sketch_hll_add_hash(&hll, sketch_fnv1a(key, strlen(key)));
   }
   for (int i = 0; i < 1000; i++)
   {
      snprintf(key, sizeof(key), "id-%d", i);
      sketch_hll_add_hash(&hll, sketch_fnv1a(key, strlen(key)));
   }

   double est = sketch_hll_estimate(&hll);
   assert(hll.item_count == 2000);
   assert(est > 900.0);
   assert(est < 1100.0);

   sketch_hll_t small;
   sketch_hll_init(&small);
   sketch_hll_add_hash(&small, sketch_fnv1a("one", 3));
   sketch_hll_add_hash(&small, sketch_fnv1a("two", 3));
   double small_est = sketch_hll_estimate(&small);
   assert(small_est > 1.0);
   assert(small_est < 4.0);
}

static void test_fixture_exact_duplicate_bloom(void)
{
   char *a = read_fixture("exact_duplicates/doc_a.txt");
   char *b = read_fixture("exact_duplicates/doc_b.txt");
   sketch_bloom_t bloom;
   uint64_t ha = sketch_fnv1a(a, strlen(a));
   uint64_t hb = sketch_fnv1a(b, strlen(b));
   assert(ha == hb);
   sketch_bloom_init(&bloom);
   assert(sketch_bloom_test_hash(&bloom, hb) == 0);
   sketch_bloom_add_hash(&bloom, ha);
   assert(sketch_bloom_test_hash(&bloom, hb) == 1);
   free(a);
   free(b);
}

static void test_fixture_lsh_recall_bands(void)
{
   char *near_a = read_fixture("near_duplicates/doc_a.txt");
   char *near_b = read_fixture("near_duplicates/doc_b.txt");
   char *distinct_a = read_fixture("distinct_text/doc_a.txt");
   char *distinct_b = read_fixture("distinct_text/doc_b.txt");
   sketch_minhash_t a, b, c, d;
   sketch_minhash_init(&a);
   sketch_minhash_init(&b);
   sketch_minhash_init(&c);
   sketch_minhash_init(&d);
   sketch_minhash_add_text(&a, near_a, 5);
   sketch_minhash_add_text(&b, near_b, 5);
   sketch_minhash_add_text(&c, distinct_a, 5);
   sketch_minhash_add_text(&d, distinct_b, 5);
   assert(sketch_minhash_jaccard(&a, &b) >= 0.75);
   assert(sketch_minhash_jaccard(&c, &d) < 0.5);
   int shared_band = 0;
   for (int band = 0; band < SKETCH_LSH_BANDS; band++)
      if (sketch_lsh_band_hash(&a, band) == sketch_lsh_band_hash(&b, band))
         shared_band = 1;
   assert(shared_band);
   free(near_a);
   free(near_b);
   free(distinct_a);
   free(distinct_b);
}

static void test_fixture_count_min_accuracy(void)
{
   char *tokens = read_fixture("count_min_frequencies/tokens.txt");
   sketch_count_min_t cm;
   sketch_count_min_init(&cm);
   for (char *tok = strtok(tokens, " \t\r\n"); tok; tok = strtok(NULL, " \t\r\n"))
      sketch_count_min_add_hash(&cm, sketch_fnv1a(tok, strlen(tok)), 1);
   assert(sketch_count_min_estimate_hash(&cm, sketch_fnv1a("alpha", 5)) == 5);
   assert(sketch_count_min_estimate_hash(&cm, sketch_fnv1a("beta", 4)) == 3);
   assert(sketch_count_min_estimate_hash(&cm, sketch_fnv1a("gamma", 5)) == 1);
   free(tokens);
}

static void test_fixture_hll_error(void)
{
   char *ids = read_fixture("hll_distinct_ids/ids.txt");
   sketch_hll_t hll;
   sketch_hll_init(&hll);
   for (char *id = strtok(ids, " \t\r\n"); id; id = strtok(NULL, " \t\r\n"))
   {
      sketch_hll_add_hash(&hll, sketch_fnv1a(id, strlen(id)));
      sketch_hll_add_hash(&hll, sketch_fnv1a(id, strlen(id)));
   }
   double est = sketch_hll_estimate(&hll);
   double rel_err = fabs(est - 10.0) / 10.0;
   assert(rel_err <= 0.10);
   free(ids);
}

static void test_bloom_false_positive_bound_100k(void)
{
   sketch_bloom_t bloom;
   int false_positives = 0;
   const int n = 100000;
   sketch_bloom_init(&bloom);
   for (int i = 0; i < n; i++)
   {
      char key[32];
      snprintf(key, sizeof(key), "positive-%06d", i);
      sketch_bloom_add_hash(&bloom, sketch_fnv1a(key, strlen(key)));
   }
   for (int i = 0; i < n; i++)
   {
      char key[32];
      snprintf(key, sizeof(key), "negative-%06d", i);
      false_positives += sketch_bloom_test_hash(&bloom, sketch_fnv1a(key, strlen(key)));
   }
   assert((double)false_positives / (double)n <= 0.01);
}

int main(void)
{
   printf("sketch: ");
   test_fnv_deterministic();
   test_seeded_hash_changes_seed();
   test_bloom_hash_add_and_test();
   test_bloom_multiple_hashes();
   test_minhash_similarity_and_lsh();
   test_count_min_frequency_estimates();
   test_hll_distinct_estimate();
   test_fixture_exact_duplicate_bloom();
   test_fixture_lsh_recall_bands();
   test_fixture_count_min_accuracy();
   test_fixture_hll_error();
   test_bloom_false_positive_bound_100k();
   printf("all tests passed\n");
   return 0;
}
