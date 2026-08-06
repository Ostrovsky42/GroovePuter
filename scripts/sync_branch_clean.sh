#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
  cat <<'USAGE'
Usage:
  sync_branch_clean.sh [--discard] <branch>

Examples:
  sync_branch_clean.sh dev
  sync_branch_clean.sh agent/genre-feel-generation-texture-analysis
  sync_branch_clean.sh --discard feature/my-branch

Options:
  --discard  Discard tracked local changes before switching.
             Other untracked files are not removed.
  -h, --help Show this help.

Repository lookup order:
  1. GROOVEPUTER_REPO environment variable;
  2. parent of this script when it is stored in <repo>/scripts/;
  3. current Git working tree.
USAGE
}

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

branch=''
discard=0

while (($# > 0)); do
  case "$1" in
    --discard)
      discard=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      (($# > 0)) || die 'Branch name is missing after --.'
      [[ -z "$branch" ]] || die 'Only one branch name may be supplied.'
      branch="$1"
      ;;
    -*)
      die "Unknown option: $1"
      ;;
    *)
      [[ -z "$branch" ]] || die 'Only one branch name may be supplied.'
      branch="$1"
      ;;
  esac
  shift
done

[[ -n "$branch" ]] || {
  usage >&2
  exit 2
}

command -v git >/dev/null 2>&1 || die 'git is not installed or not in PATH.'
git check-ref-format --branch "$branch" >/dev/null 2>&1 ||
  die "Invalid branch name: $branch"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
repo_candidate=''

if [[ -n "${GROOVEPUTER_REPO:-}" ]]; then
  repo_candidate="$GROOVEPUTER_REPO"
elif git -C "$script_dir/.." rev-parse --show-toplevel >/dev/null 2>&1; then
  repo_candidate="$script_dir/.."
else
  repo_candidate="$PWD"
fi

repo_root="$(git -C "$repo_candidate" rev-parse --show-toplevel 2>/dev/null)" ||
  die "No Git repository found from: $repo_candidate"
cd "$repo_root"

git remote get-url origin >/dev/null 2>&1 ||
  die "Repository has no 'origin' remote: $repo_root"

printf 'Repository : %s\n' "$repo_root"
printf 'Branch     : %s\n' "$branch"
printf 'Fetching   : origin (prune + tags)\n'
git fetch origin --prune --tags

remote_ref="refs/remotes/origin/$branch"
git show-ref --verify --quiet "$remote_ref" ||
  die "Remote branch does not exist: origin/$branch"

if ! git diff --quiet || ! git diff --cached --quiet; then
  if ((discard == 0)); then
    git status --short
    die 'Tracked local changes detected. Commit/stash them or repeat with --discard.'
  fi
  printf 'Discarding : tracked local changes\n'
  git reset --hard
fi

if git show-ref --verify --quiet "refs/heads/$branch"; then
  git switch "$branch"
else
  git switch --create "$branch" --track "origin/$branch"
fi

git reset --hard "origin/$branch"

artifact_dirs=(
  build
  build_output
  dist
  release_bins
  .pio
  .pioenvs
  .piolibdeps
  platform_sdl/build
  platform_sdl/.deps
)

printf 'Cleaning   : known build directories\n'
for path in "${artifact_dirs[@]}"; do
  if [[ -e "$path" || -L "$path" ]]; then
    rm -rf -- "$path"
    printf '  removed  : %s\n' "$path"
  fi
done

artifact_files=(
  platform_sdl/miniacid
  platform_sdl/grooveputer
)

for path in "${artifact_files[@]}"; do
  if [[ -e "$path" || -L "$path" ]]; then
    rm -f -- "$path"
    printf '  removed  : %s\n' "$path"
  fi
done

remove_ignored_compiler_outputs() {
  local search_root="$1"
  local max_depth="$2"
  local path relative

  [[ -d "$search_root" ]] || return 0

  while IFS= read -r -d '' path; do
    relative="${path#./}"
    if git check-ignore -q -- "$relative"; then
      rm -f -- "$relative"
      printf '  removed  : %s\n' "$relative"
    fi
  done < <(
    find "$search_root" -maxdepth "$max_depth" -type f \
      \( -name '*.o' -o -name '*.a' -o -name '*.d' -o \
         -name '*.elf' -o -name '*.bin' -o -name '*.map' \) \
      -print0
  )
}

# Only scan locations where this repository emits standalone compiler outputs.
# Nested project data and arbitrary untracked files are left untouched.
remove_ignored_compiler_outputs . 1
remove_ignored_compiler_outputs platform_sdl 2

if [[ -s .gitmodules ]]; then
  printf 'Submodules : sync and update\n'
  git submodule sync --recursive
  git submodule update --init --recursive
else
  printf 'Submodules : none, skipped\n'
fi

local_sha="$(git rev-parse HEAD)"
remote_sha="$(git rev-parse "origin/$branch")"
[[ "$local_sha" == "$remote_sha" ]] ||
  die 'Local HEAD does not match the fetched remote branch.'

printf '\nReady.\n'
printf 'HEAD       : %s\n' "$local_sha"
printf 'Short SHA  : %s\n' "$(git rev-parse --short=12 HEAD)"
printf 'Subject    : %s\n' "$(git log -1 --pretty=%s)"
git status --short --branch
