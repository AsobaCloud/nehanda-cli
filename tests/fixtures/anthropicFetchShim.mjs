// #36/#37 provider regression fixture: keep the real SDK and SSE parser while
// selecting its public Web Fetch shim for restricted, socket-free runners.
const originalFetch = globalThis.fetch
const dispatch = (...args) => (globalThis.fetch === dispatch ? originalFetch : globalThis.fetch)(...args)
globalThis.fetch = dispatch
try {
  await import('@anthropic-ai/sdk/shims/web')
} finally {
  globalThis.fetch = originalFetch
}
