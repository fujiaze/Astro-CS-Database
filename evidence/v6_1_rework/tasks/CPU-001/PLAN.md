# CPU-001 计划: Provider 构建 (baseline / AVX2 / AVX512)

## 目标
建立真实 shared-library targets：baseline、avx2、avx512。CLI/Runtime 以最低
AMD64/SSE2 编译；高级 flags 只作用于对应 provider target。每个 provider manifest
声明 ABI、build ID、具体 feature bits、kernel entries/hash。AVX2+FMA 分别声明；
AVX512 声明 F/DQ/BW/VL 实际使用子集。允许某 kernel 只有 baseline；路由逐 kernel
fallback。禁止三份复制算法：共享模板/生成源，但每条路径必须编译成不同目标并运行
self-test。检查最终 link map 和反汇编，证明 CLI baseline 无高级 ISA 泄漏。

## 依赖
- RT-005: ModuleRegistry（provider 注册模式）
- 既有 lib/backend_host/: baseline_kernels.h/impl.inc（共享 kernel 源）、
  backend_table.inc（kernel 注册表 + self_test + get_api）、avx2_backend.cpp、
  avx512_backend.cpp、backend_loader.cpp（manifest 预检/受限加载）、
  cpu_features.cpp/h（CPUID+XGETBV 检测）

## 步骤
1. **CMake 独立 target**（根 CMakeLists.txt）：
   - `astrocs_cpu_avx2`：avx2_backend.cpp + `-mavx2 -mfma`（PRIVATE）
   - `astrocs_cpu_avx512`：avx512_backend.cpp + `-mavx512f -mavx512bw -mavx512vl -mavx512dq`（PRIVATE）
   - 均 PUBLIC 链 astrocs_contracts astrocs_common；不链入主 CLI target（astrocs）。
2. **required_features 宏化**（backend_table.inc）：`ASTROCS_BACKEND_REQUIRED_FEATURES`
   默认 0（baseline）；avx2 源定义 `ACS_FEAT_AVX2|ACS_FEAT_FMA`；avx512 源定义
   `ACS_FEAT_AVX512F`（F 为检测面位，DQ/BW/VL 子集声明在 manifest）。
3. **Manifest 生成器**（tools/gen_provider_manifests.py）：为每 provider 输出
   ABI/build ID/feature bits(含子集名)/kernel entries hash；机器可校验。
4. **Self-test**（tests/unit/cpu001_provider_selftest.cpp）：三份独立可执行，
   各自仅链对应 provider 库，跑 backend_self_test（handshake→allocator→cancel→
   budget→logger）并校验 backend_id/kernels/required_features。
5. **负例**（tests/unit/cpu001_negative_test.cpp）：feature bits 不满足的
   provider 预检必须 FALLBACK_BASELINE（不 REJECT、不误伤 baseline）。
6. **ISA 泄漏检查器**（tools/quality/check_isa_leak.py）：objdump 反汇编主 CLI
   无 AVX2/AVX512（ymm/zmm 寄存器 + 专属助记符）；provider 库必须含对应指令。

## 验收（全部实测）
- `ctest -R "cpu0"` 4/4 PASS（baseline/avx2/avx512 selftest + negative）
- `check_isa_leak.py`：主 CLI（build_v61 + build/cli 两份）ISA_LEAK_PASS；
  provider 库含 AVX2/AVX512 指令；selftest PASS
- `gen_provider_manifests.py`：backends=3，字段完整（baseline req=0；avx2 req=24
  =AVX2|FMA；avx512 req=32=F，子集名 F/DQ/BW/VL）
- core_|rt0 ctest 16/16 PASS（backend_table.inc 改动无回归）
- CPU-002 独立实现（hardware_inspect.cpp cgroup v1 + quota_signature），
  随本 commit 一并交付，其证据见 CPU-002 目录。
