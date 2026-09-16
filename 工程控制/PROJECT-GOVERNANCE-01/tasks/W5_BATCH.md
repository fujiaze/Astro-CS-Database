# 第五批任务卡（W5，前台立卡）

来源：控制包 TASK_LIST 中仍未派单的项。三张卡共用一个文件。

---

## W5-CFG-002　配置登记类遗留闭合

**范围**（依据 `CFG-001` 的遗留清单与 `config/**` 现状）：
1. **plugin 级默认值**：`config/defaults.json` 与 `docs/plugins/**` 的默认值是否一一对应（缺登记/多登记/值不一致都要列）；
2. **per-module knobs**：各 `lib/**/module.yaml` 声明的可调项是否都在配置合同里登记；
3. **滤镜名语义**：`config/filters.json` 的滤镜命名/带宽语义与 `docs/contracts/**` 是否一致（旧名残留、别名冲突）；
4. **os_abi 值域**：`os_abi` 字段的合法值域是否在 schema 中封闭（枚举 vs 自由字符串）；
5. **索引归属**：`config/**` 与 `contracts/config/**` 的索引归属是否唯一（避免两处事实源）。

**要求**：逐项给「现状 → 应然 → 依据（权威条款）→ 改法」；能机器校验的加门（负例注入必红）；改配置必须保持向后兼容或给出迁移说明。

---

## W5-PKG-001　打包与产品清单一致性

**范围**：`packaging/**`（`astrocs.product.json`、`dependency-lock.json`、SBOM 生成器）+ `cmake/astrocs.product.*.json.in` + `ENGINEERING_SPEC §1` 的产物要求。

**要求**：
1. 核对**产品清单**（哪些 target/文件进包）与构建图**实际产物**是否一致（多列/漏列都算缺陷）；
2. `dependency-lock.json` 与 `third_party/**` + CMake 实际依赖是否一致（版本、来源、许可）；
3. SBOM 生成器可跑且产物可校验（给 rc 与产物摘要）；
4. `packaging/` 内的 `__pycache__` 等垃圾清理；
5. 逐项给「现状 → 应然 → 依据 → 改法」+ 机器门（若可行，负例注入必红）。

---

## W5-CPU-001　CPU profile / ISA 选择链路一致性

**范围**：`lib/infrastructure/benchmark/cpu/**`（原 `providers/cpu/**`，经 ROOT-008 迁移）+ `docs/architecture/cpu/**` + `cpu_profile` 合同。

**要求**：
1. 核对 **baseline/avx2/avx512** 三个 provider 的可达性与选择逻辑（运行时选择 vs 编译期；不支持的 ISA 是否 fail-closed）；
2. 核对 `ENGINEERING_SPEC §3`「模块不得根据 CPU 型号改变公式；cpu_profile 只影响并行/ISA，不进入科学配置」是否被违反（给检查手段）；
3. 核对文档与实现的行号/符号锚（ROOT-008 迁移后是否漂移）；
4. 等价性测试：三档 ISA 在**同一输入**下的科学结果必须一致（给容差与实测）；
5. 逐项「现状 → 应然 → 依据 → 改法」+ 门。

**注意**：迁移后路径已变（`providers/cpu/**` → `lib/infrastructure/benchmark/cpu/**`），文档/脚本里的旧路径要一并核对。