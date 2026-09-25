import sys, math, statistics
sys.stdout.reconfigure(encoding='utf-8')

M64 = (1 << 64) - 1

class SM:
    def __init__(self, seed):
        self.state = seed
    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & M64
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)
    def uniform01(self):
        return (self.next() >> 11) * (1.0 / 9007199254740992.0)
    def normal01(self):
        u1 = self.uniform01()
        while u1 <= 0.0:
            u1 = self.uniform01()
        u2 = self.uniform01()
        return math.sqrt(-2.0 * math.log(u1)) * math.cos(6.28318530717958647 * u2)

def median(vals):
    v = sorted(vals)
    n = len(v)
    return v[n // 2] if n % 2 else 0.5 * (v[n // 2 - 1] + v[n // 2])

# ---------- FIX-CAL-D: per-frame median, w=h=12 (units) and 12x12 (properties) ----------
for (w, h) in [(12, 12), (16, 16), (8, 8), (20, 20), (24, 24)]:
    npix = w * h
    meds = []
    for base in (100.0, 200.0, 400.0):
        vals = [base + 100.0 * (i % 3) for i in range(npix)]
        meds.append(median(vals))
    print(f"FIX-CAL-D npix={npix}: per-frame medians = {meds}  (design claims 100/200/400)")

# ---------- FIX-CAL-E: hot/cold counts for seed 577215 (cosmetic group) ----------
def fix_cal_e(seed, w=20, h=20):
    npix = w * h
    st = SM(seed)
    dark = []
    bias = []
    data = []
    for i in range(npix):
        dark.append(100.0 + 2.0 * st.normal01())
        bias.append(50.0 + 0.5 * st.normal01())
        data.append(1000.0 + 5.0 * st.normal01())
    dark = [float(x) for x in dark]      # emulate float32 storage
    bias = [float(x) for x in bias]
    for (y, x) in [(3, 3), (5, 5), (5, 6), (6, 5)]:
        pass
    dark[3 * w + 3] = 100000.0
    dark[5 * w + 5] = 100000.0
    dark[5 * w + 6] = 100000.0
    dark[6 * w + 5] = 100000.0
    for dy in range(3):
        for dx in range(4):
            dark[(10 + dy) * w + (10 + dx)] = 100000.0
    return w, h, dark, bias, data

K = 1.482602218505602

def detect(dk, sigma_mult, kind):
    med = median(dk)
    mad = median([abs(v - med) for v in dk])
    sg = K * mad
    thr = med + sigma_mult * sg if kind == 'hot' else med - sigma_mult * sg
    return med, mad, sg, thr, [1 if (v > thr if kind == 'hot' else v < thr) else 0 for v in dk]

def components(mask, w, h):
    lab = [0] * (w * h)
    n = 0
    sizes = []
    for s in range(w * h):
        if not mask[s] or lab[s]:
            continue
        n += 1
        q = [s]
        lab[s] = n
        cnt = 0
        while q:
            cur = q.pop(0)
            cnt += 1
            x, y = cur % w, cur // w
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        ni = ny * w + nx
                        if mask[ni] and not lab[ni]:
                            lab[ni] = n
                            q.append(ni)
        sizes.append(cnt)
    return sizes

for seed, tag in [(577215, 'cosmetic'), (823543, 'edge-block')]:
    w, h, dark, bias, data = fix_cal_e(seed)
    for kind, arr in (('hot', dark), ('cold', bias)):
        med, mad, sg, thr, mask = detect(arr, 5.0, kind)
        cnt = sum(mask)
        # structure filter: label 8-connected components, clear those with size >= 4
        lab = [0] * (w * h)
        n = 0
        sz = []
        for s in range(w * h):
            if not mask[s] or lab[s]:
                continue
            n += 1
            q = [s]
            lab[s] = n
            c = 0
            while q:
                cur = q.pop(0)
                c += 1
                x, y = cur % w, cur // w
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < w and 0 <= ny < h:
                            ni = ny * w + nx
                            if mask[ni] and not lab[ni]:
                                lab[ni] = n
                                q.append(ni)
            sz.append(c)
        final = sum(1 for i in range(w * h) if mask[i] and sz[lab[i] - 1] < 4)
        print(f"seed={seed}({tag}) {kind}: med={med:.4f} MAD={mad:.4f} sigma={sg:.4f} thr={thr:.4f} "
              f"detect={cnt} comps={sorted(sz, reverse=True)[:6]} after_filter_kept={final}")
        if kind == 'cold':
            print(f"    cold detail: min(bias)={min(arr):.4f} thr={thr:.4f} "
                  f"pixels_below_thr={cnt}")

# ---------- FIX-CAL-C: how many spike pixels for the seeds used ----------
def fix_cal_c(seed, n_frames, w, h, nan_variant, spike_prob=0.05):
    npix = w * h
    st = SM(seed)
    spikes = 0
    for i in range(npix):
        delta = 1.0 if st.uniform01() < spike_prob else 0.0
        base = 100.0 + 10.0 * st.normal01()
        for n in range(n_frames):
            st.normal01()
        spikes += int(delta)
    return spikes

for seed, w, h, tag in [(20260907, 20, 20, 'properties variant loop'), (271828, 8, 8, 'properties NaN block'),
                        (314159, 16, 16, 'mean/median narrow'), (999331, 32, 32, 'I4 determinism'),
                        (314265, 648, 648, 'perf')]:
    print(f"FIX-CAL-C seed={seed} ({tag}) npix={w*h} spike px={fix_cal_c(seed, 5 if seed != 314265 else 8, w, h, False)}")
