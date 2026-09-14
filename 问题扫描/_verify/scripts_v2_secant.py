
import math
# 逐字复刻 lib/plate_solve/cpp/ipv/src/ipv_select.cpp::estimate_mag_lim_iterative (HEAD)
P = dict(alpha_prior=0.2885, alpha_min=1e-3, alpha_max=100.0, safety=3.0,
         m0_exposure=180.0, m0_offset=-4.0, clamp_lo=6.0, clamp_hi=22.0,
         zero_step=3.0, cap_per_file=200000.0, max_iter=4, tol=0.1)

def llround(x):
    # C++ std::llround: 四舍五入到最近整数, .5 远离 0
    import decimal
    return int(decimal.Decimal(repr(x)).quantize(0, rounding=decimal.ROUND_HALF_UP))

def iterate(query, n_target, focal, exposure, p, trace):
    out = dict(m_final=0.0, q=0, n=0, converged=False, capped=False,
               qfail=False, valid=False, alpha=p['alpha_prior'])
    if n_target <= 0:
        return out
    lo = min(p['clamp_lo'], p['clamp_hi']); hi = max(p['clamp_lo'], p['clamp_hi'])
    m0_hi = min(hi, 13.0)
    safety = p['safety'] if p['safety'] > 0 else 3.0
    tol = p['tol'] if p['tol'] > 0 else 0.1
    max_q = p['max_iter'] if p['max_iter'] > 0 else 4
    zero = p['zero_step'] if p['zero_step'] > 0 else 3.0
    cap = p['cap_per_file']
    alpha = p['alpha_prior'] if p['alpha_prior'] > 0 else 0.2885
    N_target = n_target * safety
    f = max(focal, 1.0); t = max(exposure, 0.1)
    m = 6.0 + 1.5*math.log10(f) + 2.0*math.log10(t) + p['m0_offset']
    m = min(max(m, lo), m0_hi)
    m_prev = 0.0; logN_prev = 0.0; have_prev = False
    m_ok = 0.0; n_ok = 0
    for it in range(max_q):
        rc, n_ret = query(m)
        out['q'] += 1
        if rc != 0:
            out['qfail'] = True; trace.append((it, m, 'FAIL')); break
        out.update(m_final=m, n=n_ret, valid=True, alpha=alpha)
        m_ok, n_ok = m, n_ret
        capped = (n_ret > 0 and cap > 0 and math.fmod(float(n_ret), cap) == 0.0)
        trace.append((it, m, n_ret))
        if capped:
            out.update(capped=True, converged=True); break
        rel = abs(n_ret - N_target)/N_target
        if rel <= tol:
            out['converged'] = True; break
        if n_ret == 0:
            have_prev = False
            m = min(max(m + zero, lo), hi); continue
        logN = math.log10(float(n_ret) + 0.5)
        if have_prev and m != m_prev:
            a_new = (logN - logN_prev)/(m - m_prev)
            if p['alpha_min'] < a_new < p['alpha_max']:
                alpha = a_new; out['alpha'] = alpha
        step = (math.log10(N_target) - logN)/alpha
        if not math.isfinite(step): step = 0.0
        step = min(max(step, -6.0), 6.0)
        m_prev, logN_prev, have_prev = m, logN, True
        m = min(max(m + step, lo), hi)
    if out['valid']:
        out['m_final'] = m_ok; out['n'] = n_ok
    return out

def powlaw(alpha, m_star, N_star, n0=0.0, cap=None):
    def q(m):
        n = N_star*10**(alpha*(m-m_star)) + n0
        if cap is not None and n > cap: n = cap
        return (0, llround(n))
    return q

tr=[]
print("== 复现 test_determinism 金标准 (alpha=0.2885, m_star=13, N_star=180) ==")
o = iterate(powlaw(0.2885,13.0,180.0), 60, 1877.0, 300.0, P, tr)
print(" trace:", [(i, round(m,6), n) for i,m,n in tr])
print(" result:", {k:(round(v,10) if isinstance(v,float) else v) for k,v in o.items()})
print(" 期望: query_count=2, m_lim_final=12.9850849855, n_returned=178, alpha=0.2885, converged")

