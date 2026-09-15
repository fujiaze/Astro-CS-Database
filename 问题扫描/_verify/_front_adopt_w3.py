
import re,os,shutil
src=["问题扫描/findings_r2/W3/p1/W3-b.md","问题扫描/findings_r2/W3/p2/W3-b.md"]
txt=""
for s in src:
    if os.path.exists(s): txt+=open(s,encoding="utf-8").read()+"\n"
blocks=re.split(r"(?m)^(?=### W3-R2-)",txt)
p1=[];p2=[];other=[]
for b in blocks:
    b=b.rstrip("\n")
    if not b.startswith("### W3-R2-"): continue
    head=b.split("\n")[0]
    if re.search(r"P1",head): p1.append(b)
    elif re.search(r"\bP2\b",head): p2.append(b)
    else: other.append(head)
def put(path,title,bl):
    old=""
    if os.path.exists(path): old=open(path,encoding="utf-8").read().rstrip("\n")
    open(path,"w",encoding="utf-8").write((old+("\n\n" if old else ""))+"\n".join(bl)+"\n")
    print(path,"写入",len(bl),"条")
os.makedirs("问题扫描/findings/C_ALG_IMPL/p1",exist_ok=True); os.makedirs("问题扫描/findings/C_ALG_IMPL/p2",exist_ok=True)
put("问题扫描/findings/C_ALG_IMPL/p1/W3-b.md","W3 P1",p1)
put("问题扫描/findings/C_ALG_IMPL/p2/W3-b.md","W3 P2",p2)
print("未定级(需人工):",other)
print("分流 p1",len(p1),"p2",len(p2))
if os.path.isdir("问题扫描/findings_r2"): shutil.rmtree("问题扫描/findings_r2"); print("已删 findings_r2")

