# AstroCS 最高设计细节规范

> 上游：ASTROCS_DESIGN.md §0.1（唯一权威链）、§0.2（详细文档层与双向索引）

文档 ID：`ASTROCS-PROJECT-SPEC-002`  
文档活动分类：以 `docs/DOCUMENT_INDEX.yaml` 登记为准（由 `eng/tools/doccheck/check_doc_index.py` 现场校验；本文不自证状态，依 `ASTROCS_DESIGN.md` §0.2/§11）  
定位：`ASTROCS_DESIGN.md` 之下，项目目标态设计的细节总入口。本文描述 AstroCS 必须做到什么，不描述某次工程修复流水账。

## 1. 权威体系

1. 最高权威是根 `ASTROCS_DESIGN.md`（§0 权威链）；本文是其下的项目目标态细节总入口。
2. 本文冻结产品目标、科学目标、Phase 边界和目标态组成。
3. 三阶段详细设计分别位于：
   - `docs/design/PHASE1_DETAILED_DESIGN.md`
   - `docs/design/PHASE2_DETAILED_DESIGN.md`
   - `docs/design/PHASE3_DETAILED_DESIGN.md`
4. 跨阶段科学量与公式由 `docs/science/UNIFIED_SCIENCE_MODEL.md` 统一；专项 SCI/ALG/DATA/API 必须向它收敛。
5. 文献总档案为 `docs/references/SCIENTIFIC_REFERENCES.md`；现行目标只由本设计文档集定义。

## 2. 项目使命

AstroCS 将单帧 CCD/CMOS 观测转换为可独立消费的球面科学产品，将多帧球面产品按明确科学目标合成为统计可解释的马赛克，再把任意兼容 HiPS 导出为测量意义明确的 WCS FITS。

三个 Phase 独立启动、独立恢复、独立验收，只通过原子磁盘产品和 manifest 交换。系统必须同时服务：

- 扩展源/面亮度无偏重建；
- 点源最大信噪检测和最小方差测光；
- 天体测量与光度一致性；
- 可追溯的不确定度、相关性、有效性和排异；
- 明确隔离的视觉质量/筛帧质量。

## 3. 统一观测模型

所有科学处理都从 `d_k=A_k x+n_k, Cov(n_k)=C_k` 出发。`A_k` 包含光度响应、PSF、像素响应、WCS 和重采样。代码优化不能改变该模型；任何标量化、独立噪声、平稳 PSF 或对角 covariance 假设都必须有适用域和误差门。

## 4. Phase1 必须做到什么

Phase1 必须从一帧 light 和校准资料生成完整单帧观测模型，而非只生成可显示 HiPS。目标输出包含：

- 校准 signal 及单位；像素 variance/ivar 和相关噪声描述；
- validity/support/coverage；
- 空间 PSF 模型、WCS、光度响应及各自不确定度；
- 点源信息权重 `W_psf=a²PᵀC⁻¹P` 或其空间模型；
- 逐源 flux/variance/SNR，固定参考条件下的 depth m5；
- Drizzle 线性算子/相关性摘要和球面产品；
- 完整 provenance 与原子发布。

实际恒星样本的 median SNR 只是诊断，不能代表图像固有信噪能力，也不能成为 Phase2 科学权重。一帧一个信息系数只在空间均匀性及功率损失门通过后允许；否则输出 map/model/control points。

## 5. Phase2 必须做到什么

Phase2 验证输入兼容并建立 overlap graph；帧间乘性光度响应已在 Phase1 测光归一化中吸收，Phase2 只处理加性天光（UPM），随后执行可解释排异与逆方差集成，产出统一的面亮度球面层级。

- 产品信号统一为面亮度（surface brightness），以天球像素为单位消费各帧该像素的 SNR 现场计算权重 w=SNR²/F_ref² 做逆方差集成，权重是消费时的派生量，不存在独立的 weight 产品或权重模式；
- 点源检测功率以 `Q=aPᵀC⁻¹d`、`W=a²PᵀC⁻¹P`（point information）作为统计最优性的对拍与验证口径（实验单元二已对拍），不作为用户选择的独立产品模式；
- 输出包含 signal、variance、effective PSF、coverage/validity/rejection、UPM 参数与 provenance；不输出语义模糊的 weight 图。

## 6. Phase3 必须做到什么

Phase3 接收任意兼容 HiPS，按选定投影与采样导出标准 WCS 平面 FITS。它负责投影、重采样、不确定度/PSF 传播、流式 FITS 和原子发布，不重新计算上游质量权重。

投影（TAN/SIN/CAR/AIT 等）、HEALPix/HiPS、FITS 关键字和像素中心遵循外部标准；投影算法内置多种，启用口径以科学验证为准（当前冻结 TAN）。采样核是产品语义，由科学文档冻结，不能由现有代码倒推；variance、PSF 和离散 mask 分别传播。

## 7. 数据对象不可混淆

`signal`、`variance/ivar`、帧级 SNR 与稀疏相对 SNR 层、`quality`、`support`、`coverage`、`validity`、`rejection`、UPM 参数和 `provenance` 是不同合同对象。禁止模糊 `weight/value/mask/snr` 承载多个含义；权重是 Phase2 消费 SNR 时的现场派生量，不是独立合同对象。

## 8. 科学正确性门

- 科学定义先于算法，算法先于实现；不得以当前程序输出生成唯一 expected；
- 每个模型有解析/独立高精度/Monte Carlo Oracle；
- 注入点源验证理论 information 与实测 flux variance；
- 独立帧条件下验证 `SNR_combined²=ΣSNR_k²`，有相关项时验证简单求和被拒；
- 扩展源验证常量面亮度、梯度、总通量和 covariance；
- 任何压缩/近似用 mutation 证明门能红；
- 真实 M42/银心验证接缝、背景、星形、排异、黑洞和预测噪声。

## 9. 软件与运行目标

单一 `astrocs` CLI；CPU-only 生产；ACR/GPU dormant。科学模块独立可验证，Runtime 统一线程/内存预算，AIO 统一 FITS/HiPS/manifest 和原子发布。执行块、worker、ISA 由 profile 选择，不进入 phase_config，不改变科学结果。

## 10. 状态与发布

每个 Phase 分别标 CONTRACT_READY、IMPLEMENTED、INSTALLED、VERIFIED 或 unavailable。合成测试或其他平台上的可用节点不等于真实数据/Windows VERIFIED。允许只发布通过验收的 Phase1/2；只有负责人作最终发布决定。

## 11. 当前迁移原则

现有 FROZEN SCI、代码和旧工程包凡与本规范冲突，均作为待迁移基线而非反证。本轮优先完成三阶段设计和统一科学合同，再做合同/代码差距审计，最后实施；不得再次把工程现状写进目标规范主体。
