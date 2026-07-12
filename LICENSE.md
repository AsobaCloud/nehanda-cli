# License

**SPDX-Identifier:** `AGPL-3.0-only`

Copyright (C) 2026 Asoba Corporation

nehanda-cli is free software: you may redistribute it and/or modify it under the
terms of the [GNU Affero General Public License v3.0](https://www.gnu.org/licenses/agpl-3.0.html)
or (at your option) any later version.

The full license text is maintained verbatim in the [LICENSE](LICENSE) file at the
repository root. A copy is also available from the Free Software Foundation at
<https://www.gnu.org/licenses/agpl-3.0.txt>.

## What this covers

This license applies to **nehanda-cli** — the open-source client and local server
stack in this repository, including:

- `nehanda`, `nehanda-server`, and `nehanda-kb` binaries built from this tree
- Nehanda-specific code under `src/`
- Patches under `patches/` and the `upstream/` aimee subtree as distributed here
- Documentation, configuration examples, and install scripts in this repository

If you modify nehanda-cli and make it available to users over a network, AGPL-3.0
requires you to provide corresponding source to those users. See Section 13 of the
AGPL for the network-use provision.

## AGPL boundary

nehanda-cli is designed so that proprietary services communicate over standard HTTP
using the OpenAI/Anthropic wire protocol. The following are **not** part of this
repository and are **not** licensed under AGPL-3.0:

- Nehanda Gateway (HTTPS proxy in front of the inference endpoint)
- Nehanda model weights and fine-tune data
- Hosted inference at `nehanda.asoba.co`
- ONA/Zorora auth services (`ona-user-auth`, `ona-platform-users`, device pairing)

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the three-tier design and where
the AGPL line sits.

## Upstream — aimee

nehanda-cli is a fork of [aimee](https://github.com/RakuenSoftware/aimee), Copyright
(C) 2026 The aimee authors, also licensed under AGPL-3.0. Upstream source is
tracked as a git subtree under `upstream/`. Nehanda-specific changes live in `src/`
and `patches/`.

## Third-party components

Third-party licenses inherited from aimee and vendored dependencies are listed in
[NOTICE](NOTICE), including cJSON (MIT) and build-time tree-sitter grammars (MIT).

## Questions

For licensing questions about nehanda-cli, contact **support@asoba.co**.
