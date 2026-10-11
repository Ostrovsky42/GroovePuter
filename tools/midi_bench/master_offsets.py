"""GroovePuter as master (INTERNAL) from a listen log: its own clock tempo and
jitter, and every NoteOn vs its own sixteenth grid counted from the last FA.

Usage: master_offsets.py LOG
Needs a real FA in the capture (see listen.c pitfall).
"""
import sys, statistics as st
ev=[l.rstrip("\n").split("\t") for l in open(sys.argv[1])]
clk=[int(t) for t,m in ev if m=="RT F8"]; starts=[int(t) for t,m in ev if m=="RT FA"]; stops=[int(t) for t,m in ev if m=="RT FC"]
print(f"F8 {len(clk)}  FA at {[round(s/1e9,3) for s in starts]}  FC at {[round(s/1e9,3) for s in stops]}")
if len(clk)<50: sys.exit("too few clocks")
iv=[(clk[i+1]-clk[i])/1e6 for i in range(len(clk)-1)]
span=(clk[-1]-clk[0])/1e9; bpm=(len(clk)-1)/span*60/24
print(f"GP clock: {bpm:.3f} BPM over {span:.1f}s; interval mean {st.mean(iv):.3f} ms sd {st.pstdev(iv):.2f} max {max(iv):.2f} min {min(iv):.2f}; >1.5x gaps {sum(i>1.5*st.median(iv) for i in iv)}")
# notes vs GP's own clock grid: index pulses from the first F8 after the last FA
s=starts[-1] if starts else clk[0]
grid=[c for c in clk if c>=s]
on=[(int(t),m) for t,m in ev if m[0]=="9" and not m.endswith(" 0") and int(t)>=grid[0]-50_000_000]
import bisect
offs=[]
for t,m in on:
    i=bisect.bisect_left(grid,t)
    # nearest 16th = every 6th pulse counted from grid[0]
    cands=[k for k in (i-4,i-3,i-2,i-1,i,i+1,i+2,i+3) if 0<=k<len(grid) and k%6==0]
    if not cands: continue
    k=min(cands,key=lambda k:abs(grid[k]-t)); offs.append((t,(t-grid[k])/1e6))
o=[x for _,x in offs]
print(f"NoteOn {len(o)} vs GP's own 16th clock: mean {st.mean(o):+.1f} ms sd {st.pstdev(o):.1f} min {min(o):+.1f} max {max(o):+.1f}")
c=max(1,len(o)//8); print("  drift per chunk:"," ".join(f"{st.mean(o[i:i+c]):+.1f}" for i in range(0,len(o),c)))
