/* sketch.c: approximate sketch primitives for ingest pre-filtering. */
#include "sketch.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

uint64_t sketch_fnv1a(const void *data, size_t len)
{
   const uint8_t *p = (const uint8_t *)data;
   uint64_t h = FNV_OFFSET;
   for (size_t i = 0; i < len; i++)
   {
      h ^= (uint64_t)p[i];
      h *= FNV_PRIME;
   }
   return h;
}

uint64_t sketch_fnv1a_seeded(const void *data, size_t len, uint64_t seed)
{
   uint64_t h = sketch_fnv1a(&seed, sizeof(seed));
   const uint8_t *p = (const uint8_t *)data;
   for (size_t i = 0; i < len; i++)
   {
      h ^= (uint64_t)p[i];
      h *= FNV_PRIME;
   }
   return h;
}

void sketch_bloom_init(sketch_bloom_t *b)
{
   memset(b->bits, 0, sizeof(b->bits));
   b->item_count = 0;
}

static void bloom_hashes(uint64_t h1, uint64_t h2, uint32_t out[SKETCH_BLOOM_K])
{
   uint64_t m = SKETCH_BLOOM_M_BITS;
   for (int i = 0; i < SKETCH_BLOOM_K; i++)
      out[i] = (uint32_t)((h1 + (uint64_t)i * h2) % m);
}

void sketch_bloom_add_hash(sketch_bloom_t *b, uint64_t h)
{
   uint64_t h2 = sketch_fnv1a_seeded(&h, sizeof(h), 0xdeadbeefcafe1234ULL);
   uint32_t positions[SKETCH_BLOOM_K];
   bloom_hashes(h, h2, positions);
   for (int i = 0; i < SKETCH_BLOOM_K; i++)
      b->bits[positions[i] >> 3] |= (uint8_t)(1u << (positions[i] & 7));
   b->item_count++;
}

int sketch_bloom_test_hash(const sketch_bloom_t *b, uint64_t h)
{
   uint64_t h2 = sketch_fnv1a_seeded(&h, sizeof(h), 0xdeadbeefcafe1234ULL);
   uint32_t positions[SKETCH_BLOOM_K];
   bloom_hashes(h, h2, positions);
   for (int i = 0; i < SKETCH_BLOOM_K; i++)
   {
      if (!(b->bits[positions[i] >> 3] & (uint8_t)(1u << (positions[i] & 7))))
         return 0;
   }
   return 1;
}

void sketch_minhash_init(sketch_minhash_t *sig)
{
   for (int i = 0; i < SKETCH_MINHASH_PERMUTATIONS; i++)
      sig->values[i] = UINT64_MAX;
}

void sketch_minhash_add_hash(sketch_minhash_t *sig, uint64_t h)
{
   for (int i = 0; i < SKETCH_MINHASH_PERMUTATIONS; i++)
   {
      uint64_t seed = 0x9e3779b97f4a7c15ULL ^ ((uint64_t)i * 0xbf58476d1ce4e5b9ULL);
      uint64_t v = sketch_fnv1a_seeded(&h, sizeof(h), seed);
      if (v < sig->values[i])
         sig->values[i] = v;
   }
}

void sketch_minhash_add_text(sketch_minhash_t *sig, const char *text, size_t shingle_len)
{
   size_t len;
   if (!sig || !text)
      return;
   len = strlen(text);
   if (shingle_len == 0)
      shingle_len = 5;
   if (len == 0)
      return;
   if (len < shingle_len)
   {
      sketch_minhash_add_hash(sig, sketch_fnv1a(text, len));
      return;
   }
   for (size_t i = 0; i + shingle_len <= len; i++)
      sketch_minhash_add_hash(sig, sketch_fnv1a(text + i, shingle_len));
}

double sketch_minhash_jaccard(const sketch_minhash_t *a, const sketch_minhash_t *b)
{
   int comparable = 0;
   int equal = 0;
   if (!a || !b)
      return 0.0;
   for (int i = 0; i < SKETCH_MINHASH_PERMUTATIONS; i++)
   {
      if (a->values[i] == UINT64_MAX && b->values[i] == UINT64_MAX)
         continue;
      comparable++;
      if (a->values[i] == b->values[i])
         equal++;
   }
   return comparable > 0 ? (double)equal / (double)comparable : 0.0;
}

