import sys, collections
rows=[l.rstrip("\n").split("\t") for l in open(r"独立审计/证据/scan402/four_side_tight.tsv",encoding="utf-8")][1:]
d=collections.defaultdict(list)
for k,w,s in rows: d[k].append((w,s))
want=sys.argv[1:] or sorted(d)
for k in want:
    print("### KEY",k, len(d.get(k,[])))
    for w,s in d.get(k,[]):
        print("   ",w,"|",s)
