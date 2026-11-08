# U2A — Theme-independent observability evidence

RED head: `b1b2830d3b1e156dde973e10a2007a9d904783b8`.

RED run: `34053004958` — expected failure: `drawTabIndicator()` depended on `UI::currentStyle`, so NOTES active-state observability could disappear in some themes.

GREEN production change: remove only the theme-dependent early return in `SynthSequencerPage::drawTabIndicator()`.

Preserved:
- `[N]KM / N[K]M / NK[M]` representation;
- Synth A/B entity colors;
- coordinates and tab-strip geometry;
- Pattern and Phrase NOTES paths;
- Tab interaction behaviour;
- no musical, source, transport, persistence or layout semantics changed.

Verification pending on final exact HEAD at time this evidence file was introduced.
