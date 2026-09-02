# 测试与验收

## 1. OperationProfile

必须验证：

- JSON完整roundtrip；
- 微秒/纳秒单位回归测试；
- 已知线性曲线的交叉点精确计算；
- 理论阈值在额外实测点上得到验证；
- GPU无收益时Operation仍可qualified但GPU路径为false/null；
- 顶层`qualified/partial/diagnostic`与Operation状态一致；
- 原始样本、候选块和拟合结果均进入Evidence。

## 2. 路由

必须验证：

- qualified但GPU无收益：使用Profile CPU块，不启动GPU；
- GPU比CPU快很多时，CPU仍可因小块能提前完成而参与；
- CPU领取会形成拖尾时自然停止；
- GPU推荐块不被收益阈值覆盖；
- host、resident使用各自正确阈值；
- Auto无固定比例；
- ForcedMixed只用于correctness。

## 3. 真实驻留

必须通过Dispatcher端到端验证：

- 预取/首次建立发生在执行前；
- 一次整帧或批次H2D；
- 多个GPU token使用resident launcher；
- 每token不重复上传同一输入；
- GPU中间输出或accumulator可跨调用保留；
- 最终仅一次必要D2H；
- generation改变后重新上传；
- cache释放同步失效ResidencyManager。

直接调用桥接resident API的测试只能作为底层单测，不能替代Dispatcher验收。

## 4. Private partial与merge

- ACR管理或精确公开partial槽位容量；
- 每attempt开始前清零；
- retry不叠加失败尝试的旧值；
- CPU/GPU不并发写同一累计区；
- CPU-only、GPU-only、ForcedMixed和真实AutoMixed结果正确；
- merge成本进入报告。

## 5. 性能验收

- 性能测试必须使用`qualified=true`的Operation；
- 必须断言实际设备集合和传输次数，不能只比较总耗时；
- 至少有一个真实resident Operation证明Auto Mixed由CPU和GPU共同完成且优于合理单设备基线；
- 若实测最优是单设备，允许Auto退化，但必须说明路由原因；
- Auto中位耗时不比实测最佳合理模式差超过10%。

## 6. 内存与故障

- RAM、staging/pinned、每GPU VRAM独立预算；
- Shrink后重新估算；
- OOM、设备失败、retry、cache释放和fallback无漏算、死锁和泄漏；
- 真实pinned pool如实现，必须验证reserve/allocation/reuse/release一致。

## 7. 交付

- CPU-only与CUDA构建；
- 0 failed、0 timeout，SKIPPED理由准确；
- compute-sanitizer通过；
- CPU sanitizer真实开启或准确说明工具链限制；
- path guard PASS；
- Evidence全部来自最终单一干净HEAD；
- SHA清单可复核。
