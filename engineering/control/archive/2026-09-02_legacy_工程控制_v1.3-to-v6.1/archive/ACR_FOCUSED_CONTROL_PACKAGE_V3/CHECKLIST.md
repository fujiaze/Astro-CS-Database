# ACR聚焦版检查表

## 范围

- [ ] 仅使用`feature/astrocompute-runtime`
- [ ] Phase1、现有算法、OpenMP、Pipeline和CLI零修改
- [ ] 低负载、解析、元数据和稀疏小任务未接入
- [ ] 未扩展无关通用Benchmark

## OperationProfile

- [ ] 对象层级JSON解析和完整roundtrip
- [ ] 原始样本、重复值、候选块进入Evidence
- [ ] fixed统一换算为ns后再参与成本公式
- [ ] host输出字节与merge成本按Operation真实计算
- [ ] 交叉点在额外实测点得到验证
- [ ] recommended chunk与profitability threshold完全分离
- [ ] GPU无收益的可信Operation仍可qualified
- [ ] qualified/partial/diagnostic顶层状态一致
- [ ] 每Operation有qualification_reason
- [ ] holdout误差真实写回

## 混合路由

- [ ] 无固定cpu_share/gpu_share
- [ ] Profile可信度与GPU eligibility分离
- [ ] Auto在worker启动前筛选GPU
- [ ] GPU无收益时仍使用Profile CPU块
- [ ] 不使用5%吞吐接近门决定Mixed
- [ ] claim比较有/无当前块的预测makespan
- [ ] CPU/GPU独立块来自实测
- [ ] 阈值不覆盖推荐块
- [ ] 尾段慢设备停止claim
- [ ] 实际设备与停止原因进入ExecutionReport

## 真实驻留

- [ ] 驻留upload在worker执行前发生
- [ ] CUDA launcher真实使用device view/resident API
- [ ] resident路径无每token重复H2D
- [ ] 一次上传、多token复用、一次必要D2H
- [ ] host无收益时有明确prefetch/KeepDevice机制建立驻留
- [ ] 输出/accumulator可跨调用留在显存
- [ ] generation与cache释放正确失效
- [ ] bridge计数与ExecutionReport一致

## Partial与merge

- [ ] 调用者不猜最大token数量
- [ ] ACR管理或公开精确partial slots/bytes
- [ ] 每attempt前清零
- [ ] retry不叠加旧partial
- [ ] CPU/GPU不并发写共享累计区
- [ ] merge明确执行并计时
- [ ] CPU/GPU/ForcedMixed/Auto结果正确

## 内存与传输

- [ ] RAM、staging/pinned、每GPU VRAM独立
- [ ] resident复用不重复计费
- [ ] Shrink后重估
- [ ] Release/Wait/Fallback/Fail真实执行
- [ ] pinned名称对应真实pinned分配，否则已改名
- [ ] OOM无泄漏、漏算和死锁

## 测试与交付

- [ ] 性能测试使用qualified Operation
- [ ] 至少一个Dispatcher真实resident Auto Mixed验收
- [ ] Auto性能距实测最佳合理模式≤10%
- [ ] CPU-only和CUDA构建通过
- [ ] compute-sanitizer通过
- [ ] CPU sanitizer真实开启或准确说明限制
- [ ] 0 failed、0 timeout
- [ ] path guard PASS
- [ ] Evidence来自最终单一干净HEAD
- [ ] 源码HEAD与Evidence HEAD一致
- [ ] 包含源码、原始日志、manifest和SHA
- [ ] dormant合并后普通AstroCS零副作用
