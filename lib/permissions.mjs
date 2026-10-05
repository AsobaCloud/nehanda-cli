/** §5.12 — permission evaluation (deny > ask > allow > defaultMode). */

// ── Session-scoped approval cache ────────────────────────────
//
// Allows a user who selects [A] in BlockConfirmMenu to suppress subsequent
// prompts for the same operation within one REPL session.  Keys are arbitrary
// strings — callers use a stable pattern like `Bash:sudo` or `PythonSafetyChecker`.
//
const _sessionApprovals = new Map()

/**
 * Record a single-session approval for key.
 * @param {string} sessionId
 * @param {string} key  — e.g. 'Bash:sudo' or 'ShellSafetyChecker'
 */
export function cacheSessionApproval(sessionId, key) {
  const bucket = _sessionApprovals.get(sessionId) || new Set()
  bucket.add(key)
  _sessionApprovals.set(sessionId, bucket)
}

/**
 * Return true if a prior approval for key exists in this session.
 * @param {string} sessionId
 * @param {string} key
 */
export function hasSessionApproval(sessionId, key) {
  return _sessionApprovals.get(sessionId)?.has(key) ?? false
}

/** §5.12 — permission evaluation (deny > ask > allow > defaultMode). */
export function evaluatePermission(permissions, toolName, _toolInput, phase) {
  const p = permissions || { defaultMode: 'default' }
  const defaultMode = p.defaultMode || 'default'

  if (matchesAny(p.deny, toolName)) return 'deny'
  if (matchesAny(p.ask, toolName)) return 'ask'
  if (matchesAny(p.allow, toolName)) return 'allow'

  switch (defaultMode) {
    case 'bypassPermissions': return 'allow'
    case 'dontAsk': return 'deny'
    case 'acceptEdits':
      return (toolName === 'Read' || toolName === 'Write' || toolName === 'Edit') ? 'allow' : 'ask'
    case 'plan':
      return (toolName === 'Write' || toolName === 'Edit' || toolName === 'Bash' || toolName === 'NotebookEdit') ? 'deny' : 'ask'
    case 'default':
    default: {
      // In implement/test phases the user already approved the plan — auto-allow all execution tools
      if (phase === 'implement' || phase === 'test') return 'allow'

      // Read-only/safe tools auto-allowed in all other modes
      const AUTO_ALLOW = new Set(['Read', 'Glob', 'Grep', 'WebFetch', 'WebSearch',
        'ToolSearch', 'ListMcpResources', 'ReadMcpResource',
        'AskUserQuestion', 'Brief', 'TodoWrite', 'TaskOutput', 'TaskStop', 'Skill'])
      return AUTO_ALLOW.has(toolName) ? 'allow' : 'ask'
    }
  }
}

function matchesAny(rules, toolName) {
  if (!Array.isArray(rules)) return false
  for (const r of rules) {
    if (typeof r !== 'string' || !r.trim()) continue
    if (ruleMatches(r.trim(), toolName)) return true
  }
  return false
}

function ruleMatches(rule, toolName) {
  if (rule === toolName) return true
  if (rule.endsWith('*')) return toolName.startsWith(rule.slice(0, -1))
  return false
}
