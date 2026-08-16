# 0.9.10 R5 — MIDI input lifecycle hardening

R5 makes controller ownership cleanup explicit across physical session changes.

The fixed active-owner table remains keyed by transport/session/input-channel/source-note.
A new bounded `releaseSession(transport, session)` operation releases only notes owned
by that physical lifetime. USB disconnect/reconnect uses that scoped operation instead
of a global input panic, so a later UART/BLE/bridge adapter cannot be silenced by an
unrelated USB lifecycle edge.

Configuration changes remain cleanup-first, queue overflow remains discard + panic,
and stale NoteOff from a retired session is counted as orphaned rather than affecting
a newer owner.

No heap-backed note map, polling history, or persisted connection state is introduced.
