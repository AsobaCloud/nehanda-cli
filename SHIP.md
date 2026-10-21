# Shipping Nehanda CLI

Process guide for packaging, publishing, and serving `nehanda-cli`[cite: 1] via single-line curl installation.

**1. Package the Release Archive**
Compress required core runtime files, schemas, and npm manifests into a distribution tarball:
* Source paths: `bin/`[cite: 1], `lib/`[cite: 1], `templates/`[cite: 1], `schema.sql`[cite: 1], `package.json`[cite: 1], `package-lock.json`[cite: 1]
* Build command:
  ```bash
  tar -czvf nehanda-cli-latest.tar.gz bin/ lib/ templates/ schema.sql package.json package-lock.json
* Copy tar.gz to the /releases folder at the web root of the asoba.co website
* install.sh in that same location runs the install
