# scripts/ — 仓库根脚本目录（ENGINEERING_SPEC §7 固定目录）

本目录是 §7「其他固定目录」之一。ROOT-008 之后，这里**不再有可执行脚本**；
保留本文件是为了：本目录在干净检出中依然存在（§7 要求该条目存活；
`ci/root_manifest.json` 的 `required_dirs` 含 `scripts`），并登记下面这次退役。

## 已退役脚本（ROOT-008，2026-09-16）

| 旧路径 | 世代 | 能力去向 / 退役依据 |
|---|---|---|
| `scripts/package_audit.py` | REL-003 审核包线 | 白名单打包器（源码快照 + L0-L2 文档 + 证据 + SHA 清单 tar.gz）。其 WHITELIST 指向 `docs/refactor`、`evidence/refactor`、`lib/phase1`、`lib/core`、`lib/io`、`cli` 等**本世代已不存在的路径**，已无打包对象；同职能工具 `tools/pack_audit_package.py` / `tools/assemble_audit.py` / `tools/make_capsule.py` 已按 RETIRE-001 退役（`assemble_audit.py` 打印 `ASSEMBLE_AUDIT_RETIRED` 并 exit 2）。 |
| `scripts/validate_audit.py` | REL-003 审核包线 | 审核包校验器（解包重验 SHA + 禁止项）。与上面的打包器成对，无包可验；能力由 `packaging/verify_install_tree.py`（安装树校验，在役）承接安装面校验。 |

两者在 `ci/checks.json` 的注册 step 面引用数为 0（V21-N-07 复核），
不参与任何构建/测试/CI 门。ROOT-008 任务卡判定为「与已退役的审核包线重复 ⇒ 退役（删除）」。

## 恢复方式（走 git 历史，不做副本）

```bash
git show e6d65dcdf9727899d508b46c0c33d200e835088f:scripts/package_audit.py
git show e6d65dcdf9727899d508b46c0c33d200e835088f:scripts/validate_audit.py
```

（`e6d65dcd` = ROOT-008 执行前的 main；两条路径的最后修改提交为 `b840ed64`。）
