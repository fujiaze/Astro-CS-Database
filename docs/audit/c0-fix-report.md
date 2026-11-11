# C0 审查 P0/P1 修复记录（2026-11-11）

> 依据 `docs/audit/c0-process-audit.md`。未改真值/容差；所有修改有实际构建或探针证据。

## P0-1 bind 身份不覆盖父观测内容

**问题**：`bind_wcs`/`bind_observation_model` 的 `revise` 只哈希 tag/frame/count/wcs 或 kernel/response；同一 frame、同样本数、不同像素值得到同一 `content_hash`/`object_id`。

**修复**：
- `src/observation/bind.cpp` 的 `revise` 现在把 `parent.content_hash` 与 `parent.revision` 纳入哈希，并同步更新 `object_id`（旧实现只改 `content_hash`，`object_id` 仍是父对象）。
- `bind_wcs`/`bind_observation_model` 的 payload 显式加入 `integrated_values` 与 `dq` 数组的内容哈希。

**证据**：`validation/observation/bind_identity_probe.cpp` 构造同一 frame、同一 index、同一父 identity，但 integrated_values 分别为 `{10,11}` 与 `{20,21}` 的两个 NativeObservation，用同一 wcs 调用 `bind_wcs`：

```
a=sha256:6108b3c2...
b=sha256:377909b7...
different=1
```

修复前两者相同；修复后不同。

## P0-2 全量构建不可复现

**问题**：`validation/process/photometry.fit/m5_process_plan.cpp` 的 `MetaAction` 未实现 `LinearAction` 新增纯虚；`tools/generate_contract_schema.py` 不能解析带默认初值的字段与嵌套 enum 默认。

**修复**：
- `MetaAction` 增加 `dependency_arrays()`/`dependency_children()` 返回空。
- schema 生成器：declarator 去掉 `= default`；nested enum 允许 `= Kind::x` 默认。

**证据**：`python3 tools/generate_contract_schema.py` → `302 definitions, 79 operation variants`；`cmake --build build/science -j8` 全量通过；`acsd_typed_smoke` 1255 checks、`acsd_host_lifecycle` 66 checks。

## P1-0 帧中位数 gauge 未记录

`src/photometry/apply_scale.cpp` 在应用帧中位数 gauge 时写入：
- `included: scale_gauge=frame_median_of_the_spatial_factors`
- `omitted: the_absolute_photometric_zero_point_is_not_measured`

避免把相对标度当成绝对定标；单位命名保持可区分。

## P1-1 astrometry 空 optional 解引用

`src/sources/astrometry.cpp` 现在先检查 `catalogue.query_quads`：取消/资源耗尽直接返回具名失败，其他失败作为该假设不成立而 `continue`，不再 `val(candidates)` 解引用空 optional。

## P1-9 information 把 conditional 当 known

`src/information/internal.cpp` 的 `covariance_has_unknown_term` 现在把 `model->state != known`、`assessment.noise == unknown` 以及任何 unknown term 都视为 unknown；`estimate_flux_information` 不再为这类输入发布 `state=known` 的 `V_F`。

## 仍未修（C0 报告 §6 其余项）

- bind 的 detector 域检查用 sample 网格；`lookup` 的 `complete=true` 与 unknown 并存。
- regroup 的 `input_is_mean` 组合维度、无父区间时名义值当精确界。
- export 数值界漏 overlap 面积区间。
- `Information` 的 `V_F` 单位不随模板 h 传播、参数数组 px/rad 混存。
- 11 个 process 无 driver；8 份 comparison 仍是旧基线。

## P1-2 lookup 的 complete 与 unknown 并存

`src/catalogue/provider.cpp` 的 `lookup` 在缺标识符或 topocentric 修正无界时仍置 `complete=true`。现在：
- 缺标识符 → `missing_identifier`，结束置 `complete=false`；
- 无界 omitted bound → `noise=unknown`、`science=conditional`、`complete=false`。

## P1-3 bind 的 detector 域检查用 sample 网格

`bind_wcs` 现在把 footprint 的 sample 包围盒（`[-0.5,width-0.5]×[-0.5,height-0.5]`）经 `detector_from_sample` 与 offset 映射到 detector 坐标，再要求 `wcs.domain` 覆盖该 bbox；恒等映射与旧的 `[-0.5,width-0.5]` 检查一致，非恒等/旋转映射不再误判。

