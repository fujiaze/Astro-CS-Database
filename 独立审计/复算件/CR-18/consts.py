import sys, math
sys.stdout.reconfigure(encoding='utf-8')
exact_moff = 2*math.sqrt(2*(2**0.25-1))
print("Moffat4 beta=4 FWHM factor exact =", repr(exact_moff))
for lit in (1.230310,):
    print(" literal", lit, " rel diff = %.6e" % ((lit-exact_moff)/exact_moff))
gauss = 2*math.sqrt(2*math.log(2))
print("gauss FWHM exact =", repr(gauss), " lit 2.3548200450309493 rel = %.3e" % ((2.3548200450309493-gauss)/gauss))
print("ratio 2.3548200450309493/1.230310 = %.6f   /exact = %.6f" % (2.3548200450309493/1.230310, 2.3548200450309493/exact_moff))
s = math.sqrt(math.pi/2)
print("sqrt(pi/2) =", repr(s), " lit 1.253 rel = %.6e" % ((1.253-s)/s))
print("sqrt(200)/1.253 =", math.sqrt(200)/1.253, "  (comment says ~11x at N=200)")
print("1.253*0.05/sqrt(200) =", 1.253*0.05/math.sqrt(200), " oracle kOracleZpSE = 0.00443002398413372")
print("with sqrt(pi/2):", s*0.05/math.sqrt(200), " rel diff vs 1.253 = %.3e" % ((s-1.253)/1.253))