uint64_t sketch_lsh_band_hash(const sketch_minhash_t *sig, int band)
{
   if (!sig || band < 0 || band >= SKETCH_LSH_BANDS)
      return 0;
   return sketch_fnv1a_seeded(&sig->values[band * SKETCH_LSH_ROWS_PER_BAND],
                              sizeof(uint64_t) * SKETCH_LSH_ROWS_PER_BAND,
                              0x6a09e667f3bcc909ULL ^ (uint64_t)band);
}

void sketch_count_min_init(sketch_count_min_t *cm)
{
   memset(cm, 0, sizeof(*cm));
}

void sketch_count_min_add_hash(sketch_count_min_t *cm, uint64_t h, uint32_t count)
{
   if (!cm || count == 0)
      return;
   for (int row = 0; row < SKETCH_COUNT_MIN_DEPTH; row++)
   {
      uint64_t seed = 0xa5a5a5a55a5a5a5aULL ^ ((uint64_t)row * 0x9e3779b97f4a7c15ULL);
      uint64_t rh = sketch_fnv1a_seeded(&h, sizeof(h), seed);
      uint32_t *slot = &cm->counters[row][rh % SKETCH_COUNT_MIN_WIDTH];
      uint32_t next = *slot + count;
      *slot = next < *slot ? UINT32_MAX : next;
   }
   {
      uint64_t next_count = cm->item_count + count;
      cm->item_count = next_count < cm->item_count ? UINT64_MAX : next_count;
   }
}

uint32_t sketch_count_min_estimate_hash(const sketch_count_min_t *cm, uint64_t h)
{
   uint32_t best = UINT32_MAX;
   if (!cm)
      return 0;
   for (int row = 0; row < SKETCH_COUNT_MIN_DEPTH; row++)
   {
      uint64_t seed = 0xa5a5a5a55a5a5a5aULL ^ ((uint64_t)row * 0x9e3779b97f4a7c15ULL);
      uint64_t rh = sketch_fnv1a_seeded(&h, sizeof(h), seed);
      uint32_t v = cm->counters[row][rh % SKETCH_COUNT_MIN_WIDTH];
      if (v < best)
         best = v;
   }
   return best == UINT32_MAX ? 0 : best;
}

void sketch_hll_init(sketch_hll_t *hll)
{
   memset(hll, 0, sizeof(*hll));
}

static uint8_t hll_rank(uint64_t x)
{
   uint8_t rank = 1;
   int max_rank = 64 - SKETCH_HLL_PRECISION + 1;
   while (rank < max_rank && (x & (1ULL << 63)) == 0)
   {
      rank++;
      x <<= 1;
   }
   return rank;
}

void sketch_hll_add_hash(sketch_hll_t *hll, uint64_t h)
{
   uint32_t idx;
   uint8_t rank;
   if (!hll)
      return;
   idx = (uint32_t)(h & (SKETCH_HLL_REGISTERS - 1));
   rank = hll_rank(h << SKETCH_HLL_PRECISION);
   if (rank > hll->registers[idx])
      hll->registers[idx] = rank;
   hll->item_count++;
}

double sketch_hll_estimate(const sketch_hll_t *hll)
{
   const double m = (double)SKETCH_HLL_REGISTERS;
   double sum = 0.0;
   int zeros = 0;
   double estimate;
   if (!hll)
      return 0.0;

   for (uint32_t i = 0; i < SKETCH_HLL_REGISTERS; i++)
   {
      uint8_t r = hll->registers[i];
      if (r == 0)
         zeros++;
      sum += ldexp(1.0, -(int)r);
   }
   if (sum <= DBL_MIN)
      return 0.0;

   estimate = 0.7213 / (1.0 + 1.079 / m) * m * m / sum;
   if (estimate <= 2.5 * m && zeros > 0)
      estimate = m * log(m / (double)zeros);
   return estimate;
}
