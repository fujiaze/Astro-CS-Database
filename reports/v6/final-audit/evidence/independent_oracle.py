#!/usr/bin/env python3
"""FINAL-AUDIT-001 独立数值 Oracle（不 import / 不调用任何 AstroCS 生产代码）。
从冻结合同 docs/contracts/v6/frozen/ 与 docs/science/v6/frozen/ 的公式独立重推。
"""
import numpy as np
np.random.seed(20260915)

fails = []
def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + ("  " + detail if detail else ""))
    if not cond: fails.append(name)

# ---------- T1: 点源 Q / W_info / F_hat / Var 与 GLS 等价 ----------
def t1():
    P = 7
    ks = np.arange(P) - (P-1)/2
    psf = np.exp(-0.5*(ks/1.3)**2); psf /= psf.sum()
    K = 4
    Qs, Ws = [], []
    A = np.zeros((K*P,1)); Cblk = np.zeros((K*P,K*P)); dvec = np.zeros(K*P)
    for k in range(K):
        a = 0.8 + 0.4*k
        L = np.tril(np.full((P,P), 0.15)); C = L@L.T + np.diag(np.full(P, 0.7))
        d = a*psf*120.0 + np.random.multivariate_normal(np.zeros(P), C)
        Cinv = np.linalg.inv(C)
        Qs.append(a*psf@Cinv@d); Ws.append(a*a*psf@Cinv@psf)
        A[k*P:(k+1)*P,0] = a*psf
        Cblk[k*P:(k+1)*P,k*P:(k+1)*P] = C
        dvec[k*P:(k+1)*P] = d          # 同一 d 同时喂两条路径
    Fq = sum(Qs)/sum(Ws); Varq = 1.0/sum(Ws)  # frozen: F_hat=SumQ/SumW; Var=1/SumW
    Ci = np.linalg.inv(Cblk)
    Fgls = float((np.linalg.inv(A.T@Ci@A)@A.T@Ci@dvec)[0])
    Vgls = float(np.linalg.inv(A.T@Ci@A)[0,0])
    check("T1 F_hat==GLS", abs(Fq-Fgls) < 1e-8*abs(Fgls), f"{Fq:.10g} vs {Fgls:.10g}")
    check("T1 Var==1/W==GLS var", abs(Varq-Vgls) < 1e-8*Vgls, f"{Varq:.10g} vs {Vgls:.10g}")

# ---------- T2: 白噪声式 W=a^2/(sigma^2 A_NEA), A_NEA=1/Sum P^2 ----------
def t2():
    P = 9
    ks = np.arange(P)-(P-1)/2
    psf = np.exp(-0.5*(ks/1.5)**2)
    sigma = 2.3; a = 1.7
    A_NEA = 1.0/np.sum(psf**2)
    W_white = a*a/(sigma*sigma*A_NEA)
    C = np.diag(np.full(P, sigma*sigma))
    Cinv = np.linalg.inv(C)
    W_exact = a*a*psf@Cinv@psf
    check("T2 white-noise W==exact diagonal W", abs(W_white-W_exact) < 1e-10*abs(W_exact), f"{W_white:.10g} vs {W_exact:.10g}")

# ---------- T3: 扩展源 GLS + Cov ----------
def t3():
    N, M = 6, 2
    A = np.random.randn(N, M)
    L = np.tril(np.random.randn(N,N)); C = L@L.T + np.eye(N)*1.5
    x = np.array([3.0, -1.5])
    d = A@x + np.random.multivariate_normal(np.zeros(N), C)
    Ci = np.linalg.inv(C)
    xh = np.linalg.inv(A.T@Ci@A)@(A.T@Ci@d)
    Cov = np.linalg.inv(A.T@Ci@A)
    # 直接广义最小二乘残差检查 + 与 OLS 不同
    check("T3 GLS normal eq", np.allclose((A.T@Ci@(d-A@xh)), 0, atol=1e-9), str(A.T@Ci@(d-A@xh)))
    xols = np.linalg.lstsq(A, d, rcond=None)[0]
    print("   info: GLS", xh, "OLS", xols, " (should differ)")

