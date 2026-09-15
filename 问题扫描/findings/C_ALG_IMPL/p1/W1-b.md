# W1 片2 · C_ALG_IMPL｜P1 一条（HEAD a3a343a4）

### W1-N-04（P1）未跟踪新代码 `lib/snr_estimator/src/module_entry.cpp`：JSON 整数字段**无界转 int**（UB 面），且同文件已有正确示范 ⇒ 边界校验只做一半
- `json_get_f64` 明确放行 nan/inf 字面量（`:137-143` 注释，理由为 scale_law alpha 直传），但同一解析器产出的 double 被**四处直转 int 进科学 cfg 且无 isfinite/值域判**：`:447 patch_grid_x=(int)d`、`:449 patch_grid_y`、`:463 min_patch_samples`、`:465 max_clip_rounds`（dtype `:411` 走 u64 路径已界内，除外）。
- **后果**：NaN 或 1e300 输入时 `(int)` 转换是 **C++ 未定义行为**；**同文件 h/w 已有「小于等于 0 或大于 0x7fffffff 即拒绝」的正确写法（`:429-434`）** ⇒ 簇 6「边界校验做一半」在新代码复现。
- **限定**：该文件当前不在编译面（见 `W1-N-03`）⇒ **静态判 UB 成立、实跑触发需入库接线后验证**。related 簇 6、`C-20`（三条禁止静默兜底硬规则）、`W1-N-03`
