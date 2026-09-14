import csv,subprocess
new=list(csv.DictReader(open("问题扫描/账本/FIX_LEDGER.csv",encoding="utf-8-sig")))
txt=subprocess.run(["git","show","1d2f0736:问题扫描/账本/FIX_LEDGER.csv"],capture_output=True,text=True).stdout
old=list(csv.DictReader(txt.replace("\ufeff","").splitlines()))
om={r["id"]:r for r in old}
mis=[];reg=[]
for r in new:
    d=r["evidence_file"]
    dirp="P0" if "/p0/" in d else ("P1" if "/p1/" in d else ("P2" if "/p2/" in d else "?"))
    if dirp!="?" and dirp!=r["priority"]: mis.append((r["id"],r["priority"],dirp))
    o=om.get(r["id"])
    if o and o["priority"] not in ("P?",r["priority"],""): reg.append((r["id"],o["priority"],r["priority"]))
print("目录与标注不一致:",len(mis),mis[:6])
print("相对建账提交的真改判(排除 P? 补全):",len(reg),reg[:10])
print("release_blocker=Y:",sum(1 for r in new if r["release_blocker"]=="Y"),"总:",len(new))
