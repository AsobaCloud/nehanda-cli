// Issue #39: forward proxy for the OS-primitives sandbox.
// Listens on a unix domain socket inside the per-run sandbox directory.
// HTTP absolute-URI forwarding and HTTPS CONNECT; exact-host, case-insensitive
// allowlist; an empty allowlist denies all egress. The allowlist is operator
// configuration (NEHANDA_SANDBOX_ALLOWLIST) — sandboxed processes never mutate it.
// Note: no mainstream HTTP client honors a unix-socket proxy variable, so this is
// a raw-socket endpoint for clients that can target a socket path (node today).
// Direct egress stays denied by the sandbox profile regardless.
import net from 'node:net'

export function parseAllowlist (value) {
  if (!value) return new Set()
  return new Set(String(value).split(',').map(h => h.trim().toLowerCase()).filter(Boolean))
}

const HEADER_LIMIT = 16 * 1024
const deny = (client, statusLine) => {
  try { client.end(`HTTP/1.1 ${statusLine}\r\nContent-Length: 0\r\nConnection: close\r\n\r\n`) } catch { client.destroy() }
}

export function startAllowlistProxy ({ socketPath, allowlist }) {
  const allowed = host => !!host && allowlist.has(String(host).toLowerCase())
  const parseAuthority = target => {
    const m = String(target).match(/^([^:\[\]]+|\[[0-9a-f:]+\])(?::(\d{1,5}))?$/i)
    if (!m) return null
    const port = m[2] ? Number(m[2]) : 443
    if (port < 1 || port > 65535) return null
    return { host: m[1].toLowerCase(), port }
  }
  const server = net.createServer(client => {
    let head = Buffer.alloc(0), done = false
    const finish = fn => { if (!done) { done = true; client.removeListener('data', onData); fn() } }
    const onData = chunk => {
      head = Buffer.concat([head, chunk])
      const idx = head.indexOf('\r\n\r\n')
      if (idx === -1) { if (head.length > HEADER_LIMIT) finish(() => deny(client, '400 Bad Request')); return }
      finish(() => {
        const headerText = head.subarray(0, idx).toString('latin1')
        const body = head.subarray(idx + 4)
        const requestLine = headerText.split('\r\n')[0] || ''
        const m = requestLine.match(/^([A-Z]+)\s+(\S+)\s+HTTP\//i)
        if (!m) return deny(client, '400 Bad Request')
        const method = m[1].toUpperCase(), target = m[2]
        let host = null, port = null, connect = false
        if (method === 'CONNECT') {
          const authority = parseAuthority(target); if (!authority) return deny(client, '400 Bad Request')
          host = authority.host; port = authority.port; connect = true
        } else {
          let url; try { url = new URL(target) } catch { return deny(client, '400 Bad Request') }
          if (url.protocol !== 'http:' && url.protocol !== 'https:') return deny(client, '400 Bad Request')
          host = url.hostname.toLowerCase(); port = Number(url.port || (url.protocol === 'https:' ? 443 : 80))
        }
        if (!allowed(host)) return deny(client, '403 Forbidden')
        const upstream = net.connect({ host, port }, () => {
          if (connect) { client.write('HTTP/1.1 200 Connection Established\r\n\r\n'); if (body.length) upstream.write(body) } else { upstream.write(head) }
          client.pipe(upstream).pipe(client)
        })
        upstream.on('error', () => deny(client, '502 Bad Gateway'))
      })
    }
    client.on('data', onData)
    client.on('error', () => client.destroy())
  })
  return new Promise((resolve, reject) => {
    server.once('error', reject)
    server.listen(socketPath, () => resolve({
      socketPath,
      close: () => new Promise(res => server.close(() => res())),
    }))
  })
}
