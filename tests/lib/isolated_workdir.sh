# Shared by host gate scripts. Programs under test write project pages, scene-name files and
# similar state into their CURRENT directory. They must never run inside the repository tree:
# `isolated_run` runs them in a private mktemp directory, and the EXIT trap removes only that
# directory. Nothing here deletes any repository path.
isolated_init() {
  GP_WORK="$(mktemp -d "${TMPDIR:-/tmp}/gp-test-work.XXXXXX")"
  trap 'rm -rf "$GP_WORK"' EXIT
}
isolated_run() {
  (cd "$GP_WORK" && "$@")
}
