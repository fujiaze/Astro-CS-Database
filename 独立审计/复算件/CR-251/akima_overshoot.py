# 独立复算：按 lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:46-125 的 Akima 切线+Hermite 公式
# 把 filters.json 的 T(λ) 重采样到 docs/science/PHOTOMETRY.md:230 声明的 XP 网格 336-1020 nm / 2 nm / 343 点
import sys, json, os
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
F = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"),encoding="utf-8"))["filters"]

def akima(x_src, y_src, x_dst, fill=0.0):
    n=len(x_src); m=len(x_dst); y_dst=[fill]*m
    if n<2 or m==0: return y_dst
    slope=[(y_src[i+1]-y_src[i])/(x_src[i+1]-x_src[i]) for i in range(n-1)]
    ext=[0.0]*(n+3)
    ext[0]=3*slope[0]-2*slope[1] if n>=3 else slope[0]
    ext[1]=2*slope[0]-slope[1] if n>=3 else slope[0]
    for i in range(n-1): ext[2+i]=slope[i]
    ext[n+1]=2*slope[n-2]-slope[n-3] if n>=3 else slope[n-2]
    ext[n+2]=3*slope[n-2]-slope[n-3] if n>=3 else slope[n-2]
    t=[]
    for i in range(n):
        ml2,ml1,mc,mr=ext[i],ext[i+1],ext[i+2],ext[i+3]
        w1=abs(mr-mc); w2=abs(ml1-ml2)
        t.append(0.5*(ml1+mc) if w1+w2==0 else (w1*ml1+w2*mc)/(w1+w2))
    for k,x in enumerate(x_dst):
        if x<x_src[0] or x>x_src[n-1]: y_dst[k]=fill; continue
        lo,hi,j=0,n-2,0
        while lo<=hi:
            mid=(lo+hi)//2
            if x_src[mid]<=x<=x_src[mid+1]: j=mid; break
            if x<x_src[mid]: hi=mid-1
            else: lo=mid+1
        x0,x1=x_src[j],x_src[j+1]; y0,y1=y_src[j],y_src[j+1]; dx=x1-x0
        s=(x-x0)/dx
        h00=(2*s-3)*s*s+1; h10=((s-2)*s+1)*s; h01=(-2*s+3)*s*s; h11=(s-1)*s*s
        y_dst[k]=h00*y0+h10*dx*t[j]+h01*y1+h11*dx*t[j+1]
    return y_dst

grid=[336.0+2.0*i for i in range(343)]
print("XP 网格:",grid[0],"->",grid[-1],"点数",len(grid))
bad=[]
for n,f in sorted(F.items()):
    y=akima(f["wavelength_nm"],f["value"],grid)
    ymin=min(y); ymax=max(y)
    neg=sum(1 for v in y if v<-1e-9); over=sum(1 for v in y if v>1+1e-9)
    if ymin<-1e-9 or ymax>1+1e-9:
        bad.append((n,f["n_points"],ymin,ymax,neg,over,len(y)))
print("\n== 重采样后越出 [0,1] 的曲线（按 |min| 排序）==")
for r in sorted(bad,key=lambda z:z[2]):
    print(f"  {r[0]:38s} n={r[1]:4d}  T_min={r[2]:+.4f}  T_max={r[3]:+.4f}  负值点数={r[4]}/{r[6]}  >1点数={r[5]}")
print("\n越界曲线数:",len(bad),"/45")
