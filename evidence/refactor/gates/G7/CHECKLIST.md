# G7 唯一生产路径 Gate Checklist

状态: **PASS** (8/8) — HEAD=`6a66aca`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | CLI不include科学内部实现 | PASS | CLI-001 `bb2d9fc`: kRules 表驱动 15 路径; cmd_* 委托 session/preset 不内联算法 |
| 2 | phase all真正传 Artifact ID/hash | PASS | CLI-002 `e5b84f3`: run manifest 哈希链 + resume hash-mismatch→8 |
| 3 | test/benchmark/doctor无stub | PASS | CLI-003 `d93054c`: test synthetic 7 组合成门真实运行; benchmark/doctor 实测 |
| 4 | direct drizzle退出 | PASS | LEG-001 `317566c`: drizzle 降级测试 wrapper + 无 2048 硬编码 |
| 5 | old Orchestrator退出 | PASS | LEG-002 `fb0d081`: 生产符号=0, CMake 零引用, PUBLIC_API 标退出 |
| 6 | AIO PipelineEngine退出调度 | PASS | LEG-003 `9451227`: engine run API caller=0; frame API 保留 |
| 7 | old Stage2退出 | PASS | LEG-004 `6a66aca`: stage2 工具不随根构建产出 |
| 8 | production link/module list无ACR | PASS | LEG-004: nm acr=0, ASTROCS_ENABLE_ACR=OFF, module list 无 ACR |

## 验证命令 (全部 exit 0)
- `python3 tools/check_cli_command_layer.py` → CLI-001_PASS
- `python3 tools/check_cli_run_preset.py` → CLI-002_PASS
- `./build/root-cmake/astrocs test synthetic --group all` → 7 tests PASS
- `python3 tools/check_legacy_exit.py` → LEGACY_EXIT_PASS
- `nm build/root-cmake/astrocs | grep -ci acr` → 0

## Gate 判定
G7 PASS (8/8)。进入 G8 (文档与质量机器门: DOC-002..005)。
