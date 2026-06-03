---
name: writing-skills
description: Use when creating or editing an aimee skill.
source: bundled
---

# Writing Skills

A skill is process code. Write it to change future agent behavior, not to
record a past fix.

## Iron Law

Every new skill or content edit needs a pressure test: baseline behavior fails
without the skill, then treatment behavior complies with the skill. If the text
does not move behavior, tighten the text before promoting it.

Violating the letter is violating the spirit. A vague skill that gets ignored is
worse than no skill because it still spends context.

## Description Rule

The frontmatter `description` says when to use the skill, not what the body
contains. Start with `Use when` or `Use before`.

Bad: `description: Explains the workflow for verifying work.`

Good: `description: Use before claiming work is done, fixed, passing, complete, or ready to merge.`

If the description summarizes the steps, the agent may take it as a shortcut and
skip the body.

## Structure

For discipline skills, prefer this shape:

1. Iron Law: the rule that must survive pressure.
2. Reality check: why violating the letter violates the spirit.
3. Rationalization table: common excuse and the actual requirement.
4. Red Flags: phrases or situations that mean stop and apply the skill.
5. Gate Function: the concrete check before proceeding.

## Rationalization Table

| Excuse | Reality |
| --- | --- |
| "This is obvious." | Obvious rules still need executable steps and a gate. |
| "The description is enough." | The body is where behavior changes. |
| "No eval is needed for wording." | Skill wording is agent-facing control logic. |

## Red Flags

STOP when the skill draft:

- Retells one incident instead of naming a reusable class of work.
- Uses forced file-load links instead of progressive disclosure.
- Hides the trigger in the body instead of the description.
- Says "consider", "try", or "be mindful" without a gate.
- Has no before/after compliance fixture.

## Gate Function

Before creating or promoting a skill:

1. Confirm the name is lowercase, stable, and class-level.
2. Confirm the description is a trigger.
3. Keep the body concise; move deep references to `references/`.
4. Run `aimee skill lint <name>` or lint the proposed content.
5. Run `aimee skill eval <name>` when eval fixtures exist.
6. Promote only when lint passes and eval evidence shows improvement, or record
   the missing evidence explicitly for human review.

## Layout

Keep principles and code patterns inline when they fit. Put heavy background in
`references/`, reusable text in `templates/`, scripts in `scripts/`, and assets
in `assets/`. Cross-reference other skills by skill name, not path.

This skill adapts authoring discipline from the MIT-licensed
`obra/superpowers` project for aimee's local skill engine.
