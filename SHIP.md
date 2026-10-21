# Shipping Nehanda CLI

Process guide for packaging, publishing, and serving `nehanda-cli`[cite: 1] via single-line curl installation.

**1. Package the Release Archive**
Compress required core runtime files, schemas, and npm manifests into a distribution tarball:
* Source paths: `bin/`[cite: 1], `lib/`[cite: 1], `templates/`[cite: 1], `schema.sql`[cite: 1], `package.json`[cite: 1], `package-lock.json`[cite: 1]
* Build command:
  ```bash
  tar -czvf nehanda-cli-latest.tar.gz bin/ lib/ templates/ schema.sql package.json package-lock.json
  ```
  (Or, since #13: `scripts/build-artifacts.sh` rebuilds all artifacts, including `install.sh` alongside + inside the tarball.)
* Copy tar.gz **and** `scripts/install.sh` to the /releases folder at the web root of the asoba.co website
* install.sh in that same location runs the install (`curl -fsSL https://asoba.co/releases/install.sh | sh`)

**Artifacts also rebuilt by the same script:**
* `nehanda-cli.bin` — makeself (label "Nehanda CLI Installer"), Linux self-extractor: payload + `install.sh`, extraction verified via `diff -rq` against the repo before shipping
* `Nehanda-CLI.dmg` — macOS UDZO image: `Install.command` (from prior DMG) + payload + `install.sh`

**Preconditions enforced by `scripts/build-artifacts.sh`** (#13 behavioral spec BG-3): refuses to build on a dirty payload tree (`bin/ lib/ templates/ schema.sql package*.json`) or a red behavioral suite (`NEHANDA_TEST_CMD` override for testing); verifies payload contents match HEAD (incl. #11/#12 fixes) before shipping.
