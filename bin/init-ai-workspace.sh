#!/usr/bin/env bash
set -euo pipefail

program_name="${0##*/}"

usage() {
  cat <<'EOF'
Portable AI workspace initializer and launcher.

Usage:
  init-ai-workspace.sh init [directory]
  workspace begin [default|custom] [--cli auto|codex|claude|gemini|print]
                  [--prompt "workspace requirements"] [--prompt-file path]
  workspace doctor
  workspace help

Examples:
  ./init-ai-workspace.sh init ~/Workspace/my-new-workspace
  cd ~/Workspace/my-new-workspace
  ./workspace begin
  ./workspace begin custom --cli claude --prompt "A workspace for two APIs and shared research"
  ./workspace begin --cli codex "A focused Python library with durable AI handoffs"
  ./workspace begin custom --cli print --prompt-file requirements.md

Modes:
  default  Activate and describe the current environment without redesigning it.
  custom   Tailor and finalize the workspace from a saved requirements prompt.

The initializer never replaces an existing file. If a generated path already
exists, it is preserved and reported.
EOF
}

fail() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

write_if_missing() {
  local destination="$1"
  if [[ -e "$destination" ]]; then
    printf 'kept    %s\n' "$destination"
    return 0
  fi
  mkdir -p "$(dirname "$destination")"
  cat > "$destination"
  printf 'created %s\n' "$destination"
}

absolute_directory() {
  local directory="$1"
  (cd "$directory" && pwd -P)
}

find_workspace_root() {
  local current
  current="$(pwd -P)"
  while :; do
    if [[ -f "$current/.workspace/MANIFEST.md" ]]; then
      printf '%s\n' "$current"
      return 0
    fi
    [[ "$current" = "/" ]] && break
    current="$(dirname "$current")"
  done

  local script_directory
  script_directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
  if [[ -f "$script_directory/.workspace/MANIFEST.md" ]]; then
    printf '%s\n' "$script_directory"
    return 0
  fi

  return 1
}

