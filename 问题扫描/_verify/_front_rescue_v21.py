
import subprocess,re,os
def show(rev,p):
    r=subprocess.run(["git","--no-optional-locks","show",rev+":"+p],capture_output=True,text=True)
    return r.stdout
old=show("HEAD~1","问题扫描/findings/C_ALG_IMPL/p1/V21.md")
def cut(t):
    i=t.find("### V21-N-")
    if i<0: return []
    parts=re.split(r"(?m)^(?=### V21-N-)",t[i:])
    return [p.rstrip("\n") for p in parts if p.strip().startswith("### V21-N-")]
ob=cut(old); cur=open("问题扫描/findings/C_ALG_IMPL/p1/V21.md",encoding="utf-8").read()+open("问题扫描/findings/C_ALG_IMPL/p2/V21.md",encoding="utf-8").read()
p1=open("问题扫描/findings/C_ALG_IMPL/p1/V21.md",encoding="utf-8").read()
p2=open("问题扫描/findings/C_ALG_IMPL/p2/V21.md",encoding="utf-8").read()
def ids(t): return set(re.findall(r"### V21-N-(\d+)",t))
have=ids(cur); miss=[b for b in ob if re.match(r"### V21-N-(\d+)",b).group(1) not in have]
add1=[b for b in miss if "(P1" in b.split("\n")[0] or "（P1" in b.split("\n")[0] or "(P1）" in b]
add2=[b for b in miss if b not in add1]
open("问题扫描/findings/C_ALG_IMPL/p1/V21.md","w",encoding="utf-8").write(p1.rstrip("\n")+"\n\n"+"\n\n".join(add1)+"\n" if add1 else p1)
open("问题扫描/findings/C_ALG_IMPL/p2/V21.md","w",encoding="utf-8").write(p2.rstrip("\n")+"\n\n"+"\n\n".join(add2)+"\n" if add2 else p2)
allf=p1+ ("" if not add1 else "\n".join(add1)) + p2 + ("" if not add2 else "\n".join(add2))
print("旧块",len(ob),"缺失",len(miss),"回 p1",len(add1),"回 p2",len(add2))
print("现存 V21 id",sorted(ids(open("问题扫描/findings/C_ALG_IMPL/p1/V21.md",encoding="utf-8").read())|ids(open("问题扫描/findings/C_ALG_IMPL/p2/V21.md",encoding="utf-8").read()),key=int))

