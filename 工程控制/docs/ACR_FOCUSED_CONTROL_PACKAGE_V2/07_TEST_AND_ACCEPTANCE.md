# 测试与验收

## 1. Profile

必须新增：

- 完整Profile roundtrip：CPU/GPU/transfer/memory/fingerprint/eligibility逐字段一致；
- GPU样本缺失时该Operation不得qualified；
- 无真实resident测量时resident路径不得eligible；
- GPU边际成本不优于CPU时收益阈值必须为null；
- holdout误差真实写回，不能以0表示未测；
- 推荐块来自候选块实测；
- 落盘Profile重新加载后路由结果与内存对象一致。

## 2. Auto路由

- 小任务加载qualified Profile后仍不得错误启动GPU；
- host无交叉点时GPU不进入worker集合；
- resident有收益时可进入；
- 慢设备停止claim后不得按固定轮数强制重入；
- AutoMixed中位耗时距CPU-only、GPU-only和ForcedMixed实测最佳值≤10%；
- Mixed无收益时允许自然退化。

## 3. 正确性与合并

每个focused Operation比较可靠CPU参考、CPU-only、GPU-only、ForcedMixed和Auto：

- Dense FP32；
- Dense FP32输入+FP64累加；
- FP64 reduction；
- 多块Drizzle-like scatter；
- 非整块尾部；
- retry attempt；
- NaN/mask适用情况。

Reduction和Drizzle必须验证私有partial无竞态、最终merge完整、coverage准确。

## 4. 真实驻留

- 同一只读输入跨多个GPU块只执行必要上传；
- generation变化后重新上传；
- 两个连续GPU调用中间buffer不D2H；
- CPU消费前才下载；
- backend实际传输计数与report一致；
- device cache释放后状态和VRAM reservation一致。

不能仅测试ResidencyManager枚举变化。

## 5. 内存

- RAM、真实pinned ledger、每GPU VRAM独立；
- 缩块后循环重估；
- reservation成功/释放成对；
- wait恢复有效；
- fallback重新检查目标设备预算；
- OOM注入无泄漏、漏算和死锁。

## 6. 工程质量

- CPU-only和CUDA构建；
- ASan/UBSan覆盖可构建的CPU ACR生产路径；
- compute-sanitizer覆盖focused CUDA；
- 0 failed、0 timeout；
- SKIPPED原因准确；
- path guard PASS；
- Evidence全部来自最终单一干净HEAD。

旧通用测试可保留为回归，但不得用其数量替代focused验收。
