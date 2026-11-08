"""Follow accuracy from a clockmaster log: each NoteOn GroovePuter sent vs the
nearest sixteenth of the exact master grid (pulse 0 = downbeat after FA).

Usage: follow_offsets.py LOG BPM
Positive = GroovePuter late. Drift chunks show the mean per time slice.
"""
import sys, statistics as st
log, bpm = sys.argv[1], float(sys.argv[2])
period_ns = 60e9/(bpm*24); step_ns = 6*period_ns
tx = {}; notes = []
for line in open(log):
    d, t, m = line.rstrip("\n").split("\t"); t = int(t)
    if d == "TX" and m.startswith("F8 "): tx[int(m.split()[1])] = t
    if d == "RX" and len(m.split()) == 3:
        s, n, v = m.split(); s = int(s, 16)
        if s & 0xF0 == 0x90 and int(v) > 0: notes.append((t, s & 0x0F, int(n)))
if not notes: sys.exit("no NoteOn received")
# GP's step 0 is at pulse 0 (Start then first F8). Offset of each note vs nearest 16th of the sent grid.
sendjit = [tx[k] - k*period_ns for k in tx]
print(f"BPM {bpm}: pulses sent {len(tx)}, send jitter max {max(map(abs,sendjit))/1e6:.2f} ms; 16th = {step_ns/1e6:.1f} ms")
rows = []
for t, ch, n in notes:
    k = round(t / step_ns); off = (t - k*step_ns)/1e6
    rows.append((t/1e9, ch, n, k, off))
offs = [r[4] for r in rows]
print(f"NoteOn {len(rows)}: offset vs nearest 16th  mean {st.mean(offs):+.1f} ms  median {st.median(offs):+.1f}  sd {st.pstdev(offs):.1f}  min {min(offs):+.1f}  max {max(offs):+.1f}")
half = step_ns/2e6
print(f"  as fraction of 16th: mean {st.mean(offs)/(step_ns/1e6):+.2f}; notes beyond ±0.4 step: {sum(abs(o)>0.4*step_ns/1e6 for o in offs)}")
# drift: mean offset per 10% chunk of time
n = len(rows); c = max(1, n//8)
print("  drift (mean offset per time chunk):", " ".join(f"{st.mean(o[4] for o in rows[i:i+c]):+.1f}" for i in range(0, n, c)))
by = {}
for r in rows: by.setdefault(r[1], []).append(r[4])
for ch, o in sorted(by.items()): print(f"  ch{ch+1:2d}: n={len(o):3d} mean {st.mean(o):+.1f} sd {st.pstdev(o):.1f}")
