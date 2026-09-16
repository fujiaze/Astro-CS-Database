import math
# D5: 5x5 窗减背景后丢负残差的整流期望
sig=5.0; n=25
E=n*sig/math.sqrt(2*math.pi)
print('[D5] E[sum max(v,0)] for pure sky 5x5 = 25*sigma/sqrt(2pi) = %.4f ADU (真值 0)'%E)
for F in (50,100,250,1000):
    print('    真通量 %6.1f -> 期望读数 %8.2f  mag 偏 %+0.3f'%(F, F+E, -2.5*math.log10((F+E)/F)))
print('    5sigma 门限被整流抬高到 %.1f ADU = %.2f sigma_F(=sqrt(25*25)=50)'%(E, E/math.sqrt(n*sig*sig)))
# D4: 5x5 窗 (整格中心求和) 对 Moffat4 beta=4 总通量的占比
def prof(r2,s2): 
    q=1.0+r2/(2.0*s2); return q**-4.0
def frac(sig):
    tot=2.0*math.pi*1.0*sig*sig/3.0   # A=1 解析积分 2*pi*A*sx*sy/3
    s=0.0
    for dy in range(-2,3):
        for dx in range(-2,3):
            s+=prof(dx*dx+dy*dy, sig*sig)
    return s/tot
for s in (1.0,2.0,3.0,4.0,6.0):
    f=frac(s); print('[D4] sigma=%.1f px: 5x5 窗含 %.2f%% 总通量 -> 若按总通量消费则 mag 偏 %+.3f'%(s,f*100,-2.5*math.log10(f)))
# D4 二次施加 f_in
for s,r in ((2.0,3.69),):
    u=math.sqrt(1.0+r*r/(2.0*s*s)); fin=1.0-u**-3
    f=frac(s); print('    sigma=2,r=%.2f: f_in=%.4f, 报告 S_ap=%.4f*F_true (SNR_ap 低 %.1f%%)'%(r,fin,f*fin,(1-1.0/ (f*fin/f))*100 if False else (1-f*fin/f)*100))
# C3: 外部 CDELT+PC 头被按无旋转读
for s_deg,scale_lbl in ((3e-7,'0.5"/px@r=1024'),(1.1e-5,'0.0396"/px 生产尺度')):
    for th,lab in ((30.0,'rot30'),):
        for r in (1024.0, 1024*math.sqrt(2)):
            e=r*s_deg*2*math.sin(math.radians(th/2))*3600
            print('[C3] %s s=%.1e deg/px r=%.0f px: 静默丢旋转 -> 位置错 %.3f arcsec'%(lab,s,r,e))
# A-3: NaN 方差像素分母/分子错配
v1,w1,v2,w2=100.0,0.6,float('nan'),0.4
code=v1*w1*w1/ (w1+w2)**2; contract=v1*w1*w1/w1**2
print('[A-3] 代码 var=%.2f/A^2  契约(整颗剔除)=%.2f/A^2  低估 %.2fx'%(code,contract,contract/code))
# A-1: 父元方差丢协方差
for k in (2,4):
    w=1.0/k; code=k*v1*w*w; true=v1*(k*w)**2
    print('[A-1] 单一 drop 均分 %d 子叶: 代码父 var=%.3f v  真值=%.3f v  低估 %.2fx (ivar 高估同倍)'%(k,code/1.0,true/1.0,true/(code/1.0)))
# D9: 跨帧 ivar 权重
for ratio in (1.5,2.0,3.0):
    print('[D9] photscal_i/photscal_j=%.1f -> ivar 权重比应为 %.4f, 现按 1:%.4f 计, 被高标定帧少计 %.1f%%'%(ratio,1/ratio**2,1/ratio**2,(1-1/ratio**2)*100))
