---
name: search-project-knowledge
description: Use when you need a project fact, document, relationship, investigation note, or prior finding that may already be captured.
source: bundled
triggers:
  user_pattern: ["known issue", "prior investigation", "what does the docs say", "relationship between"]
---

# Search Project Knowledge

Use Aimee's knowledge and note surfaces before re-deriving project facts from scratch.

Preferred route:

1. Search the most specific surface first: `search_docs` for documented facts, `search_graph` for relationships, `search_notes` for investigations.
2. Use `aimee memory search <terms>` when the fact may be operator-specific or cross-session.
3. Follow cited files or records to verify high-impact claims before changing code.
4. Fall back to direct file reads when the knowledge result is missing, stale, or too vague.

This skill is for project knowledge, not arbitrary web research. Keep the query anchored to the current repo, service, proposal, or incident.
