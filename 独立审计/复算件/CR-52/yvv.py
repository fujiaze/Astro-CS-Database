import sys, math
sys.stdout.reconfigure(encoding='utf-8')

# ---- 逐字转录 sdet_image.cpp:198-288 (sdet_gaussian_blur_yvv_d, double 版) ----
def factors(sigma):
    if sigma < 2.5:
        q = 3.97156 - 4.14554 * math.sqrt(1.0 - 0.26891 * sigma)
    else:
        q = 0.98711 * sigma - 0.96330
    b0 = 1.57825 + 2.44413*q + 1.4281*q*q + 0.422205*q*q*q
    b1 = 2.44413*q + 2.85619*q*q + 1.26661*q*q*q
    b2 = -1.4281*q*q - 1.26661*q*q*q
    b3 = 0.422205*q*q*q
    B = 1.0 - (b1+b2+b3)/b0
    b1 /= b0; b2 /= b0; b3 /= b0
    M = [[0.0]*3 for _ in range(3)]
    M[0][0] = -b3*b1 + 1.0 - b3*b3 - b2
    M[0][1] = (b3+b1)*(b2+b3*b1)
    M[0][2] = b3*(b1+b3*b2)
    M[1][0] = b1 + b3*b2
    M[1][1] = -(b2-1.0)*(b2+b3*b1)
    M[1][2] = -(b3*b1 + b3*b3 + b2 - 1.0)*b3
    M[2][0] = b3*b1 + b2 + b1*b1 - b2*b2
    M[2][1] = b1*b2 + b3*b2*b2 - b1*b3*b3 - b3*b3*b3 - b3*b2 + b3
    M[2][2] = b3*(b1+b3*b2)
    nrm_d = (1.0+b1-b2+b3)*(1.0-b1-b2-b3)
    for i in range(3):
        for j in range(3):
            M[i][j] *= (1.0 + b2 + (b1-b3)*b3)
            M[i][j] /= nrm_d
    return B, b1, b2, b3, M

def yvv(img, w, h, sigma):
    B, b1, b2, b3, M = factors(sigma)
    tmp = [[0.0]*w for _ in range(h)]
    for y in range(h):                                   # 水平
        s = img[y]; t = tmp[y]
        t[0] = s[0]*(B+b1+b2+b3)
        t[1] = B*s[1] + b1*t[0] + s[0]*(b2+b3)
        t[2] = B*s[2] + b1*t[1] + b2*t[0] + b3*s[0]
        for x in range(3, w):
            t[x] = B*s[x] + b1*t[x-1] + b2*t[x-2] + b3*t[x-3]
        sW = s[w-1]
        t1 = sW + M[0][0]*(t[w-1]-sW) + M[0][1]*(t[w-2]-sW) + M[0][2]*(t[w-3]-sW)
        t2 = sW + M[1][0]*(t[w-1]-sW) + M[1][1]*(t[w-2]-sW) + M[1][2]*(t[w-3]-sW)
        t3 = sW + M[2][0]*(t[w-1]-sW) + M[2][1]*(t[w-2]-sW) + M[2][2]*(t[w-3]-sW)
        old_w2 = t[w-2]; old_w3 = t[w-3]                 # C 里 RHS 读的仍是前向值
        t[w-1] = t1
        t[w-2] = B*old_w2 + b1*t[w-1] + b2*t2 + b3*t3
        t[w-3] = B*old_w3 + b1*t[w-2] + b2*t[w-1] + b3*t2
        for x in range(w-4, -1, -1):
            t[x] = B*t[x] + b1*t[x+1] + b2*t[x+2] + b3*t[x+3]
    dst = [[0.0]*w for _ in range(h)]                     # 垂直
    for x in range(w):
        dst[0][x] = tmp[0][x]*(B+b1+b2+b3)
        dst[1][x] = B*tmp[1][x] + b1*dst[0][x] + tmp[0][x]*(b2+b3)
        dst[2][x] = B*tmp[2][x] + b1*dst[1][x] + b2*dst[0][x] + b3*tmp[0][x]
        for y in range(3, h):
            dst[y][x] = B*tmp[y][x] + b1*dst[y-1][x] + b2*dst[y-2][x] + b3*dst[y-3][x]
        sH = tmp[h-1][x]
        u1 = sH + M[0][0]*(dst[h-1][x]-sH) + M[0][1]*(dst[h-2][x]-sH) + M[0][2]*(dst[h-3][x]-sH)
        u2 = sH + M[1][0]*(dst[h-1][x]-sH) + M[1][1]*(dst[h-2][x]-sH) + M[1][2]*(dst[h-3][x]-sH)
        u3 = sH + M[2][0]*(dst[h-1][x]-sH) + M[2][1]*(dst[h-2][x]-sH) + M[2][2]*(dst[h-3][x]-sH)
        old_h2 = dst[h-2][x]; old_h3 = dst[h-3][x]
        dst[h-1][x] = u1
        dst[h-2][x] = B*old_h2 + b1*dst[h-1][x] + b2*u2 + b3*u3
        dst[h-3][x] = B*old_h3 + b1*dst[h-2][x] + b2*dst[h-1][x] + b3*u2
        for y in range(h-4, -1, -1):
            dst[y][x] = B*dst[y][x] + b1*dst[y+1][x] + b2*dst[y+2][x] + b3*dst[y+3][x]
    return dst

