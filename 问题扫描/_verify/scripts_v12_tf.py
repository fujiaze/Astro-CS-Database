import subprocess, os, re, json, collections
root="/workspace/Astro CS Database"
files = subprocess.run(["git","--no-optional-locks","ls-files"],cwd=root,capture_output=True,text=True).stdout.splitlines()
print("TRACKED FILES:", len(files))
cnt = collections.Counter(os.path.splitext(f)[1] for f in files)
print(cnt.most_common(20))
# save list
open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt"),"w").write("\n".join(files))
