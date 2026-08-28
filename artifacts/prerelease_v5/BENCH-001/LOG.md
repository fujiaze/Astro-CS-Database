# BENCH-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS BENCH-001 行(实现硬件/affinity/NUMA/cgroup/Job 检测,输出 schema;验收=fixture/mock 和实机比对+available CPUs 受 affinity 约束); 06 §2(不得以 hardware_concurrency/nproc 单独作 worker 数); cpu_profile.schema.json(hardware 子对象同构)。

## 动作
1. lib/backend_host/hardware_inspect.{h,cpp}: astrocs_hardware_inspect_json_v1(build_id 注入)——CPU 身份(/proc/cpuinfo vendor/family/model/stepping/brand/microcode)+feature_bits/名称(ABI-002 CPUID 路径)+XCR0(OSXSAVE 探测后 target-xsave 读取)+有效 affinity 数组/计数+**available_logical_cpus=affinity∩cgroup quota**(/sys/fs/cgroup/cpu.max, 06 §2 硬性)+logical_cpus_configured+SMT/NUMA(可读时)+RAM/page_size/cache 层级+os/kernel+compiler+astrocs_build+cli_sha256(/proc/self/exe 实测)+backend_hashes(与 backends.manifest 联动)。Windows 分支留 WIN/FAT 实测。
2. schemas/hardware_inspect.schema.json(v1): 与 cpu_profile.schema hardware 子对象同构+additionalProperties=false。
3. CLI 接线: astrocs hardware inspect --json 真实现(stub 除名); backend_host cpu_features/hardware_inspect/backend_loader 编入 CLI target。
4. tests/backend/test_hardware_inspect.py 5 测试: schema 最小校验器(required+additionalProperties)/实机 ground-truth(affinity 数组==sched_getaffinity+configured==os.cpu_count)/CPU 身份 vs /proc/cpuinfo+page_size+RAM+版本串/taskset 单 CPU fixture(available=1, mock vs 实机比对)/stdout 恰一 JSON。
5. 过程修复: printf("%s", std::string) UB(输出乱码)→fputs/.c_str(); cpuid/xsave include 与 target 属性; backend_loader 引用; CMake include 目录。

## 验证
- 全量回归 unittest **131/131 OK**(新增 5)。
- fixture(taskset -c 0) vs 实机(affinity=2) 双环境比对: available_cpus 分别=1/2, 严格随 affinity; 满足"受 affinity 约束, ≠机器总数"。

## 限制与遗留
- Windows Job Object 限值分支已留口(job_object_limit=null), 随 WIN/FAT 域实测补齐。
- NUMA/SMT/物理包拓扑在受限容器/云主机上可能不可读(输出 null/false 并标注 known=false)——schema 允许, 06 §2 "可得时"语义。
- cgroup v2 cpu.max 在本容器根路径未显式限制(空)→cgroup_cpu_limit=0; 有配额场景的钳制逻辑已实现, fixture 由环境提供。

## 产物
lib/backend_host/hardware_inspect.{h,cpp}; schemas/hardware_inspect.schema.json; tests/backend/test_hardware_inspect.py; cli/main.cpp+cli/CMakeLists.txt 接线; 本日志。

## PASS 判定
硬件/affinity/cgroup/NUMA/SMT/RAM/cache/OS/编译器/hash 画像齐; 输出对 schema 有效; fixture(taskset)与实机双环境比对一致; available CPUs 严格受 affinity 约束。BENCH-001 = PASS。
