
import re, os
# 找出 ABI 结构体局部声明未初始化 (无 {} / 无 = {}) 的站点
PAT = re.compile(r'\b(P3WcsDescriptor|P3Provenance|P3OutputResult|acs_error_info_v1|acs_module_api_v1|acs_module_descriptor_v1|SnrFrameScienceConfig|SnrFrameScienceResult|P2UpmBuildConfig|P2SampleConfig|P2RejectConfig|P2IntegrationConfig|DrizzleConfig|exec_cfg|hips_cfg|drz_cfg|gaia_cfg|noise_cfg|AioAbiInfo|aio_abi_info_v1|astrocs_backend_api_v1|astrocs_kernel_entry_v1|acs_host_api_v1|acs_strbuf_v1|acs_str_v1|acs_span_u8)\s+(\w+)\s*;')
HITS = []
for root in ['lib', 'cli', 'runtime', 'providers']:
    for dp, dn, fn in os.walk(root):
        if 'third_party' in dp or 'build' in dp: continue
        for f in fn:
            if not f.endswith(('.c', '.cpp', '.h')): continue
            p = os.path.join(dp, f)
            if '/tests/' in p.replace(os.sep, '/') or '_test' in f: continue
            for i, l in enumerate(open(p, encoding='utf-8', errors='ignore').read().split('\n'), 1):
                s = l.strip()
                if s.startswith('//') or s.startswith('*'): continue
                m = PAT.search(l)
                if m and not re.search(r'\{\s*\}', l):
                    HITS.append((p, i, m.group(1), m.group(2), s[:90]))
for h in HITS:
    print('%s:%d  %s %s;   |  %s' % h)
print('total non-brace-initialized ABI locals (prod):', len(HITS))
