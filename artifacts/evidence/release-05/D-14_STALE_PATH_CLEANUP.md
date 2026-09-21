# D-14：陈旧 artifacts/prerelease_v5 路径清零（GATE-501）

生成：RELEASE-05 / GATE-501。依据：RELEASE-04 ACCEPTANCE D-14、GAP_AUDIT G2-3、
GATE-501 任务书步骤 4。

## 1. 改前实测（grep 全仓）

陈旧根级路径 `artifacts/prerelease_v5/`（下划线、根级）在**可执行读写面**有 6 处：

| # | 文件:行 | 角色 | 处置 |
|---|---|---|---|
| 1 | eng/tests/backend/test_isa_variants.py:79 | 写 ISA-001 测量工件 | 改到 `artifacts/evidence/prerelease-v5/ISA-001/` |
| 2 | eng/tests/backend/test_isa_avx.py:78 | 写 ISA-002 | 同上（ISA-002） |
| 3 | eng/tests/backend/test_isa_avx2_fma.py:74 | 写 ISA-003 | 同上（ISA-003） |
| 4 | eng/tests/backend/test_isa_avx512.py:75 | 写 ISA-004 | 同上（ISA-004） |
| 5 | eng/tests/backend/test_isa_bit_manip.py:66 | **读** ISA-005 证据 | 改到证据树（ISA-005 缺位事实不变，仍显式 skipTest） |
| 6 | eng/tools/pack_audit_package.py:30 | 退役打包器 OUT 常量 | 改到 `artifacts/evidence/prerelease-v5`（工具已退役，无参 exit 2） |

任务书只点了 #1 与 #6；grep 实测同源读写面共 6 处，**全部**处理（只改输出/读取路径，
不动任何断言口径）。另两处同源退役工具常量一并归位：
eng/tools/make_capsule.py:53、eng/tools/make_rev2_capsule.py:33（均 RETIRED，exit 2）。

## 2. 未跟踪陈旧目录删除

`artifacts/prerelease_v5/`（ISA-001/002/003 共 3 个 MEASUREMENTS.csv，由旧路径的
测试运行重造）已删除；删除前确认 `git ls-files` 无该路径（未跟踪）。

## 3. 改后实测

```bash
# ① 陈旧路径的可执行面清零（仅剩历史说明/退役拒绝清单）
grep -rn 'artifacts/prerelease_v5' eng/ lib/ docs/          # 命中：注释、known_failures 历史描述、退役拒绝清单
# ② 端到端验证：ISA 后端测试现在写**已跟踪证据树**
python3 -B -m unittest discover -s eng/tests/backend -t eng/tests/backend -p 'test_isa*.py'
#    → Ran 22 tests；ISA-001/002/003 三份已跟踪 MEASUREMENTS.csv 被真实刷新
#      （git status 显示 M artifacts/evidence/prerelease-v5/ISA-00{1,2,3}/MEASUREMENTS.csv）
#      → 不再产生未跟踪的 artifacts/prerelease_v5/
```

- 写入面已在注册表显式登记：`eng/ci/checks.json` 的 `CHK-UNIT`
  新增 `dirty_ignore_prefixes: ["artifacts/evidence/prerelease-v5/"]`
  （UT-BACKEND 按设计刷新测量工件；GATE-501 同时把 `mutates_workspace` 从
  "整体跳过对比"改为"只豁免登记写面"，故必须显式登记，否则工作区对比会判红）。
- 遗留（**域外，登记不改**）：
  1. `eng/tests/backend/test_isa_variants.py::test_05@@ 编译 `backend_loader.cpp`
     失败（`aio_atomic_file.h` 不在 include 路径；头文件实际在
     `lib/infrastructure/aio/src/`）→ UT-BACKEND（linux-main 档）当前红，
     与本次路径改动无关（改动前同样失败），派单 eng/tests + aio 线；
  2. `eng/tools/quality/known_failures_baseline.py:966` 与
     `eng/ci/known_failures_baseline.json` 中的 `artifacts/prerelease_v5`
     是**历史失效描述文本**（非可执行路径），保留原样；
  3. `eng/tools/pack_audit_package.py:68` 的 `/artifacts/prerelease_v5/...`
     是退役打包器的**拒绝清单**条目，保留（删掉反而放宽打包排除面）。
