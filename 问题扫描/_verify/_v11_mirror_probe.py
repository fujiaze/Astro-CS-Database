
import ctypes as C, os, sys, importlib.util
p='lib/plate_solve/tools/ipv_abi_mirror.py'
print('### ipv_abi_mirror.py 存在:', os.path.exists(p))
spec=importlib.util.spec_from_file_location('m', p); m=importlib.util.module_from_spec(spec)
try:
    spec.loader.exec_module(m)
    names=[n for n in dir(m) if 'Params' in n or 'Result' in n]
    print('   导出符号:', names)
    for n in names:
        o=getattr(m,n)
        if isinstance(o, type) and issubclass(o, C.Structure):
            print(f'   {n}: sizeof={C.sizeof(o)} align={C.alignment(o)} 字段={len(o._fields_)}')
except Exception as e:
    print('   import 失败:', type(e).__name__, e)
