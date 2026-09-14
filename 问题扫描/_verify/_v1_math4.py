import math
def mult(c, n=400001, R=9.0):
    dx=2*R/(n-1); e2=0.0; ep=0.0
    for k in range(n):
        u=-R+k*dx; pdf=math.exp(-0.5*u*u)/math.sqrt(2*math.pi)
        if abs(u)<c:
            t=(u/c)**2; psi=u*(1-t)**2; dpsi=(1-t)*(1-5*t)
        else:
            psi=0.0; dpsi=0.0
        e2+=psi*psi*pdf*dx; ep+=dpsi*pdf*dx
    return math.sqrt(e2)/abs(ep), e2, ep
print('Tukey bisquare IRLS location SE multiplier sqrt(E psi^2)/E psi\'  (sigma/sqrt(N) units)')
for c in [4.685,4.0,6.0]:
    m,e2,ep=mult(c,120001,9.0)
    print(f'  c={c}: E[p^2]={e2:.5f} E[pprime]={ep:.5f} multiplier={m:.4f}  | median=1.2533 mean=1.0000 | code 1.253 / true = {1.253/m:.3f}')