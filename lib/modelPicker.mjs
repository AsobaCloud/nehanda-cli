/**
 * Pure navigation logic for the /model wizard's large model picker (#10).
 *
 * Grouping, filtering, windowing (HP-2) and ask-prompt identity (HP-1a) live
 * here as pure functions so they are directly testable. The React wiring is
 * in bin/nehanda-ui.mjs (ModelPicker component / bridge.ask).
 *
 * HP-1a: every wizard question gets a unique ask id used as the React key for
 *        AskPrompt, forcing a remount between questions so the typed value of
 *        the previous question (e.g. an API key) can never carry into the
 *        next prompt's input field.
 * HP-2:  discovered lists longer than PICKER_THRESHOLD render in a picker with
 *        vendor groups, type-to-filter search, and a centered window.
 */

/** Select lists longer than this switch to the grouped/filterable picker. */
export const PICKER_THRESHOLD = 12

/** Maximum rows rendered at once in browse/search views. */
export const PICKER_WINDOW = 12

/**
 * Monotonic per-question id source for AskPrompt React keys (HP-1a).
 * Distinct ids across consecutive questions ⇒ AskPrompt remounts ⇒ value
 * state resets to '' for every new prompt.
 */
export function createAskIdSequencer() {
  let n = 0
  return {
    next() { return ++n },
  }
}

/**
 * Vendor prefix of a discovered model id.
 * OpenRouter style `vendor/model` → `vendor`; Ollama style `vendor:tag` →
 * `vendor`; separator-less ids are their own single-model group.
 * @param {string} name
 */
export function vendorOfModelId(name) {
  const s = String(name ?? '')
  const sep = s.includes('/') ? s.indexOf('/') : s.indexOf(':')
  return sep === -1 ? s : s.slice(0, sep)
}

/**
 * Group discovered model items by vendor (HP-2).
 * Groups sort by size desc, then vendor name; the `__custom__` entry is
 * always pinned last so it is reachable without scrolling.
 * @param {{ id: string, name?: string, label?: string, desc?: string }[]} items
 */
export function groupModelsByVendor(items) {
  const custom = items.find(i => i && i.id === '__custom__') || {
    id: '__custom__',
    label: '(enter custom name…)',
    desc: 'Type a model id not listed above',
  }
  const real = items.filter(i => i && i.id !== '__custom__')
  const byVendor = new Map()
  for (const it of real) {
    const vendor = vendorOfModelId(it.name ?? it.label ?? it.id ?? '')
    if (!vendor) continue
    if (!byVendor.has(vendor)) byVendor.set(vendor, [])
    byVendor.get(vendor).push(it)
  }
  const groups = [...byVendor.entries()].map(([vendor, models]) => ({
    id: vendor,
    label: vendor,
    desc: `${models.length} model${models.length === 1 ? '' : 's'}`,
    models,
    isCustom: false,
  }))
  groups.sort((a, b) => (b.models.length - a.models.length) || a.id.localeCompare(b.id))
  groups.push({
    id: '__custom__',
    label: custom.label || '(enter custom name…)',
    desc: 'Type a model id not listed above',
    models: [custom],
    isCustom: true,
  })
  return groups
}

/**
 * Case-insensitive substring filter for type-to-search (HP-2).
 * The `__custom__` entry is always pinned last, even with no matches, so the
 * user can always fall back to typing a custom model id.
 * @param {{ id: string, name?: string, label?: string }[]} items
 * @param {string} query
 */
export function filterModelItems(items, query) {
  const q = String(query ?? '').trim().toLowerCase()
  const custom = items.find(i => i && i.id === '__custom__')
  const real = items.filter(i => i && i.id !== '__custom__')
  const matched = q
    ? real.filter(i => String(i.name ?? i.label ?? '').toLowerCase().includes(q))
    : real
  return custom ? [...matched, custom] : matched
}

/**
 * Centered window over a list for windowed rendering (HP-2).
 * Keeps the cursor near the middle once the list overflows the window and
 * reports hidden rows above/below for "…N more" hints.
 * @param {any[]} list
 * @param {number} cursor
 * @param {number} [max] window size (default PICKER_WINDOW)
 */
export function visibleWindow(list, cursor, max = PICKER_WINDOW) {
  const len = list.length
  if (len <= max) {
    return { start: 0, visible: list, hiddenAbove: 0, hiddenBelow: 0 }
  }
  const half = Math.floor(max / 2)
  let start = cursor - half
  if (start < 0) start = 0
  if (start > len - max) start = len - max
  return {
    start,
    visible: list.slice(start, start + max),
    hiddenAbove: start,
    hiddenBelow: len - (start + max),
  }
}
