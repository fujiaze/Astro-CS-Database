import sys, math
sys.stdout.reconfigure(encoding='utf-8')
M64 = (1 << 64) - 1

class SM:
    def __init__(s, seed): s.state = seed
    def next(s):
        s.state = (s.state + 0x9E3779B97F4A7C15) & M64
        z = s.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)
    def u(s): return (s.next() >> 11) * (1.0 / 9007199254740992.0)
    def n(s):
        u1 = s.u()
        while u1 <= 0.0: u1 = s.u()
        u2 = s.u()
        return math.sqrt(-2.0 * math.log(u1)) * math.cos(6.28318530717958647 * u2)

def f32(x):
    import struct
    return struct.unpack('f', struct.pack('f', x))[0]

def median(v):
    v = sorted(v); n = len(v)
    return v[n//2] if n % 2 else 0.5*(v[n//2-1]+v[n//2])

K = 1.482602218505602

def fix_c(seed, nf, w, h, nan_variant, prob=0.05):
    npix = w*h
    st = SM(seed); stack = []
    spikes = [0]*npix
    for i in range(npix):
        delta = 1.0 if st.u() < prob else 0.0
        spikes[i] = delta
        base = f32(100.0 + 10.0*st.n())
        col = [f32(base + f32(2.0*st.n())) for _ in range(nf)]
        if nf >= 5:
            col[4] = float('nan') if nan_variant else f32(col[4] + 1000.0*delta)
        stack.append(col)
    return stack, spikes

def clip(vals, slo, shi, maxit):
    v = list(vals); rej_events = 0
    for it in range(maxit):
        work = [x for x in v if not math.isnan(x)]
        if not work: break
        med = median(work)
        devs = [abs(x-med) for x in v if not math.isnan(x)]
        sig = K*median(devs)
        if sig <= 0.0: break
        rej = 0
        for i, x in enumerate(v):
            if math.isnan(x): continue
            dev = x-med
            if dev < -slo*sig or dev > shi*sig:
                v[i] = float('nan'); rej += 1
        rej_events += rej
        if rej == 0: break
    keep = [x for x in v if not math.isnan(x)]
    if not keep: return float('nan'), rej_events
    return sum(keep)/len(keep), rej_events

# --- A: mean vs median gate headroom (core:296-313, seed 314159, prob=0.0) ---
stack, _ = fix_c(314159, 5, 16, 16, False, prob=0.0)
diffs = []
for col in stack:
    diffs.append(abs(sum(col)/len(col) - median(col)))
print(f"A mean/median narrow: max|mean-median| = {max(diffs):.4f}  (gate band 2.0)  pixels>1.5: {sum(1 for d in diffs if d>1.5)}")

# --- B: variant loop (core:276-292) spike rejection counts ---
for nan in (False, True):
    stack, spk = fix_c(20260907, 5, 20, 20, nan)
    tot = sum(1 for s in spk if s)
    ev = sum(clip(c, 3.0, 3.0, 5)[1] for c in stack)
    print(f"B nan_variant={nan}: spike px={tot}  clip rejected values total={ev}")

# --- C: I4 determinism fixture (999331) and perf fixture (314265): finite outputs? ---
for seed, w, h, nf in [(999331, 32, 32, 5), (314265, 648, 648, 8), (1234, 8, 8, 5), (271828, 8, 8, 5)]:
    stack, spk = fix_c(seed, nf, w, h, False)
    outs = [clip(c, 3.0, 3.0, 5)[0] for c in stack]
    bad = sum(1 for o in outs if math.isnan(o))
    print(f"C seed={seed} {w}x{h}x{nf}: spike px={sum(1 for s in spk if s)} NaN out={bad} "
          f"out range=[{min(o for o in outs if not math.isnan(o)):.2f},{max(o for o in outs if not math.isnan(o)):.2f}]")

# --- D: sigma_low=sigma_high=0 semantics on seed 1234 (core:493-506) ---
stack, _ = fix_c(1234, 5, 8, 8, False)
ev0 = sum(clip(c, 0.0, 0.0, 5)[1] for c in stack)
print(f"D sigma=0/0: rejected values total={ev0} (每列期望剔除 = 高于中位数的帧)")
