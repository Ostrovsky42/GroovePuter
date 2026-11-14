import csv, subprocess, sys
E = "/home/gg/pro/gp-0924-artifacts/exp"
REPO = "/home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-0924"
def load(path):
    head = {}; G = {}; T = {}; X = {}
    for line in open(path, encoding="utf-8"):
        c = line.rstrip("\n").split("\t")
        if c[0] in ("T", "G") and len(c) > 1 and c[1] == "genre":
            head[c[0]] = c; continue
        if c[0] in ("T", "G") and c[0] in head and len(c) == len(head[c[0]]):
            row = dict(zip(head[c[0]], c))
            (G if c[0] == "G" else T).setdefault(row["genre"], []).append(row)
        elif c[0].startswith("X.") and len(c) > 2:
            try: float(c[2])
            except ValueError: continue
            X.setdefault(c[0], []).append(c)
    return G, T, X
def house(path):
    G, T, X = load(path)
    g = G["House"][0]
    row = X["X.bass"][9]; lead = X["X.lead"][9]
    return {
        "bass.diag": float(row[2 + 9]), "synthB.diag": float(lead[2 + 9]),
        "B.n/bar": float(g["B.n/bar"]), "B.adj16": float(g["B.adj16"]),
        "chords/bar": float(g["chords"]), "A.att412": float(g["A.att412"]),
        "takes chords": [float(t["chords"]) for t in T["House"]],
        "takes B.adj16": [float(t["B.adj16"]) for t in T["House"]],
    }
files = {"base": f"{E}/feat_base.txt", "cand-house": f"{E}/feat_cand-house.txt",
         "cand-organ-nomel": f"{E}/feat_cand-organ-nomel.txt",
         "cand-organ-p1": f"{E}/feat_cand-organ-p1.txt",
         "cand-organ-nostab15": f"{E}/feat_cand-organ-nostab15.txt"}
for name, path in files.items():
    h = house(path)
    print(f"{name:20s} bass.diag={h['bass.diag']:.2f} synthB.diag={h['synthB.diag']:.2f} "
          f"B.n/bar={h['B.n/bar']:.2f} B.adj16={h['B.adj16']:.3f} chords/bar={h['chords/bar']:.2f} A.att412={h['A.att412']:.2f}")
    print(f"{'':20s} takes chords={h['takes chords']} takes B.adj16={[round(x,3) for x in h['takes B.adj16']]}")
