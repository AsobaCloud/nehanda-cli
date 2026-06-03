---
name: recall-before-asking
description: Use before asking the user about infrastructure, preferences, conventions, prior decisions, or past incidents.
source: bundled
triggers:
  user_pattern: ["do you remember", "what did we decide", "which host", "what is my preference"]
---

# Recall Before Asking

Search Aimee memory before spending a turn asking the user for information that may already be known.

Preferred route:

1. Use `search_memory` / `memory_recall`, or run `aimee memory search <terms>`, with the concrete host, repo, service, person, convention, or incident name.
2. Treat memory as evidence with confidence, not as authority over current user instructions.
3. If memory answers the question, proceed and cite the remembered fact in your reasoning.
4. Ask the user only when memory is absent, ambiguous, low-confidence, or conflicts with the current request.

This is especially important for homelab endpoints, repo conventions, operator preferences, and recurring operational fixes.
