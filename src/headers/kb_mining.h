/* kb_mining.h: aimee-kb continuous mining scheduler and jobs. */
#ifndef DEC_KB_MINING_H
#define DEC_KB_MINING_H 1

#ifdef __cplusplus
extern "C"
{
#endif

   int kb_mining_run_once(void);
   int kb_mining_start(int min_poll_s);
   void kb_mining_stop(void);
   int kb_mining_active(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_KB_MINING_H */
