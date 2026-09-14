import math
print("pi/2                =", repr(math.pi/2))
print("pi/(2*sqrt2)        =", repr(math.pi/(2*math.sqrt(2))))
print("AE_C45_FACTOR 字面  =", repr(1.1107207345395915), " 差", 1.1107207345395915-math.pi/(2*math.sqrt(2)))
print("1.2 裕量 vs C45     : 1.2/1.11072073 =", 1.2/(math.pi/(2*math.sqrt(2))))
print("1.2 裕量 vs pi/2    : 1.2/1.5708 =", 1.2/(math.pi/2))
print("=> 赤道带 1.2 相对极区紧界 C45 富余 =", 1.2/(math.pi/(2*math.sqrt(2)))-1)