initialize_workspace() {
  local requested_directory="${1:-.}"
  mkdir -p "$requested_directory"

  local target
  target="$(absolute_directory "$requested_directory")"
  local workspace_name
  workspace_name="$(basename "$target")"
  local created_date
  created_date="$(date +%F)"
  local source_script
  source_script="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)/$(basename "${BASH_SOURCE[0]}")"
  local launcher="$target/workspace"

  if [[ -e "$launcher" ]] && ! cmp -s "$source_script" "$launcher"; then
    fail "launcher already exists and differs: $launcher"
  fi

  if [[ -n "$(find "$target" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
    printf 'note: initializing inside an existing directory; existing files will be preserved.\n'
  fi

  mkdir -p \
    "$target/.workspace/requests" \
    "$target/.workspace/sessions" \
    "$target/_shared/skills/workspace-coach"

  write_if_missing "$target/.workspace/MANIFEST.md" <<EOF
# Portable AI Workspace

- Schema: 1
- Name: $workspace_name
- Initialized: $created_date
- Finalization: pending

This file identifies the nearest workspace root. The active profile and the
finalization record are stored beside it.
EOF

  write_if_missing "$target/.workspace/AGENT-CONTRACT.md" <<'EOF'
# Workspace Agent Contract

This directory is an organizer and control plane for independent targets, not
one application or one implicit Git repository.

## Activation

Before substantive work:

1. Select the smallest owning target for the request.
2. Keep one primary write target. Treat other folders as read-only unless the
   user explicitly requests a coordinated change.
3. Read applicable `AGENTS.md` files down to that target.
4. Load compact state and indexes before detailed reports, sessions, raw data,
   generated files, or inputs.
5. Inspect the owning repository's Git status when one exists.
6. Run commands and tests from the owning target, and use `git -C <repo>`.

## Routing

- Application implementation: `projects/<name>/`
- Reusable operational workflow or verification: `agents/<name>/`
- Current external investigation: `research/topics/<topic>/`
- Verified reusable conclusions: `knowledge/`
- Reusable prompts, templates, and skills: `_shared/`
- Local infrastructure: `tools/`
- Disposable cross-context output: `temp-sessions/<name>/`
- Isolated parallel Git work: `worktrees/`

Create only the locations the workspace actually needs. A single-repository
workspace may use its current directory as the owning target instead of adding
an unnecessary hierarchy.

## Safety and completion

- Preserve existing files and unrelated changes.
- Never put credentials in tracked files, prompts, or state.
- Do not commit, push, rewrite history, discard changes, or manage worktrees
  without explicit authorization.
- Never claim an unrun check passed.
- Keep current status, active tasks, context pointers, and durable decisions in
  compact files. Put verbose evidence in dated session or report files.
- At completion, report outcomes, changed files, checks actually run, Git
  state, remaining blockers, and the exact next action.
EOF

  write_if_missing "$target/_shared/skills/workspace-coach/SKILL.md" <<'EOF'
---
name: workspace-coach
description: Activates, tailors, and operates a portable multi-CLI AI workspace with strict target scope and durable file-based memory.
---

# Portable Workspace Coach

Read `.workspace/AGENT-CONTRACT.md` before using this skill.

## Default mode

Use the environment as it already exists.

1. Inspect top-level paths and instruction files without scanning all content.
2. Detect whether the current directory is a control plane or one focused
   repository.
3. Preserve its structure. Do not create folders merely for symmetry.
4. Record the detected ownership model in `.workspace/PROFILE.md`.
5. Write `.workspace/FINALIZED.md` only when the description is accurate and
   the workspace passes `./workspace doctor`.
6. If no work request exists yet, report readiness and ask for the task.

## Custom mode

Tailor the environment from the request file named in the launch prompt.

1. Read the request, this skill, the workspace contract, and only relevant
   existing instruction or state files.
2. Separate durable workspace needs from the user's first application task.
3. Choose the smallest useful layout and one primary write scope.
4. Preserve existing files. Add or revise only the files needed to support the
   requested workflow across AI CLIs.
5. Keep vendor-neutral rules in `.workspace/AGENT-CONTRACT.md`; keep
   `AGENTS.md`, `CLAUDE.md`, and `GEMINI.md` as thin entry points.
6. For each independent target, create compact state, tasks, index, decisions,
   and nested instructions only when they add real value.
7. Document assumptions and rejected complexity in `.workspace/DECISIONS.md`.
8. Update `.workspace/PROFILE.md`, run `./workspace doctor`, and then write
   `.workspace/FINALIZED.md` with the final layout, ownership boundaries,
   startup command, verification, and exact next action.

Do not implement the user's application or research task during workspace
finalization unless the request explicitly asks for both operations.

## Operating rule

Chats and AI CLIs are replaceable workers. Files are persistent memory, and
Git records reviewed checkpoints. Switching CLIs must not change ownership,
state, or safety rules.
EOF

  write_if_missing "$target/.workspace/PROFILE.md" <<EOF
# Workspace Profile

- Name: $workspace_name
- Status: unfinalized
- Mode: not selected
- Primary ownership model: not selected

## Purpose

To be completed by Workspace Coach in default or custom mode.

## Active targets

- None selected.

## Startup

Run \`./workspace begin\` for default activation, or pass a requirements prompt
to use custom finalization.
EOF

  write_if_missing "$target/.workspace/STATE.md" <<'EOF'
# Workspace State

- Status: initialized; finalization pending
- Blocker: none
- Exact next action: run `./workspace begin` or `./workspace begin custom --prompt "..."`
EOF

  write_if_missing "$target/.workspace/TASKS.md" <<'EOF'
# Workspace Tasks

## Active

- [ ] Finalize the workspace in default or custom mode.

## Next

- [ ] Start the first scoped task after finalization.
EOF

  write_if_missing "$target/.workspace/INDEX.md" <<'EOF'
# Workspace Context Index

- Agent contract: `.workspace/AGENT-CONTRACT.md`
- Portable Workspace Coach: `_shared/skills/workspace-coach/SKILL.md`
- Active profile: `.workspace/PROFILE.md`
- Current state: `.workspace/STATE.md`
- Active tasks: `.workspace/TASKS.md`
- Durable decisions: `.workspace/DECISIONS.md`
- Final layout and startup: `.workspace/FINALIZED.md` after finalization
- Saved custom requests: `.workspace/requests/`
- Detailed handoffs: `.workspace/sessions/`
EOF

  write_if_missing "$target/.workspace/DECISIONS.md" <<'EOF'
# Workspace Decisions

Record only durable architecture and workflow decisions here. Keep temporary
reasoning and terminal output out of this file.
EOF

  write_if_missing "$target/.workspace/requests/.gitkeep" <<'EOF'
EOF
  write_if_missing "$target/.workspace/sessions/.gitkeep" <<'EOF'
EOF

  write_if_missing "$target/AGENTS.md" <<'EOF'
# Workspace Entry Point

Before acting, read and follow:

1. `.workspace/AGENT-CONTRACT.md`
2. `_shared/skills/workspace-coach/SKILL.md` when activating, routing, or
   finalizing the workspace
3. The applicable nested `AGENTS.md` and compact state files for the selected
   target

Do not treat this workspace root as one application or one Git repository.
EOF

  write_if_missing "$target/CLAUDE.md" <<'EOF'
@AGENTS.md
EOF

  write_if_missing "$target/GEMINI.md" <<'EOF'
# Gemini Workspace Entry Point

Read and follow `AGENTS.md`, `.workspace/AGENT-CONTRACT.md`, and the applicable
nested instructions before acting.
EOF

  write_if_missing "$target/WORKSPACE.md" <<'EOF'
# Portable AI Workspace

This folder keeps durable context independent of the AI CLI that works on it.

## Begin

```bash
./workspace begin
./workspace begin default --cli codex
./workspace begin custom --cli claude --prompt "Describe the workspace you need"
./workspace begin custom --cli gemini --prompt-file requirements.md
```

No prompt selects default mode. Supplying a prompt selects custom mode. Use
`--cli print` to print the prepared launch prompt for any unsupported AI CLI.

## Verify

```bash
./workspace doctor
```

The initializer preserves every pre-existing file. If this scaffold was added
to an existing repository, review the generated entry points before tracking
them in Git.
EOF

  if [[ -e "$launcher" ]]; then
    if cmp -s "$source_script" "$launcher"; then
      printf 'kept    %s\n' "$launcher"
    else
      fail "launcher already exists and differs: $launcher"
    fi
  else
    cp "$source_script" "$launcher"
    chmod +x "$launcher"
    printf 'created %s\n' "$launcher"
  fi

  printf '\nInitialized workspace: %s\n' "$target"
  printf 'Next:\n'
  printf '  cd %q\n' "$target"
  printf '  ./workspace begin\n'
  printf 'Or customize it:\n'
  printf '  ./workspace begin custom --prompt %q\n' 'Describe the environment you need'
}

doctor_workspace() {
  local root
  root="$(find_workspace_root)" || fail "no initialized workspace found from the current directory"
  local missing=0
  local relative
  local required=(
    ".workspace/MANIFEST.md"
    ".workspace/AGENT-CONTRACT.md"
    ".workspace/PROFILE.md"
    ".workspace/STATE.md"
    ".workspace/TASKS.md"
    ".workspace/INDEX.md"
    ".workspace/DECISIONS.md"
    "_shared/skills/workspace-coach/SKILL.md"
    "workspace"
  )

  printf 'Workspace: %s\n' "$root"
  for relative in "${required[@]}"; do
    if [[ -e "$root/$relative" ]]; then
      printf 'ok      %s\n' "$relative"
    else
      printf 'missing %s\n' "$relative"
      missing=1
    fi
  done

  printf '\nCLI availability:\n'
  local cli
  for cli in codex claude gemini; do
    if command -v "$cli" >/dev/null 2>&1; then
      printf 'ok      %-7s %s\n' "$cli" "$(command -v "$cli")"
    else
      printf 'optional missing %s\n' "$cli"
    fi
  done

  printf '\nFinalization:\n'
  if [[ -f "$root/.workspace/FINALIZED.md" ]]; then
    printf 'final   .workspace/FINALIZED.md\n'
  else
    printf 'pending run ./workspace begin or custom mode\n'
  fi

  [[ "$missing" -eq 0 ]]
}

select_cli() {
  local requested="$1"
  if [[ "$requested" != "auto" ]]; then
    printf '%s\n' "$requested"
    return 0
  fi

  local candidate
  for candidate in codex claude gemini; do
    if command -v "$candidate" >/dev/null 2>&1; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  printf 'print\n'
}

launch_cli() {
  local cli="$1"
  local root="$2"
  local prompt="$3"

  case "$cli" in
    print)
      printf '%s\n' "$prompt"
      ;;
    codex)
      command -v codex >/dev/null 2>&1 || fail "Codex CLI is not installed; use --cli print"
      exec codex -C "$root" "$prompt"
      ;;
    claude)
      command -v claude >/dev/null 2>&1 || fail "Claude CLI is not installed; use --cli print"
      cd "$root"
      exec claude "$prompt"
      ;;
    gemini)
      command -v gemini >/dev/null 2>&1 || fail "Gemini CLI is not installed; use --cli print"
      cd "$root"
      exec gemini -i "$prompt"
      ;;
    *)
      fail "unsupported CLI '$cli'; choose auto, codex, claude, gemini, or print"
      ;;
  esac
}

begin_workspace() {
  local root
  root="$(find_workspace_root)" || fail "no initialized workspace found; run '$program_name init <directory>' first"

  local mode="default"
  local mode_explicit=0
  local cli="${AI_WORKSPACE_CLI:-auto}"
  local prompt=""
  local prompt_file=""
  local positional_prompt=()

  if [[ $# -gt 0 && ( "$1" = "default" || "$1" = "custom" ) ]]; then
    mode="$1"
    mode_explicit=1
    shift
  fi

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --cli)
        [[ $# -ge 2 ]] || fail "--cli requires a value"
        cli="$2"
        shift 2
        ;;
      --prompt)
        [[ $# -ge 2 ]] || fail "--prompt requires text"
        prompt="$2"
        shift 2
        ;;
      --prompt-file)
        [[ $# -ge 2 ]] || fail "--prompt-file requires a path"
        prompt_file="$2"
        shift 2
        ;;
      --help|-h)
        usage
        return 0
        ;;
      --)
        shift
        while [[ $# -gt 0 ]]; do
          positional_prompt+=("$1")
          shift
        done
        ;;
      -* )
        fail "unknown begin option: $1"
        ;;
      *)
        positional_prompt+=("$1")
        shift
        ;;
    esac
  done

  if [[ -n "$prompt_file" ]]; then
    [[ -z "$prompt" ]] || fail "use only one of --prompt and --prompt-file"
    [[ -f "$prompt_file" ]] || fail "prompt file not found: $prompt_file"
    prompt="$(<"$prompt_file")"
  fi

  if [[ ${#positional_prompt[@]} -gt 0 ]]; then
    [[ -z "$prompt" ]] || fail "do not combine positional prompt text with --prompt or --prompt-file"
    prompt="${positional_prompt[*]}"
  fi

  if [[ -n "$prompt" ]]; then
    if [[ "$mode_explicit" -eq 1 && "$mode" = "default" ]]; then
      fail "default mode does not accept requirements; use custom mode"
    fi
    mode="custom"
  elif [[ "$mode" = "custom" ]]; then
    fail "custom mode requires --prompt, --prompt-file, or positional prompt text"
  fi

  local launch_prompt
  if [[ "$mode" = "default" ]]; then
    IFS= read -r -d '' launch_prompt <<'EOF' || true
Use the local Workspace Coach skill in DEFAULT mode.

1. Read AGENTS.md, .workspace/AGENT-CONTRACT.md, and
   _shared/skills/workspace-coach/SKILL.md completely.
2. Inspect only the current workspace's top-level paths and relevant compact
   context. Do not scan every project, report, session, raw file, or input.
3. Detect whether this is a control plane or one focused repository and use
   the existing structure without redesigning it.
4. Update .workspace/PROFILE.md and compact workspace state so another AI CLI
   can resume consistently.
5. Run ./workspace doctor. If the profile is accurate, create or update
   .workspace/FINALIZED.md with the detected layout, ownership boundaries,
   verification, startup command, and exact next action.
6. Report the active environment. If I have not supplied a work task, remain
   interactive and ask what scoped task I want to begin.

Preserve existing files, do not expose secrets, and do not perform Git writes.
EOF
  else
    local request_id
    request_id="$(date +%Y%m%d-%H%M%S)-$$"
    local request_file="$root/.workspace/requests/$request_id.md"
    {
      printf '# Workspace Finalization Request\n\n'
      printf -- '- Created: %s\n' "$(date '+%Y-%m-%d %H:%M:%S %z')"
      printf -- '- Requested mode: custom\n\n'
      printf '## Requirements\n\n%s\n' "$prompt"
    } > "$request_file"
    chmod 600 "$request_file"
    local request_relative="${request_file#"$root/"}"

    IFS= read -r -d '' launch_prompt <<EOF || true
Use the local Workspace Coach skill in CUSTOM mode to tailor and finalize this
workspace. The user's requirements are saved in:

  $request_relative

Read AGENTS.md, .workspace/AGENT-CONTRACT.md,
_shared/skills/workspace-coach/SKILL.md, and that request completely. Inspect
only relevant existing paths and compact state. Then:

1. Separate workspace architecture needs from the first product or research
   task.
2. Choose the smallest useful target layout and one primary write scope.
3. Preserve existing files and unrelated work. Do not create folders merely
   to mirror the example routing categories.
4. Make the architecture CLI-neutral; keep CLI entry files thin.
5. Document durable decisions and assumptions, update the profile and compact
   state, and add nested target contracts only where useful.
6. Run ./workspace doctor.
7. Only after verification, create or update .workspace/FINALIZED.md with the
   final layout, ownership boundaries, startup command, verification actually
   run, and exact next action.

This pass finalizes the environment. Do not start implementing the user's
application or research task unless the saved request explicitly asks you to
do both. Do not expose secrets, perform Git writes, or modify paths outside
this workspace.
EOF
    printf 'Saved custom request: %s\n' "$request_relative"
  fi

  local selected_cli
  selected_cli="$(select_cli "$cli")"
  printf 'Mode: %s\nCLI:  %s\n\n' "$mode" "$selected_cli" >&2
  launch_cli "$selected_cli" "$root" "$launch_prompt"
}

main() {
  local command="${1:-help}"
  [[ $# -eq 0 ]] || shift

  case "$command" in
    init)
      [[ $# -le 1 ]] || fail "init accepts at most one directory"
      initialize_workspace "${1:-.}"
      ;;
    begin)
      begin_workspace "$@"
      ;;
    doctor|status)
      [[ $# -eq 0 ]] || fail "$command does not accept arguments"
      doctor_workspace
      ;;
    help|--help|-h)
      usage
      ;;
    *)
      fail "unknown command '$command'; run '$program_name help'"
      ;;
  esac
}

main "$@"
