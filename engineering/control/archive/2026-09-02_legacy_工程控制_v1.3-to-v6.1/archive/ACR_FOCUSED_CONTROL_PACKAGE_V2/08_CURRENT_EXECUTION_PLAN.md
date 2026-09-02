# 当前执行计划

本文件是当前唯一任务计划。保持聚焦范围，不改Phase1和真实业务算法。

## 0. 基线

- 继续`feature/astrocompute-runtime`；
- 记录最终控制包SHA；
- 保留现有CPU/CUDA执行、工作池、WorkToken、CUDA容量修复和业务零修改；
- 不新建仓库、版本分支或日期控制包。

## 1. 修复OperationProfile

- 删除按字符串搜索同名字段的读取器；
- 使用可靠JSON对象层级解析；
- 完整读取GPU数组、CPU/GPU曲线、transfer、memory、eligibility和指纹；
- roundtrip逐字段比较；
- compiler、driver和kernel hash改为真实运行指纹；
- 未测值使用明确状态/null，禁止伪零。

验收：落盘再加载后所有字段一致，路由计划一致。

## 2. 重做精简成本模型

只针对五个focused Operation：

- 分开实测CPU、GPU resident、GPU host roundtrip；
- fixed来自独立开销或拟合截距；
- 测至少3个CPU块和3个GPU块候选；
- 计算host/resident真实交叉点；
- GPU边际成本不优于CPU时路径标记not eligible，阈值为null；
- 做留出尺寸，写入真实median/P95误差；
- 每个Operation独立qualified。

不得扩展通用benchmark族。

## 3. 修复Auto设备筛选

- OperationProfile成为Auto eligibility的唯一focused依据；
- 在创建worker前执行host/resident收益阈值；
- Auto禁止“每设备先领一块”；
- ForcedMixed保留首轮参与，仅用于正确性；
- 删除慢设备等待固定轮数后强制claim；
- CPU/GPU块来自Profile实测，尾段按remaining缩小。

## 4. 完成真实驻留

- BufferBinding补充或关联真实bytes、access mode、stable key/generation；
- ResidencyManager接入CUDA device allocation缓存；
- 只读输入上传一次并跨GPU块复用；
- GPU中间输出保持resident；
- 需要CPU结果时才下载；
- 实际桥接上传/下载驱动状态和report；
- 修正当前按元素数登记bytes及机械uploaded→downloaded逻辑。

本轮不强制异步stream；先完成同步语义下的真实复用。

## 5. 修正Reduction与Drizzle合并

- 为reduction按稳定token分配FP64 partial并统一merge；
- 为drizzle-like使用输出tile独占或私有partial；
- 禁止CPU/GPU并发写同一累计数组；
- 覆盖retry attempt、非整块尾部和Mixed结果；
- 报告merge成本。

## 6. 完成容量预算

- 保留RAM和每GPU VRAM逻辑；
- 实现ACR自身pinned reservation ledger；
- claim前计入真实partial、workspace和resident复用；
- Shrink/Release/Wait/Fallback/Fail均有生产测试。

## 7. focused验收

必须运行：

- Profile完整roundtrip、holdout和无交叉点测试；
- CPU-only、GPU-only、ForcedMixed、AutoMixed；
- reduce/drizzle全路径正确性；
- small task + loaded Profile退化；
- real residency transfer-count测试；
- memory/fault/retry；
- CPU sanitizer与compute-sanitizer。

## 8. Evidence

从一个最终干净实现HEAD在仓库外生成。源码、git HEAD、测试、benchmark、Profile、sanitizer、path guard和SHA必须完全一致。

## 9. 完成定义

满足`07_TEST_AND_ACCEPTANCE.md`和`CHECKLIST.md`后，ACR底层可dormant合并main。Phase1、真实积分和Drizzle接入后续另行处理。
