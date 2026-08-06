# Phrase Arranger Stage 2 CI policy

The experimental feature remains stacked on `integration/genre-song-ui` in PR #90.

A separate draft PR against `dev` is used only to trigger the repository's full
GitHub Actions matrix. It is not a merge candidate and must be closed after the
reviewed branch head receives a successful host, SDL, Cardputer ADV and fixed
DRAM run.