# ---- 参考：同 sdet_mirror_fetch(REFLECT_101) 的截尾可分离 FIR，半径 5σ ----
def mfetch(seq, n, i):
    if i < 0: i = -i - 1
    if i >= n: i = 2*n - i - 1
    return seq[max(0, min(n-1, i))]

def fir_ref(img, w, h, sigma, R=None):
    R = R or int(math.ceil(5*sigma))
    k = [math.exp(-0.5*d*d/(sigma*sigma)) for d in range(-R, R+1)]
    s = sum(k); k = [v/s for v in k]
    mid = [[0.0]*w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            mid[y][x] = sum(img[y][mfetch(list(range(w)), w, x+d)]*0 for d in range(0,0))
            acc = 0.0
            for i, kk in enumerate(k):
                acc += mfetch(img[y], w, x + (i - R)) * kk
            mid[y][x] = acc
    out = [[0.0]*w for _ in range(h)]
    col = [0.0]*h
    for x in range(w):
        for y in range(h): col[y] = mid[y][x]
        for y in range(h):
            acc = 0.0
            for i, kk in enumerate(k):
                acc += mfetch(col, h, y + (i - R)) * kk
            out[y][x] = acc
    return out

def make_symmetric(w, h, sigma_bump=6.0):
    """输入在 x 与 y 两轴均关于图像中心严格镜像对称 (x <-> w-1-x)"""
    img = [[0.0]*w for _ in range(h)]
    cx = (w-1)/2.0; cy = (h-1)/2.0
    for y in range(h):
        for x in range(w):
            r2 = ((x-cx)/sigma_bump)**2 + ((y-cy)/sigma_bump)**2
            img[y][x] = 1000.0*math.exp(-0.5*r2) + 3.0*(x-cx)**2*0.0 + 50.0
    return img

for sigma in (1.0, 2.0, 2.5, 4.0):
    w = h = 33
    img = make_symmetric(w, h)
    got = yvv(img, w, h, sigma)
    ref = fir_ref(img, w, h, sigma)
    # 对称性残差
    asym = 0.0
    for y in range(h):
        for x in range(w):
            asym = max(asym, abs(got[y][x] - got[y][w-1-x]), abs(got[y][x] - got[h-1-y][x]))
    # 误差：中心区 (距边 >=4σ) vs 边缘带 (距边 <2σ)
    R = int(math.ceil(4*sigma))
    inter_b, edge_b, inter_e, edge_e = 0.0, 0.0, 0.0, 0.0
    scale = 1000.0
    for y in range(h):
        for x in range(w):
            e = abs(got[y][x]-ref[y][x])/scale
            d = min(x, y, w-1-x, h-1-y)
            if d >= R:
                inter_b = max(inter_b, got[y][x]-ref[y][x]); inter_e = max(inter_e, e)
            else:
                edge_b = max(edge_b, got[y][x]-ref[y][x]); edge_e = max(edge_e, e)
    print(f"sigma={sigma:>4}: 对称性最大残差={asym:.3e}  内部(|e|max, 相对)={inter_e:.3e}  "
          f"边缘带(相对)={edge_e:.3e}  边缘/内部放大={edge_e/inter_e if inter_e else float('inf'):.1f}x")

# 常数输入 ⇒ 输出必须逐位等于该常数 (DC 保持)
w = h = 16
const = [[7.0]*w for _ in range(h)]
out = yvv(const, w, h, 2.0)
dev = max(abs(out[y][x]-7.0) for y in range(h) for x in range(w))
print("DC 保持 (常数 7.0, sigma=2.0) 最大偏差 =", f"{dev:.3e}")
