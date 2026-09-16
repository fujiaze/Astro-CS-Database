
cd "/workspace/Astro CS Database"
echo "### writer keys vs verify-read keys diff:"
python3 - <<'PY'
import re
src=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
wr_keys=set(re.findall(r'\{"([a-z_0-9]+)",', '\n'.join(src[5865:5885])))
rd_keys=re.findall(r'wr\.value\("([a-z_0-9]+)"', '\n'.join(src[5950:5971]))
print('writer 写:', sorted(wr_keys))
print('verify 读:', sorted(set(rd_keys)))
print('读了但 writer 从不写:', sorted(set(rd_keys)-wr_keys))
print('写了但 verify 不透传:', sorted(wr_keys-set(rd_keys)))
PY
echo
echo "### tests referencing p3_verify keys:"
grep -rn "module_build_id" tests/ | head -5
echo
echo "### hips_id reader anywhere:"
grep -rn "hips_id" lib/phase3_session/*.cpp lib/core/src/module_adapters.cpp cli/ | head -8