## P1-4 information 的 V_F 单位

`estimate_flux_information` 现在把 template `h` 的单位传播到 `flux_variance.unit`：方差单位减去 `2×h` 的 linear/sr/pixel/nm power，并按 `multiplier` 调整；`h` 无量纲时保持协方差单位。参数数组 px/rad 混存仍未修。

## C0 覆盖：information.reference 解析探针

`validation/process/information.reference/reference_driver.cpp`（typed Host 驱动）用 \(C=\mathrm{diag}(4,9)\)、\(h=[1,0]\) 调 `estimate_flux_information`，解析真值 \(V_F=1/(h^\top C^{-1}h)=4\)：

```
cov code=0
known V_F=4: ok; unknown propagated: ok
```

同一探针把 `NoiseModel.state` 改为 unknown，`V_F` 变为 `nullopt/unknown`（验证 §6 的 unknown 检查）。这补上了 11 个无 driver 中 information.reference 的独立数值证据（仍非完整 process driver）。

## P1-6 参数数组单位（部分修复）

`AstrometricSolution` 现在带 `parameter_units`，按块声明 `crpix=(px,px)`、`ra0/dec0=(rad,rad)`、`cd=(rad/px)×4`、`sip_a/b=(px^-n)`，并在 `solve_astrometry` 发布时填充。`validation/process/sources.astrometry/astrometry.cpp` 新增两项检查：

```
[ok] astrometry.parameter_units_cover_the_vector   (actual 8)
[ok] astrometry.parameter_units_are_block_specific (actual px)
```

仍未闭合：`parameters` 数组本身仍是单一 Unit，`covariance_apply`/`quadratic_form` 不按块解释 `parameter_noise`；跨参数 functional 的单位推导仍可能错。彻底修法是按块拆 covariance 或让 functional 携带块索引。

## P1-6 后续：bind 的参数/单位/协方差一致性

`bind_wcs` 现在要求：`parameter_units` 非空时必须与 `parameters` 元素数一致；`parameter_noise` 的 domain 必须等于参数个数。反例（探针）：

```
mismatched parameter units refused=1
```

两个参数只给一个 Unit 的 solution 被 `invalid_model` 拒绝，不再把错配的单位/协方差带进绑定后的观测。


## C0 driver：information.reference（typed Host）

`validation/process/information.reference/reference_driver.cpp` 通过 `vpf::start` 注册 `operators.action`、`uncertainty.covariance`、`information.reference`，用 typed `MakeActionInput`/`MakeCovarianceInput`/`EstimateFluxInformationInput` 走 Host 边界：

- 正例：C=diag(4,9)、h=[1,0] → `V_F=4`、`state=known`；
- 负例：同一模型 `NoiseState::unknown` → `V_F=nullopt`、`state=unknown`。

驱动 17 checks 全过。这是 10 个无 driver process 中第一个补齐的 typed driver（不是独立探针）。

## C0 driver：uncertainty.covariance

`validation/process/uncertainty.covariance/covariance_driver.cpp` 通过 typed Host 注册 `operators.action` + `uncertainty.covariance`：

- 正例：C=diag(4,9)，`C*[1,2]=[4,18]`；`quadratic_form([1,0])=4`；
- 负例：`NoiseState::unknown` 的 C，`covariance_apply` 返回 `unavailable`（此前错误地返回 ok）。

该负例暴露并修复了一个语义漏洞：`covariance_apply` 只检查 material unknown term，没有拒绝 `state != known`。现在未知协方差不再能当作已知矩阵作用。

## C0 driver：operators.action

`validation/process/operators.action/action_driver.cpp`（typed Host）：

- 正例：identity 2×2 作用 `[1,2] → [1,2]`；
- 负例：越界 CSR 列被拒（`invalid_request`）；
- 负例：`compose(1×1 outer, 2×2 inner)` 中间维不一致——**修复前 make_action 返回 ok**，现在 `invalid_model`。

这暴露并修复了 `make_action` 只检查 children 数量、不检查 compose 中间维的漏洞；canonical `compose()` 已有检查，typed 入口此前漏掉。
