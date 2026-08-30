# 统一最终 SHA 状态 — 双平台 Alpha(当前状态)

> 目标: 统一最终 SHA 重建双平台 Alpha。Agent 不宣布发布；最终外部审阅前需双平台同一 SHA。

## 代码 SHA 事实
- 源码最后代码提交: **b842899eb8fb**(WIN-006 MSVC zstd guard)。其后改动仅为 `tools/assemble_audit.py`(打包工具, 不出现在 CLI 二进制) + 证据/文档/ledger → **CLI 二进制源码在 b842899 后未变**。
- → 双平台二进制**功能一致**(同一源码); 仅内嵌 git-describe **版本字符串 SHA** 不同。

## 双平台构建状态
| 平台 | 二进制 | 版本字符串 | doctor | 构建时间 |
|---|---|---|---|---|
| Linux | `build/lnx_v5_clean_rel/astrocs` | `0.9.0-alpha.1+g5d9061f481a4.dirty` | **PASS** | 本次重建 |
| Windows | `win_rel/Release/astrocs.exe` | `0.9.0-alpha.1+gb842899eb8fb` | PASS(会话内 WIN-006/007/008) | b842899 时 |

- Linux 重建(cmake -S cli -B build/lnx_v5_clean_rel, --target astrocs, exit0)成功, doctor PASS(baseline_selftest/hardware_sanity/backends_manifest all pass), 无回归。
- **版本字符串 SHA 漂移根因**: `git describe` 内嵌**当前 HEAD**(5d9061f=证据提交), 而 Windows 二进制在 HEAD=b842899 时构建。因代码未变, 二者**功能同源**。

## 统一方案(待最终外部审阅前完成)
- 需在 **FATDUCK 窗口** 用 `git pull`(同步到 `5d9061f` HEAD)后 MSVC 重建 astrocs.exe → 版本字符串变为 `+g5d9061f481a4`, 与 Linux 统一。
- 或: 在统一冻结点(全部 Task PASS + 外部审阅就绪)同时重打两平台。
- 记录不宣称 release。
