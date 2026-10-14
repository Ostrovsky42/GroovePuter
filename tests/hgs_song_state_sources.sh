# Shared link dependencies for PatternPagingService's versioned HGS sidecar.
HGS_REPO_ROOT="${ROOT_DIR:-${ROOT}}"
HGS_SONG_STATE_SOURCES=(
  "${HGS_REPO_ROOT}/src/generation/harmony/song_harmonic_state.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/harmonic_source_codec.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/harmonic_fingerprint.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/song_variant_genome.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/genre_harmonic_policy.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/harmonic_relation.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/pitch_set.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/corpus_prior.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/harmonic_candidate.cpp"
  "${HGS_REPO_ROOT}/src/generation/harmony/harmonic_route.cpp"
)
