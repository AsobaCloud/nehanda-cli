#!/usr/bin/env node
/** #36/#37: continue-on-failure acceptance runner with evidence and explicit blockers.
 * --issues 36,37: complete mapped unit/integration/spec inventory.
 * --unit: mapped unit cases only; --syntax: parse cases and test fixtures.
 * --transport fetch: external-provider Response fixture (no socket requirement).
 * --json /tmp/report.json: full logs + assertion outcomes + requirement mapping.
 * Optional D1/D2/D3 run only with OLLAMA_E2E_TEST=1; never counted as passes otherwise.
 */
import fs from 'node:fs'
import path from 'node:path'
import {spawn,spawnSync} from 'node:child_process'
import crypto from 'node:crypto'
import {fileURLToPath} from 'node:url'
import {issueSuites,requirements} from './acceptance-36-37-manifest.mjs'
const root=fileURLToPath(new URL('../../',import.meta.url)),args=process.argv.slice(2)
const option=name=>args.includes(name)?args[args.indexOf(name)+1]:null
const syntax=args.includes('--syntax'),list=args.includes('--list'),scoped=args.includes('--issues'),unit=args.includes('--unit')
let suite=scoped?issueSuites:[...fs.readdirSync(path.join(root,'tests/integration')).filter(f=>/^test-.+\.mjs$/.test(f)).sort().map(f=>'tests/integration/'+f),'tests/test-explore-loop-jev-mem-behavioral.mjs']
if(unit)suite=suite.filter(f=>f.startsWith('tests/unit/'))
// A custom manifest permits isolated behavioral tests of runner failure handling.
if(option('--manifest'))suite=JSON.parse(fs.readFileSync(option('--manifest'),'utf8'))
if(syntax)suite=[...suite,...fs.readdirSync(path.join(root,'tests/fixtures')).filter(f=>f.endsWith('.mjs')).map(f=>'tests/fixtures/'+f)]
suite=[...new Set(suite)]
const results=[]
const environment={...process.env, ...(option('--transport')?{NEHANDA_TEST_TRANSPORT:option('--transport')}:{})}
async function run(file){
 const absolute=path.resolve(root,file),shell=file.endsWith('.sh')
 return await new Promise(resolve=>{
  const child=spawn(shell?'bash':process.execPath,shell?[...(syntax?['-n']:[]),absolute]:[...(syntax?['--check']:[]),absolute],{
   cwd:root,env:environment,detached:process.platform!=='win32',stdio:['ignore','pipe','pipe'],
  })
  let stdout='',stderr='',timedOut=false,spawnError
  const limit=Number(process.env.NEHANDA_ACCEPTANCE_TIMEOUT_MS)||180000
  const timer=setTimeout(()=>{timedOut=true;try{process.kill(-child.pid,'SIGKILL')}catch{child.kill('SIGKILL')}},limit)
  child.stdout.on('data',d=>{stdout=(stdout+d).slice(-4*1024*1024)})
  child.stderr.on('data',d=>{stderr=(stderr+d).slice(-4*1024*1024)})
  child.on('error',e=>{spawnError=e.message})
  child.on('close',(exitCode,signal)=>{clearTimeout(timer);resolve({exitCode,signal,stdout,stderr,timedOut,spawnError})})
 })
}
function classify(result){
 const text=result.stdout+'\n'+result.stderr
 if(result.exitCode===0&&!result.spawnError&&!result.timedOut)return 'PASS'
 if(/(?:listen|connect) EPERM|NODE_MODULE_VERSION|ECONNREFUSED|ENOTFOUND/.test(text))return 'BLOCKED'
 if(result.spawnError||result.timedOut)return 'HARNESS_ERROR'
 if(!/AssertionError|ERR_ASSERTION/.test(text)&&/SyntaxError:|ReferenceError:|TypeError:|SqliteError|ERR_MODULE_NOT_FOUND|ERR_TEST_FAILURE/.test(text))return 'HARNESS_ERROR'
 return 'RED'
}
for(const file of suite){
 if(list){console.log(file);continue}
 const optional=/test-d[123]-ollama-/.test(file)
 if(!syntax&&optional&&process.env.OLLAMA_E2E_TEST!=='1'){
  results.push({file,status:'NOT_RUN',reason:'optional real Ollama; set OLLAMA_E2E_TEST=1'});console.log('NOT_RUN '+file);continue
 }
 const started=Date.now(),result=await run(file)
 let status=classify(result)
 const assertions=[]
 const text=result.stdout+'\n'+result.stderr
 for(const line of text.split('\n')){
  const tap=line.match(/^(not ok|ok) \d+ - (.+)$/)
  if(tap)assertions.push({name:tap[2],status:tap[1]==='ok'?'PASS':'RED'})
  const old=line.match(/^[✓✗] (.+)$/)
  if(old)assertions.push({name:old[1],status:line.startsWith('✓')?'PASS':'RED'})
  const labeled=line.match(/^(.+): (PASS|FAIL)$/)
  if(labeled)assertions.push({name:labeled[1],status:labeled[2]==='PASS'?'PASS':'RED'})
 }
 if(status==='PASS'&&assertions.some(a=>a.status==='RED'))status='HARNESS_ERROR'
 let sha256=null;try{sha256=crypto.createHash('sha256').update(fs.readFileSync(path.resolve(root,file))).digest('hex')}catch{}
 results.push({file,status,sha256,elapsedMs:Date.now()-started,assertions,...result})
 console.log(`${status} ${file}`)
 if(status!=='PASS')console.error(text.slice(-6000))
}
if(!list){
 const counts={};for(const r of results)counts[r.status]=(counts[r.status]||0)+1
 const head=spawnSync('git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).stdout?.trim()||null
 const report={head,node:process.version,transport:environment.NEHANDA_TEST_TRANSPORT||'socket',syntax,createdAt:new Date().toISOString(),requirements:scoped?requirements:[],counts,results}
 const json=option('--json')
 if(json){fs.mkdirSync(path.dirname(path.resolve(json)),{recursive:true});fs.writeFileSync(json,JSON.stringify(report,null,2)+'\n')}
 console.log('Acceptance: '+JSON.stringify(counts))
 if(results.some(r=>!['PASS','NOT_RUN'].includes(r.status))||results.length===0)process.exitCode=1
}
