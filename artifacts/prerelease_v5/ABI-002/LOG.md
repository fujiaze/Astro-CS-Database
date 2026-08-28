# ABI-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ABI-002 行(生成 manifest/hash;CPUID+OSXSAVE+XGETBV;Windows 限制搜索/Linux 私有相对路径;预检与失败策略;验收=fake manifest/hash/ABI/unsupported ISA/path injection tests 全拒绝且无 illegal instruction); 05 §3/§6/§7。

## 动作
1. lib/backend_host/cpu_features.{h,cpp}: astrocs_cpu_detect_features_v1(CPUID(__builtin_cpu_supports)+OSXSAVE(ecx bit27)+XGETBV XCR0 位域判定: XMM|YMM→AVX 系, opmask|ZMM_Hi256|Hi16_ZMM→AVX512F, 不满足即降级)+astrocs_cpu_affinity_count_v1(sched_getaffinity/GetProcessAffinityMask——05 §3-4 非机器总核); SSE2 基线恒位; MSVC 分支留 Windows 编译口。
2. lib/backend_host/backend_loader.{h,cpp}: parse_backends_manifest(v1 结构严格解析, 不猜)+preflight_entry(①裸文件名白名单: 禁 / \\ : . 前缀 .. ②abi_version==1 ③文件存在(仅私有 backends/ 目录内解析) ④sha256 实测匹配(lib/common/crypto 单一实现) ⑤required⊆detected——不支持绝不尝试执行)+load_backend(预检先行→dlopen RTLD_NOW|RTLD_LOCAL / LoadLibraryEx 受限搜索→handshake→self_test; 任一步失败 dlclose+回退)+close_backend(唯一释放对)。
3. tools/gen_backends_manifest.py: manifest 生成器(实测 sha256/feature 位名映射/compiler/flags/selftest=pass 声明)。
4. tests/backend/fixture_backend.cpp(合法 DSO fixture: handshake+self_test OK)+loader_probe_main.cpp(探针 TU: 恒 return 0, 拒绝≠崩溃)。
5. tests/backend/test_abi_loader.py 9 测试: CPU 特征+affinity≥1 实测/合法加载 LOADED+SELFTEST_OK/fake hash→FALLBACK hash mismatch/fake abi→FALLBACK/unsupported ISA(合成未定义位 1<<62 恒不在 detected)→FALLBACK unsupported ISA 且进程存活/畸形 manifest→FALLBACK malformed/**路径注入 7 形态全 REJECT_SECURITY**(/tmp/../..\\sub/C: 盘符/隐藏/..)/文件缺失→FALLBACK/生成器 roundtrip(sha256=hashlib 独立一致)。

## 验证
- 全量回归 unittest **116/116 OK**(新增 9; 用时 65s——含 DSO 编译与探针运行)。
- 全部拒绝路径: 探针恒 exit 0 进程存活(无 illegal instruction/无崩溃), 与"拒绝不执行"合同一致。
- 执行清单重生成 idempotent(backend_host 无分类符号, 217 行不变)。

## 限制与遗留
- Windows LoadLibraryEx 受限搜索分支已实现但需 MSVC 实测——Fatduck 掉线(在线窗外/波动, 实测 ssh 超时), 按 AGENTS 离线不阻塞; 列入 WIN/FAT 域复验。
- 预检失败→"warning/backend 事件+回退 baseline"中的事件发射在 CLI run 接线(CODE 域)时串接; 本任务提供判定原语。
- dlopen 错误串未逐字透传(reason 固定), 后续如需诊断可加 dlerror 记录。

## 产物
lib/backend_host/{cpu_features,backend_loader}.{h,cpp}; tools/gen_backends_manifest.py; tests/backend/{fixture_backend.cpp,loader_probe_main.cpp,test_abi_loader.py}; 本日志。

## PASS 判定
manifest/hash 生成+六查(文件/ISA 面)实测+私有相对路径加载+受限搜索(代码面)+预检回退策略齐; fake manifest/hash/ABI/ISA/path-injection 九测试全拒绝且无崩溃。ABI-002 = PASS。
