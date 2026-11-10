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
