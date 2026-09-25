import json
import os
import subprocess
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
OUT = r"独立审计/证据/scan402\aud402_key_consumers.tsv"

d = json.load(open(os.path.join(REPO, "eng/packaging/config/defaults.json"), encoding="utf-8-sig"))
keys = [f["key"] for f in d["fields"]]
leafs = [k.split(".")[-1] for k in keys]


def grep_count(term, paths):
    r = subprocess.run(["git", "-C", REPO, "grep", "-n", "-w", "-e", term, "--"] + paths,
                       capture_output=True)
    lines = r.stdout.decode("utf-8", "replace").splitlines()
    return [l for l in lines if l]


with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    fh.write("key\tleaf\tlib_hits\tlib_prod_hits\tsample_prod\n")
    for k, leaf in zip(keys, leafs):
        allh = grep_count(leaf, ["lib"])
        prod = [h for h in allh if not any(x in h for x in
                ("/tests/", "/test/", "/tools/", "README", "memory.md", ".md:", "test_",
                 "_test", "oracle", "/integration/", "tests_core"))]
        fh.write(k + "\t" + leaf + "\t" + str(len(allh)) + "\t" + str(len(prod)) + "\t"
                 + (prod[0][:180] if prod else "") + "\n")
print("wrote", OUT)
for line in open(OUT, encoding="utf-8").read().splitlines()[1:]:
    p = line.split("\t")
    if int(p[3]) == 0:
        print("ZERO-PROD-CONSUMER:", p[0])
