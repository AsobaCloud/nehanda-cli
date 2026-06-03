#ifndef DEC_TRACE_ANALYSIS_H
#define DEC_TRACE_ANALYSIS_H 1

/* Run trace mining on recent execution traces.
 * Detects retry loops, recovery sequences, and common tool sequences.
 * Records findings as anti-patterns or procedure memories.
 * Returns the number of patterns discovered, or -1 on error. */
int trace_mine(void);

#endif /* DEC_TRACE_ANALYSIS_H */