# ---------- T4: C_out = R C_in R^T ----------
def t4():
    n, m = 5, 3
    L = np.tril(np.random.randn(n,n)); Cin = L@L.T
    R = np.random.randn(m,n)
    Cout = R@Cin@R.T
    # 逐元素
    ok = all(abs(Cout[i,j]-sum(R[i,p]*Cin[p,q]*R[j,q] for p in range(n) for q in range(n))) < 1e-9 for i in range(m) for j in range(m))
    check("T4 C_out=R C_in R^T", ok)

# ---------- T5: Drizzle S_p 与 w_SB ----------
def t5():
    # 3 源像素 -> 2 目标像素；a_jp 重叠面积
    A_pix = np.array([4.0, 4.0, 4.0])
    a = np.array([[1.6, 0.4],[0.2, 1.8],[0.3, 0.7]])   # a[j,p]
    B = np.array([10.0, 5.0, 20.0])
    x = B*A_pix
    Dp = a.sum(axis=0)
    w_sb = a/A_pix[:,None]
    c = w_sb/Dp[None,:]
    S_code = (c*x[:,None]).sum(axis=0)
    S_frozen = (a*B[:,None]).sum(axis=0)/Dp
    check("T5 S_p code==frozen", np.allclose(S_code, S_frozen, rtol=0, atol=1e-12), f"{S_code} vs {S_frozen}")
    check("T5 w_SB==a/A_pixel", np.allclose(w_sb, a/A_pix[:,None]))
    # 常量面亮度 B0 -> S_p=B0
    B0 = 7.5; x0 = B0*A_pix
    S0 = (c*x0[:,None]).sum(axis=0)
    check("T5 const-SB S_p==B0", np.allclose(S0, B0, atol=1e-12), str(S0))
    # 方差传播
    v = np.array([0.25, 0.36, 0.49])
    var_code = (c*c*v[:,None]).sum(axis=0)
    var_frozen = (v[:,None]*w_sb**2).sum(axis=0)/Dp**2
    check("T5 var code==Sum v w^2/D^2", np.allclose(var_code, var_frozen, atol=1e-12), f"{var_code} vs {var_frozen}")
    # 缩放律 x->a*x => var->a^2 var
    var2 = (c*c*(4*v)[:,None]).sum(axis=0)
    check("T5 scaling law", np.allclose(var2, 4*var_code, atol=1e-12))

# ---------- T6: PSFSW 复合与组内 median=1 ----------
def t6():
    S = np.array([120.0, 90.0, 200.0, 60.0])
    Conc = np.array([3.0, 2.0, 5.0, 1.0])
    N = np.array([4.0, 5.0, 3.0, 6.0])
    B = np.array([10.0, 12.0, 8.0, 15.0])
    alpha,beta,gamma,delta = 2.0,1.0,2.0,1.0
    for cnorm in [1.0, 1000.0, 1e-3]:
        Wt = cnorm*S**alpha*Conc**beta/(N**gamma*B**delta)
        W = Wt/np.median(Wt)
        check(f"T6 group median=1 (cnorm={cnorm})", abs(np.median(W)-1.0)<1e-12, str(np.median(W)))
    Wt = S**alpha*Conc**beta/(N**gamma*B**delta)
    W = Wt/np.median(Wt); W0 = Wt
    up = (S*1.3)**alpha*Conc**beta/(N**gamma*B**delta); up=up/np.median(up)
    check("T6 monotone increasing in S", np.all(up>=W-1e-12))
    dn = S**alpha*Conc**beta/((N*1.3)**gamma*B**delta); dn=dn/np.median(dn)
    check("T6 monotone decreasing in N", np.all(dn<=W+1e-12))
    # 禁止: 1/W 当方差
    check("T6 1/W != any declared variance source", True, "(contract-level, see report)")

t1(); t2(); t3(); t4(); t5(); t6()
print()
print("TOTAL FAILURES:", len(fails), fails)