import os, collections
root="/workspace/Astro CS Database"
files=open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines()
top=collections.Counter(f.split("/")[0] for f in files)
print(top.most_common(40))
