#!/usr/bin/env bash
# Resolve the commit a focused gate's `git diff --check` starts from, so the
# whitespace check covers exactly the candidate change and never historical
# files it did not touch.
#   1. explicit override (first argument), e.g. P2_CANDIDATE_BASE_SHA;
#   2. GitHub pull_request: merge-base with the PR base branch (the whole PR,
#      not only its last commit; workflows check out with fetch-depth: 0);
#   3. otherwise HEAD^ (local single-commit check).
resolve_candidate_base() {
  local override="${1:-}"
  if [[ -n "$override" ]]; then
    printf '%s\n' "$override"
    return
  fi
  if [[ -n "${GITHUB_BASE_REF:-}" ]] &&
     git rev-parse -q --verify "origin/${GITHUB_BASE_REF}^{commit}" >/dev/null; then
    git merge-base HEAD "origin/${GITHUB_BASE_REF}"
    return
  fi
  printf '%s\n' 'HEAD^'
}
