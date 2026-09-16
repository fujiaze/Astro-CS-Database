# W2 第二轮复算（纯 stdlib 只读）
import math
D2R=math.pi/180
# --- C7: p3_wcs 四角同半球守卫 vs 跨极 ---
def build_cd(s, parity, pa):
    sgn_x = -1.0 if parity=='east_left' else 1.0
    sgn_y = -sgn_x
    cp, sp = math.cos(pa*D2R), math.sin(pa*D2R)
    return [[sgn_x*s*cp, sgn_y*s*sp],[-sgn_x*s*sp, sgn_y*s*cp]]
def pix2world(cd, crpix, crval, x, y):
    dx = (x+1.0)-crpix[0]; dy=(y+1.0)-crpix[1]
    xi = (cd[0][0]*dx + cd[0][1]*dy)*D2R
    eta= (cd[1][0]*dx + cd[1][1]*dy)*D2R
    r = math.hypot(xi,eta)
    if r >= math.pi/2: return None
    th = math.atan2(1.0,r); ph = math.atan2(-xi,eta)
    a0,d0 = crval[0]*D2R, crval[1]*D2R
    dec = math.asin(math.sin(th)*math.sin(d0)+math.cos(th)*math.cos(d0)*math.cos(ph))
    dra = math.atan2(-math.cos(th)*math.sin(ph), math.cos(d0)*math.sin(th)-math.sin(d0)*math.cos(th)*math.cos(ph))
    return ((a0+dra)/D2R)%360.0, dec/D2R
W=H=15000; s=1e-3; crpix=[(W+1)/2,(H+1)/2]; crval=(40.0,85.0)
cd=build_cd(s,'east_left',0.0)
half_diag_px=math.hypot(W/2,H/2)
print('[C7] 中心 dec=85.0 |dec|<=85 通过; 半对角=%.1f px -> 半对角张角 %.3f deg (距极仅 5.000 deg)'%(half_diag_px, half_diag_px*s))
for nm,(x,y) in {'左上(0,0)':(0,0),'左下(0,H-1)':(0,H-1),'右上(W-1,0)':(W-1,0),'右上角(W-1,H-1)':(W-1,H-1),'顶边中点':(W/2,H-1)}.items():
    o=pix2world(cd,crpix,crval,x,y); print('    %-16s -> RA=%8.3f Dec=%8.3f  %s'%(nm,o[0],o[1],'(四角均 OK => p3_wcs_make 放行)' if o else 'REJECT'))
print('    FOV 边长 = %.1f deg (<20 deg 冻结上限内) => 即使 FOV<=20 与 |dec|<=85 双满足仍可跨极'%(W*s))
# --- C4: 帧头混写 CD 与 CDELT1=CD1_1/CDELT2=CD2_2/CROTA2=0 ---
s=1.1e-5; pa=30.0; cd=build_cd(s,'east_left',pa)
cd11,cd12,cd21,cd22 = cd[0][0],cd[0][1],cd[1][0],cd[1][1]
print('[C4] PA=30 east_left s=1.1e-5: CD=[%.6e %.6e; %.6e %.6e]'%(cd11,cd12,cd21,cd22))
det_true=abs(cd11*cd22-cd12*cd21); s_true=math.sqrt(det_true)
print('    写出 CDELT1=%.6e CDELT2=%.6e CROTA2=0'%(cd11,cd22))
cr=0.0; c1=[ [cd11*math.cos(cr), -cd22*math.sin(cr)], [cd11*math.sin(cr), cd22*math.cos(cr)] ]
det_r=abs(c1[0][0]*c1[1][1]-c1[0][1]*c1[1][0]); s_r=math.sqrt(det_r)
print('    按 CDELT*CROTA2 支路重构: |det| 比 = %.4f (尺度低 %.2f%%), 且旋转丢失 %.0f deg'%(det_r/det_true,(1-math.sqrt(det_r/det_true))*100,pa))
for rad in (1024.0,2048.0):
    print('    r=%.0f px: 尺度差 %.2f px = %.2f arcsec; 叠加丢旋转后的位移 ~ %.1f arcsec'%(rad, rad*(s_true/s_r-1), rad*(s_true/s_r-1)*3600*s_r/ s_r*0 + rad*(s_true-s_r)*3600, math.hypot(rad*(s_true-s_r)*3600, rad*math.sin(pa*D2R)*(s_true*3600))))
# 正确 CROTA2 (cfitsio 口径: phia=atan2(cd21,cd11))
print('    等价 CROTA2 应为 %.1f deg (非 0)'%(math.degrees(math.atan2(cd21,cd11))))
# --- N-15: src_pixel_scale_arcsec = |CD1_1|*3600 ---
print('[N-15] |CD1_1|*3600 vs sqrt|det|*3600 (s=0.0396"/px):')
for pa in (0,15,30,45,60,80,89,90):
    cd=build_cd(1.1e-5,'east_left',pa)
    got=abs(cd[0][0])*3600; tru=math.sqrt(abs(cd[0][0]*cd[1][1]-cd[0][1]*cd[1][0]))*3600
    print('    PA=%2d: 交付 %.6f  真 %.6f  偏 %+.2f%%  %s'%(pa,got,tru,(got/tru-1)*100,'键被 >0 门槛丢弃' if got==0 else ''))
# --- C8: 210960 vs 公式 ---
f=math.sqrt(math.pi/3)*(180/math.pi)*3600
print('[C8] 公式 hp_res 系数 = %.3f ; 测试/文档用 210960 (偏 %.4f%%), 注释 211034.6 (偏 %.4f%%)'%(f,(210960/f-1)*100,(211034.6/f-1)*100))
def nside(val):
    ns=1
    while ns<val: ns<<=1
    return min(max(ns,16),1048576)
lo,hi=210960/(2**21)*1.0, f/(2**21)
print('    翻转窗: finest in (%.6f, %.6f] 时测试参考给 2^20 而生产公式给 2^21'%(f/2**21, 210960/2**21))
for t in (0.100600,0.100610,0.100620):
    print('      finest=%.6f -> 测试 nside=%d / 生产 nside=%d'%(t, nside(210960/t), nside(f/t)))
# --- C1: WcsTan RA 值域 ---
def wrap_pi(ra_deg):
    r=ra_deg
    if r>180: r-=360
    if r<-180: r+=360
    return r
print('[C1] CRVAL1=210 的帧, WcsTan 侧输出 RA=%.5f 而合同 [0,360) 应为 %.5f'%(wrap_pi(210.00033-360)+360-360+ (210.00033-360), (210.00033)%360))
print('    归一化差异恒为 360 deg 的整数倍; 交叉门用 wrap-safe 角距 => 门不可见')
