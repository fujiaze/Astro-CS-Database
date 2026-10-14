# S09 证据：端到端（`normalize→mosaic→export`）现状与阻塞

分支 `audit/acsd-science-docs-p5`。生产构建 `build/linux-sci/acsd`（Ninja/Release，`+ge31b2be66`）。
可执行文件存在且 `--help` 正常输出唯一命令树：

```
acsd --version [--json]
acsd normalize (--json <config.json> | --template [-o <path>] | --help)
acsd mosaic    (--json <config.json> | --template [-o <path>] | --help)
acsd export    (--json <config.json> | --template [-o <path>] | --help)
acsd help | acsd doctor [--json] | acsd benchmark
```

## 1 e2e 判据层当前**全部静态跳过**（9/9）

```
python3 -m pytest eng/tests/e2e/test_pipeline_e2e.py -q -rs
# 4 passed, 9 skipped
```

9 条 `E2E.*` 判据（`NormalizeSerialChain` / `MosaicSerialChain` / `ExportSerialChain` /
`ProductManifestConformance` / `Atomicity` / `VisualAcceptanceChain` / `XisfReaderMatchesProduct` /
`WcsTransformDifferential` / `PhotometrySigmaRealFrameLeg`）全部带
`pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | …")`，**不是** `skipif`。

跳过原因直指两点，**均已过期**：

| 原因字段 | 原文 | 现状核对 |
|---|---|---|
| `requires` | `build/acsd` | 该路径**不存在**（`ls build/acsd` → No such file）。本分支的可执行文件在 `build/linux-sci/acsd` |
| `why` | 「产品未在本单车道构建，且 `build/acsd` 由 commit `143af8a` 产出、早于当前 HEAD」 | 产品**已构建**（`build/linux-sci/acsd`，本分支 HEAD），该叙述对应某一轮的临时车道约束 |
| `requires` | `eng/packaging/config/templates/normalize.phase_config.json` 等三份模板 | 三份模板**均存在**（`normalize/mosaic/export.phase_config.json`） |
| `blocked_by` | `T10 构建车道` / 上游 `E2E.NormalizeSerialChain` | 上游未运行，故下游「无从进入」 |

**关键约束**：`eng/tests/e2e/test_pipeline_e2e.py` 自带**A 类自洽守卫**——用 AST 扫本文件源码，
取全部 `pytest.mark.skip` 的函数名，要求其集合**逐字段精确等于**注册的 id 映射。因此
**不能**通过把 `skip` 改成 `skipif` 或删掉 `skip` 来「打开」这一层：那会同时打破该守卫，
且属改测试以放宽判据，本任务**不做**。

## 2 本轮结论

- **端到端判据层本轮 `NOT_RUN`**（9/9 静态跳过），原因是判据层自身的静态登记，**不是**产品缺陷。
- 要真正跑通需二选一，**两者都不在本任务写面内**：
  1. 把产品构建到判据登记的字面路径 `build/acsd`（纯路径问题，不动测试）；
  2. 或修改 `eng/tests/e2e/test_pipeline_e2e.py` 的注册表并同步其 A 类守卫（改测试，需负责人授权）。
- 本任务改用**直接驱动 CLI**的方式产生端到端证据（`acsd normalize --json <cfg>` →
  `acsd mosaic` → `acsd export`），配置由 `--template` 派生；该路径不经过被跳过的判据层，
  故其读数**不得**被引用为 `E2E.*` 判据已通过。

## 3 尚未完成

- 真实输入的准备与三次命令的实际运行（详见后续追加的 `S09_E2E_RUN.md`）。
  阻塞点：`normalize` 配置需 `input_lights` / `master_bias` / `master_dark` / `master_flat` /
  `wcs.gaia_data_dir`，其中 **Gaia 数据目录**与**合格母版**是否齐备需先核对。
- `acsd doctor --json` 尚未跑。
