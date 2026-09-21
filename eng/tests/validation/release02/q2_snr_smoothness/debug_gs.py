import numpy as np
W,H=512,256
X,Y=np.meshgrid(np.arange(W),np.arange(H))
U=(X-256.0)/256.0; V=(Y-128.0)/128.0
def cols(u,v,o=2):
    c=[np.ones_like(u),u,v]
    if o>=2: c+=[u*u,u*v,v*v]
    return c
S=1000.0+3.0*U+1.5*V+0.8*U*U+0.5*U*V-0.4*V*V
SL=[('A',0,192),('B',128,320),('C',256,448),('D',384,512)]
N=[s[0] for s in SL]; M={n:((X>=a)&(X<b)) for n,a,b in SL}
BC={'A':[0,2,-1,1,.5,-.7],'B':[8,-1.5,1.2,-.8,1.4,.3],'C':[-14,1,.4,1.3,-.9,.6],'D':[20,-.5,-1.8,.6,.2,1.1]}
def pe(c):
    c=list(c)+[0]*(6-len(c)); return c[0]+c[1]*U+c[2]*V+c[3]*U*U+c[4]*U*V+c[5]*V*V
B={n:pe(BC[n]) for n in N}
Z={n:S+B[n] for n in N}   # noise-free
Wn={'A':1.0,'B':0.5917,'C':0.16,'D':0.04}
def des(m): return np.stack(cols(U[m],V[m]),axis=1)
def fit(m,t,w):
    A=des(m); ww=w*np.ones(A.shape[0])
    return np.linalg.solve((A*ww[:,None]).T@A,(A*ww[:,None]).T@t[m])
def ev(cf):
    cc=cols(U,V); return sum(cf[i]*cc[i] for i in range(6))
c={n:np.zeros((H,W)) for n in N}
for it in range(8):
    res={n:Z[n]-c[n] for n in N}
    newc={}
    for n in N:
        m=M[n]; num=np.zeros((H,W)); den=np.zeros((H,W))
        for j in N:
            if j==n: continue
            mj=M[j]; num[mj]+=Wn[j]*res[j][mj]; den[mj]+=Wn[j]
        tm=m&(den>0)
        tgt=Z[n]-np.where(den>0,num/np.maximum(den,1e-300),0.0)
        newc[n]=ev(fit(tm,tgt,Wn[n]))
    c=newc
    g={n:B[n]-c[n] for n in N}
    R=sum(Wn[n]*g[n] for n in N)/sum(Wn.values())
    spread=np.median(np.std([g[n] for n in N],axis=0))
    print('it %d spread=%.4f  R_med=%.4f  cA_med=%.4f cB=%.4f cC=%.4f cD=%.4f'%(
        it,spread,np.median(R),*[np.median(c[n]) for n in N]))
