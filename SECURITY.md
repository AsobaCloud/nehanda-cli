# Security Policy

## Supported versions

Security fixes are provided for the latest commit on `main` and the most recent
release tag. Older tags and forks are supported on a best-effort basis.

| Version | Supported |
| --- | --- |
| Latest `main` | Yes |
| Latest release tag | Yes |
| Older tags | No |

## Reporting a vulnerability

**Please do not open public GitHub issues for security vulnerabilities.**

Report security issues in one of these ways:

1. **GitHub Security Advisories (preferred):**
   [Create a private advisory](https://github.com/AsobaCloud/nehanda-cli/security/advisories/new)
2. **Email:** support@asoba.co with subject line `nehanda-cli security`

We aim to acknowledge reports within **3 business days** and will work with you on
coordinated disclosure.

### What to include

- Description of the issue and potential impact
- Steps to reproduce, or a proof of concept if available
- Affected component (`nehanda` client, `nehanda-server`, `nehanda-kb`, `install.sh`,
  etc.)
- Your nehanda-cli version or commit SHA (`nehanda --version` or `git rev-parse HEAD`)
- Your environment (OS, install method)

## Scope

### In scope

Vulnerabilities in open-source components **in this repository**, including:

- Local binaries (`nehanda`, `nehanda-server`, `nehanda-kb`)
- Install and service scripts (`install.sh`, `scripts/`)
- Nehanda-specific code (`src/`, `patches/`)
- Local auth token storage and header injection (`nehanda_auth.c`,
  `nehanda_header_inject.c`)
- Unix-domain socket and local HTTP listeners bound to localhost
- Supply-chain issues in this repo's build or dependency fetch paths

### Out of scope

The following are proprietary and maintained separately. Report issues there through
their own channels:

- **Nehanda Gateway** and hosted inference at `nehanda.asoba.co`
- **Model weights**, fine-tune data, and vLLM deployment configuration
- **ONA/Zorora auth** infrastructure (device pairing Lambda, JWT validation,
  billing, DynamoDB)

Misconfigurations in your own environment (exposed API keys, world-readable
`~/.config/aimee/`, running services on non-localhost interfaces without a firewall)
are generally out of scope unless nehanda-cli defaults are unsafe.

## Safe harbor

We support good-faith security research on in-scope components. Do not access data
you do not own, degrade service for other users, or exfiltrate production credentials.

## Security practices for operators

- Keep `nehanda-cli` updated from `main` or the latest release
- Restrict filesystem permissions on `~/.config/aimee/` (session tokens, agent config)
- Run local services (`nehanda-server`, `nehanda-kb`, embedder) on localhost only unless
  you understand the exposure
- Do not commit API keys, JWTs, or `.env` files with secrets

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for operational guidance.
