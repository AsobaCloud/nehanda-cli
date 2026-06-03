# What's New

Each section covers one release. The CLI reads the section matching the
current version and prints it once after an upgrade.

---

## v0.2.0

- **Self-update notifier**: aimee now tells you what changed after each upgrade
  and warns when the binary is older than the source tree.
- **Stale binary detection**: a one-line warning appears when the binary predates
  the latest commit. Run `make` to rebuild.
- **Lean build profile** (`make lean`): a size-optimised, stripped binary pair for
  minimal deployments, capped at 1.5 MB.
- **Autonomous mode**: drive work without per-step prompts via `aimee auto` (roadmap dispatch loop) and `aimee autopilot` (end-to-end pipeline).
- **Slash command registry**: extensible `/command` system in aimee chat.
- **Event notifications**: fired at delegation and verification boundaries.
