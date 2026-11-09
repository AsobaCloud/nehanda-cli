/** Optional real-provider integration helpers. Never skip when explicitly opted in. */
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'

export const requested = process.env.OLLAMA_E2E_TEST === '1'

export async function requireRealOllama() {
  const baseUrl = (process.env.OLLAMA_BASE_URL || 'http://127.0.0.1:11434/v1').replace(/\/+$/, '')
  const base = baseUrl.replace(/\/v1$/, '')
  const response = await fetch(`${base}/api/tags`, { signal: AbortSignal.timeout(5000) })
  if (!response.ok) throw new Error(`OLLAMA_E2E_TEST=1 but Ollama /api/tags returned HTTP ${response.status}`)
  const data = await response.json()
  const model = process.env.OLLAMA_E2E_MODEL || 'beam:cloud'
  const installed = (data.models || []).some(item => String(item.name || item.model || '').split(':latest')[0] === model)
  if (!installed) throw new Error(`OLLAMA_E2E_TEST=1 but the real Ollama model ${model} is not installed`)
  return { model, baseUrl }
}

export function workspace(label, phase) {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), `ollama-${label}-`))
  const cwd = path.join(temp, 'project')
  fs.mkdirSync(cwd, { recursive: true })
  const dbPath = path.join(temp, 'runtime.db')
  const db = openStore(dbPath)
  const cid = `conv-${label}`, sid = `sess-${label}`
  db.prepare('INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,?)').run(cid, cwd, phase)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  return {
    temp, cwd, dbPath, db, cid, sid,
    makeRt({ model, baseUrl }) { return {
      cwd, sessionId: sid, conversationId: cid, runtimeDbPath: dbPath,
      signal: AbortSignal.timeout(120_000),
      settings: { permissions: { defaultMode: 'default' }, model_config: {
        provider: 'ollama', custom_model_name: model, base_url: baseUrl, api_key: 'ollama',
      } },
    } },
    close() { closeStore(dbPath); fs.rmSync(temp, { recursive: true, force: true }) },
  }
}
