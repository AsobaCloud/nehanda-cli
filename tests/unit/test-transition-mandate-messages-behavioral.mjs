/** #36 U-3: every canonical boundary rejects with exact mandate and no mutation. */
import test from 'node:test'
import assert from 'node:assert/strict'
import { fixture } from '../fixtures/sdlcAcceptance.mjs'
import { setPhase, canTransition, getPhase, MANDATE_REJECT_REASONS } from '../../lib/workflow.mjs'
for (const [edge, reason] of Object.entries(MANDATE_REJECT_REASONS)) {
  test(`TM-${edge}: Given missing prerequisites, When advancing, Then exact rejection and unchanged phase`, () => {
    const [from,to] = edge.split('->'), f = fixture(`mandate-${from}`, from, from !== 'idle')
    try {
      const ctx = {}
      assert.equal(canTransition(f.db,f.cid,to,ctx),false)
      assert.equal(ctx.lastRejectionReason,reason)
      assert.equal(setPhase(f.db,f.cid,to,ctx),false)
      assert.equal(ctx.lastRejectionReason,reason)
      assert.equal(getPhase(f.db,f.cid),from)
      assert.deepEqual(f.rows('phase'),[])
    } finally { f.close() }
  })
}
