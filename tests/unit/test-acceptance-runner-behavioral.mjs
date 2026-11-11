/** #36/#37: runner must preserve RED, harness errors, later execution and logs. */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import {spawnSync} from 'node:child_process'
import {fileURLToPath} from 'node:url'

test('RUNNER-1: Given RED then bad import then PASS, When running, Then execute all and classify separately',()=>{
 const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'runner-contract-'))
 try{
  const files=['red.mjs','broken.mjs','green.mjs'].map(n=>path.join(tmp,n))
  fs.writeFileSync(files[0],"import assert from 'node:assert/strict';assert.equal(1,2)\n")
  fs.writeFileSync(files[1],"import './nonexistent.mjs'\n")
  fs.writeFileSync(files[2],"console.log('later suite executed')\n")
  const manifest=path.join(tmp,'cases.json'),json=path.join(tmp,'report.json')
  fs.writeFileSync(manifest,JSON.stringify(files))
  const result=spawnSync(process.execPath,[fileURLToPath(new URL('../integration/run-acceptance.mjs',import.meta.url)),'--manifest',manifest,'--json',json],{encoding:'utf8',timeout:10000})
  assert.equal(result.status,1)
  const report=JSON.parse(fs.readFileSync(json))
  assert.deepEqual(report.results.map(r=>r.status),['RED','HARNESS_ERROR','PASS'])
  assert.match(report.results[2].stdout,/later suite executed/)
  assert.match(report.results[0].stderr,/ERR_ASSERTION/)
  assert.equal(report.results.length,3)
 }finally{fs.rmSync(tmp,{recursive:true,force:true})}
})
