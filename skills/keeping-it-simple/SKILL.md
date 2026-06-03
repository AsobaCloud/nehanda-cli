---
name: keeping-it-simple
description: Use when tempted to add broad abstractions, future-proofing, or extra product surface beyond the requested change.
source: bundled
---

# Keeping It Simple

Choose the smallest design that handles the current requirement cleanly.

Before adding an abstraction, ask:

- Does it remove real duplication now?
- Does it match an existing pattern in this codebase?
- Will a future maintainer understand why it exists from the local context?

Delete unused knobs, speculative layers, and generic frameworks that are not needed for the requested behavior. Simple does not mean sloppy: keep names clear, errors explicit, and tests focused.
