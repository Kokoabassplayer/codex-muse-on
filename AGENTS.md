## Agent skills

### Issue tracker

Issues and PRDs are tracked in GitHub Issues for `Kokoabassplayer/codex-muse-on`. See `docs/agents/issue-tracker.md`.

### Triage labels

Use the five default canonical triage labels. See `docs/agents/triage-labels.md`.

### Domain docs

This repository uses the single-context domain-doc layout. See `docs/agents/domain.md`.

## Execution economy

- Matt Pocock's lifecycle remains canonical for planning, specification, tickets, implementation routing, and review; run its setup once per repository.
- After a ticket is fully specified, prefer ZCode headless for mechanical code edits and tests when available. Do not install or run Matt lifecycle skills inside ZCode.
- Give ZCode one bounded ticket with acceptance criteria, allowed paths, and test commands. Codex must review the resulting diff and test evidence.
- Run ZCode work on an isolated branch or worktree. Do not push, merge, publish, deploy, or release without explicit user approval.
- Minimize OpenAI usage: avoid repeated polling and commentary, wait for a concise final result, and reserve Sol High for hard planning or final review.
- If ZCode is unavailable or unsuitable, use Codex with the smallest capable model and reasoning effort.
