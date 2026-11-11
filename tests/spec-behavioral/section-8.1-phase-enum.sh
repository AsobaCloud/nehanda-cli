#!/usr/bin/env bash
# #36 U-3: canonical vocabulary through real transition validation.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$SCRIPT_DIR/../.."
node --input-type=module <<'JS'
import assert from 'node:assert/strict'
import {fixture} from './tests/fixtures/sdlcAcceptance.mjs'
import {canTransition} from './lib/workflow.mjs'
const phases=['idle','explore','diagnose','create_issue','create_tests','planning','implement','verify','safety','confirm_fix','done']
const f=fixture('enum','idle')
try{
 for(const phase of phases){
  const ctx={};canTransition(f.db,f.cid,phase,ctx)
  assert.notEqual(ctx.lastRejectionReason,`Invalid phase: ${phase}`)
 }
 for(const phase of ['test','unknown','']){
  const ctx={};assert.equal(canTransition(f.db,f.cid,phase,ctx),false)
  assert.equal(ctx.lastRejectionReason,`Invalid phase: ${phase}`)
 }
 console.log('PASS ENUM: eleven canonical phases recognized; overloaded test/unknown rejected')
}finally{f.close()}
JS
