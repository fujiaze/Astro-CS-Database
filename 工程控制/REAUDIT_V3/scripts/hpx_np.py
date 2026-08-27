
import numpy as np
PI = np.pi; TWO_PI = 2*np.pi; HALF_PI = 0.5*np.pi; K2O3 = 2.0/3.0; KR3 = np.sqrt(3.0)

def xy_to_nest(ix, iy, bits):
    ip = np.zeros(np.broadcast(ix, iy).shape, dtype=np.int64)
    for i in range(bits):
        ip |= ((ix >> i) & 1) << (2*i)
        ip |= ((iy >> i) & 1) << (2*i+1)
    return ip

def nest_to_xy(v, bits):
    ix = np.zeros(v.shape, dtype=np.int64); iy = np.zeros(v.shape, dtype=np.int64)
    for i in range(bits):
        ix |= ((v >> (2*i)) & 1) << i
        iy |= ((v >> (2*i+1)) & 1) << i
    return ix, iy

def hp_to_xyz(base, px, py, dx, dy, nside):
    ns = int(nside)
    base = np.asarray(base, dtype=np.int64); px = np.asarray(px, dtype=np.float64); py = np.asarray(py, dtype=np.float64)
    x = px + dx; y = py + dy
    chp = base
    eq = np.ones(x.shape, dtype=bool)
    eq &= ~((chp <= 3) & (x + y > ns))
    eq &= ~((chp >= 8) & (x + y < ns))
    zf = np.ones(x.shape, dtype=np.float64)
    m_np = (chp <= 3) & (x + y > ns)
    m_sp = (chp >= 8) & (x + y < ns)
    zf[m_np] = 1.0; zf[m_sp] = -1.0
    xs = x/ns; ys = y/ns
    chp_eq = chp.copy()
    zoff = np.zeros(x.shape); phioff = np.zeros(x.shape)
    m = chp <= 3; phioff[m] = 1.0
    m2 = (chp > 3) & (chp <= 7); zoff[m2] = -1.0; chp_eq = np.where(m2, chp_eq-4, chp_eq)
    m3 = chp > 7; phioff[m3] = 1.0; zoff[m3] = -2.0; chp_eq = np.where(m3, chp_eq-8, chp_eq)
    z_eq = K2O3*(xs + ys + zoff)
    phi_eq = (PI/4.0)*(xs - ys + phioff + 2.0*chp_eq)
    xx = x.copy(); yy = y.copy()
    neg = zf == -1.0
    if neg.any():
        txx = yy.copy(); tyy = xx.copy(); txx = ns - txx; tyy = ns - tyy
        xx = np.where(neg, txx, xx); yy = np.where(neg, tyy, yy)
    cond = (yy >= ns) & (xx >= ns)
    phi_t = np.where(cond, 0.0, PI*(ns - yy)/(2.0*((ns - xx) + (ns - yy))))
    z_pol = np.where(phi_t < PI/4.0,
        1.0 - (PI*(ns - xx)/((2.0*phi_t - PI)*ns))**2/3.0,
        1.0 - (PI*(ns - yy)/(2.0*phi_t*ns))**2/3.0)
    z_pol *= zf
    phi_pol = np.where(base >= 8, HALF_PI*(base-8) + phi_t, HALF_PI*base + phi_t)
    z = np.where(eq, z_eq, z_pol)
    phi = np.where(eq, phi_eq, phi_pol)
    phi = np.where(phi < 0, phi + TWO_PI, phi)
    phi = np.where(phi >= TWO_PI, phi - TWO_PI, phi)
    z = np.clip(z, -1.0, 1.0)
    theta = np.arccos(z)
    return np.sin(theta)*np.cos(phi), np.sin(theta)*np.sin(phi), np.cos(theta)

def tile_pix2ang(K, parent, x, y):
    K = int(K); nside = 512*(1<<K)
    local = xy_to_nest(np.asarray(x, dtype=np.int64), np.asarray(y, dtype=np.int64), 9)
    leaf = (np.int64(parent) << 18) | local
    L = K + 9
    face = leaf >> (2*L)
    inf = leaf & ((1 << (2*L)) - 1)
    ix, iy = nest_to_xy(inf, L)
    vx, vy, vz = hp_to_xyz(face, ix, iy, 0.5, 0.5, nside)
    ra = np.arctan2(vy, vx)
    ra = np.where(ra < 0, ra + TWO_PI, ra)
    dec = np.arcsin(np.clip(vz, -1.0, 1.0))
    return ra*180.0/PI, dec*180.0/PI
