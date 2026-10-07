"""Per-bar view of a clockmaster log: offset of the note nearest each bar start
(ms, positive = late). "?" = no note within half a step of the bar start.

Usage: bar_offsets.py LOG BPM
"""
import sys
bpm=float(sys.argv[2]); step=60e9/bpm/4; bar=16*step
notes=[int(l.split("\t")[1]) for l in open(sys.argv[1]) if l.startswith("RX") and l.split("\t")[2][0]=="9" and not l.rstrip().endswith(" 0")]
out=[]
for b in range(int(notes[-1]//bar)+1):
    c=min(notes,key=lambda t:abs(t-b*bar))
    out.append(f"{(c-b*bar)/1e6:+.0f}" if abs(c-b*bar)<step/2 else "?")
print("bar-start offset ms:", " ".join(out))
