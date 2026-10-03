# tools

ACSD 工程工具集：构建期生成器、打包与安装校验、发布脚本，以及各专题子目录下的
诊断、剖析与可视化工具。全部为按需执行的一次性工具，不参与产品运行时。

## 职责边界

- 放：构建期代码生成、产物哈希与清单核验、打包与安装树校验、发布脚本、诊断与剖析工具。
- 不放：产品运行时代码（在 `lib/`）、机器门注册面与执行器（已退场）、验收判据正文（在 `docs/`）。

## 内容

### 构建期生成与核验

- `gen_build_stamp.py` —— 构建戳生成。
- `gen_version.py` —— 版本号派生（唯一来源 = 仓库根 `VERSION`）。
- `gen_provider_manifests.py` —— provider 清单生成。
- `gen_backends_manifest.py` —— 后端清单生成。
- `gen_cfitsio_list.py` —— vendored cfitsio 源清单生成。
- `isa_sites.py`、`isa_feature_bits.py` —— ISA 旗标站点与特性位工具。
- `canonical_product_hash.py` —— 产物规范哈希。
- `source_scan.py`、`file_audit.py`、`audit_intake.py` —— 源码扫描与审计入库。
- `validate_cpu_profile.py` —— cpu_profile 合同校验。

### 打包与发布

- `pack_audit_package.py` —— 审核包打包（白名单与凭据排除保证的唯一真源）。
- `make_linux_release.py`、`make_windows_release.py` —— 平台发布打包。
- `fatduck_ps.sh`、`fatduck_put.sh` —— Fatduck 侧发布通道脚本。

### 运行期辅助

- `run_gc.py`、`round_start.sh`、`phase1_e2e_bench.py` —— 运行期清理、轮次起点与基准。
- `migrate_stage2_config.py` —— 配置迁移。
- `acsd_diagnose.py` —— 诊断入口。

### 子目录

- `arch/` —— 构建图与架构图生成。
- `astrometry/` —— 天体测量闭包度量。
- `e2e/` —— 三命令端到端链条驱动与成品渲染自检。
- `graph/` —— 运行图渲染。
- `l4_rebuild/` —— L4 重建剖析与热点定位。
- `monitoring/` —— 资源监控与内存守卫。
- `perf/` —— 性能合成链剖析。
- `quality/` —— 产物比对、帧质控与派生文档生成。
- `realdata/` —— 真实数据匹配计划。

### 交接文档

- `HANDOVER.md`、`CONTINUE.md` —— 历史交接与续作记录（含已退场门禁面的史实登记）。

---

## 附：根目录 `scripts/` 的退役（ROOT-CONSOLIDATION）

> 现态说明：本节是历史退役登记，原样保留以记录事实。下文点名的 `eng/ci/root_manifest.json`
> 与 `eng/ci/checks.json` 属已退场的机器门注册面，现已不存在，文中相关引用只作史实、
> 不再是现行可执行入口；`ENGINEERING_SPEC.md` 亦已不在仓内。

**依据**：根目录整合；根目录规范「新产物落位到对应目录，
不散落根目录」与「仓库只保留最新生产代码、自解释文档集、合同与测试」。

`scripts/` 曾是根目录规范里的固定目录之一。ROOT-008 之后该目录**已无可执行脚本**，
只剩一份 `README.md`（退役登记）。本次整合把该登记并入本文件，**删除根条目 `scripts/`**，
并同步根目录规范与当时的根清单注册表。

### 已退役脚本（ROOT-008；登记逐字保留）

| 旧路径 | 世代 | 能力去向 / 退役依据 |
|---|---|---|
| `scripts/package_audit.py` | REL-003 审核包线 | 白名单打包器（源码快照 + L0-L2 文档 + 证据 + SHA 清单 tar.gz）。其 WHITELIST 指向 `docs/refactor`、`evidence/refactor`、`lib/phase1`、`lib/core`、`lib/io`、`cli` 等**本世代已不存在的路径**，已无打包对象；同职能工具 `eng/tools/pack_audit_package.py` 随审核包线一并退役。 |
| `scripts/validate_audit.py` | REL-003 审核包线 | 审核包校验器（解包重验 SHA + 禁止项）。与上面的打包器成对，无包可验；能力由 `eng/packaging/verify_install_tree.py`（安装树校验，在役）承接安装面校验。 |

两者在当时的门禁检查项注册表上的 step 面引用数为 0，不参与任何构建/测试/门禁。
ROOT-008 任务卡判定为「与已退役的审核包线重复 ⇒ 退役（删除）」。

### 恢复方式（走 git 历史，不做副本）

```bash
git show e6d65dcdf9727899d508b46c0c33d200e835088f:scripts/package_audit.py
git show e6d65dcdf9727899d508b46c0c33d200e835088f:scripts/validate_audit.py
# 退役登记原文：git show <ROOT-CONSOLIDATION 前一提交>:scripts/README.md
```

（`e6d65dcd` = ROOT-008 执行前的 main；两条路径的最后修改提交为 `b840ed64`。）