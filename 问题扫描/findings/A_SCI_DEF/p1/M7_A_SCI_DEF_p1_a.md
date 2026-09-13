# M7 · A_SCI_DEF · P1（第二层合并定稿：L19+L20+L21）

> **档位声明**：本文件内全部条目的 **类别与优先级由本行标题承载**（协议 §3 的「一类别×优先级一档」）；条目正文只在**偏离本档级别**时显式标注改档及理由（如 M7-A-101 记 P0→P1、M7-A-201 记 P1→P2 并撤核心结论、M7-A-001 记 P1→P0、M7-I-202 记 P1→P2 且改类 A_SCI_DEF→I_DOC_HYGIENE）。逐条四态判定与编号映射见 \`问题扫描/_merge/M7.md\` §2；每条均含 位置/权威依据/证据或证据出处/问题说明/影响/建议处置/置信度/related/四态判定 九项。

口径：每条均按当前树重读（关键句 grep 命中 + 定点 read 回读）；「判定」列＝四态判定；证据逐字原文见 `问题扫描/_cache/L19.md`／`L20.md`／`L21.md` 对应条（本文件不重复粘贴长引文）。所有位置锚为 `path::符号`，行号注「复核时 N」。

## M7-A-101 SCI-WCS §5 前向式把 SIP 写成世界坐标加数，与本文 §2/§3 声明的像素域单位及实现互斥（L19-001 改判：P0→P1）
- 位置 `docs/science/ASTROMETRY.md::§5 连续定义`（复核时 :48 SIP 加在 (u,v) 上、:52-53 `A=cd_inv·trans.x`、:29/`::§2 符号表` :21 单位 `1/pixel^{i+j-1}`、:62 翻转式）
- 权威依据 宪章 §1.1 尾句/§5.3；FITS WCS Paper I §3 + SIP（Shupe et al. 2005, ASPC 347, 491）；`STANDARDS_REGISTRY.md::D.spherical-projection SIP §A 行`（复核时 :66 PROJECT_DEFINED）
- 问题说明 本代理按任务书读实现侧定案：`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::build_fits_wcs_from_solution`（复核时 :655-656 取反 cd12/cd22；:659-670 `A[idx]*=sign_in; B[idx]*=-sign_in`，`sign_in=(j&1)?-1:1`；AP/BP :672-683、APx/BPx :685-696 同规则）与 ::::iterative_solar_tp_newton`（:861 `fx=u+fu-uvx`）证明实现是**像素域多项式**；本代理独立重推像素域翻转为 `A'_{ij}=A_{ij}(−1)^j, B'_{ij}=−B_{ij}(−1)^j` ⇒ **实现与 SCI :62 同侧且正确**（与 `L20.md::§4-5①`「自洽无错」互证）。真正的缺陷是 :48 把 SIP 项加在中间世界坐标 (u,v) 上，而同文 :21/:29 声明系数单位是 `1/pixel^{i+j-1}`（像素域），且 :52-53 的构造式（`cd_inv·trans`）本身要求像素域读法——两读法在标准域下相差一整张 CD 矩阵、在 Y 方向再差一个 `−1`。
- 影响 冻结 SCI 的前向式与其自身量纲表、其自身系数构造式互斥；施工者按 :48 实现即与现实现反号（Dec 侧），按 :21/:52 实现即与 FITS 标准键语义冲突（后者是 M9 的 L26-001 P0）。
- 建议处置 ①:48 改写为像素域式 `(u,v)=CD·[(xp−CRPIX)+SIP_A/B(xp−CRPIX)]` 并注明与 FITS 标准键的域差；②§66 的代码行锚改符号锚（见 M7-E-201）；③像素域/世界域之裁决随 L26-001 一并交负责人。
- 置信度 高（实现三处读毕 + 两侧翻转式各自独立重推）
- related L19-001（本条即其改判定稿）、L26-001（M9，像素域值直写标准键，**不同事实、P0 在其名下**）、M1a-A-003（CAR/AIT 丢 CRVAL2 致 dec(CRPIX)=0、错位至 216000 px，**事实不同，不并档**）、L20 §4-5①、L20-010、STANDARDS_REGISTRY SIP §A 行、`40_OWNER_DECISIONS.md::A-07/A-09`
- 判定 **仍成立（文本未变）＋改判降级**：L19 原 P0「实现取 −B(−1)^j ⇒ 产品 WCS 系统性符号错」被本代理读码**驳倒**；残留的 SCI 内部域自斥为纯文档互斥 ⇒ 按任务书降 P1（理由：现实现自洽且往返测试在其自身域内真通过；数值级产品错误若成立，其载体是「像素域值写入标准键」这一独立事实，归 M9/L26-001）

## M7-A-102 SCI-CW:42 断言 snr²≡x²·ivar，在 x=0 与 B≠0 两族像元上为假（L19-002）
- 位置 `docs/science/CONTROL_WEIGHT_SNR.md::§4 权重进质量场式`（复核时 :42）
- 问题说明 该恒等式是「snr² 权重≡ivar 权重」的唯一支撑；独立反例：x=0 而 B>0 ⇒ snr=0 但 ivar=(W/B)²>0（权重差无穷倍）；B≠0 时 snr²=x²/(B²/N) 与 ivar 只在 B=1/N 的特殊点相等。
- 影响 支撑 weight_mode=2 之争（A-02/M3-A-002）的代数量；按 :42 复核任一实现都会误判两者等价。
- 处置 删除恒等号或改为不等价声明并给反例域；与 L07-003/004 的 SNR 三口径裁决并置。
- related M7-A-001、M7-A-002（L21-001）、L07-003、L07-004、A-01/A-02；判定 **仍成立**（grep :42 命中）；置信度 高

## M7-A-103 SCI-NOISE 对 variance=0/饱和/无合格 patch 同文三套互斥强制，「全帧饱和」分支在其自身输入域内不可判定（L19-006）
- 位置 `docs/science/NOISE_MODEL.md::§5 ivar 式`（:53）、`::§7 Floor 夹逼不变量`、`::§8 极端表行1-2`、`::§15`
- 问题说明 常数输入帧 σ_bg=0 同时被 §5/§7 强制为 ivar=1e12、被 §8 行1 强制为 ivar=0+r=1 拒、被 §15 强制为零→ivar=0；§8 行2「全帧饱和」的输入是常数像素，与 §7 常量场分支不可区分，而 §4 输入域无饱和标志。
- 影响 退化帧逐像素权重可取 1e12（宪章 §4.1 明禁的伪有效权重形态）。
- 处置 单一决策表统一 var=0/样本不足/饱和三判；饱和判定入 qf 输入域或删行。
- related L07-002、L07-009、L07-019、L02-008、M2a-H-2（ivar==0 单裁语义，A-11）、M7-H-104（L20-027 同族）；判定 **仍成立**；置信度 高
## M7-A-104 SCI-NOISE 5 样本控制点的 MAD² 方差与满 patch 同权进无权重平面 LS（L19-008）
- 位置 `docs/science/NOISE_MODEL.md::§4 min_samples`（:37）、`::§5 控制点/空间场`
- 问题说明 本代理独立复算 MAD→σ 相对标准误 = 1.1664/√N（ARE=0.368 自洽）：N=64 ⇒ 14.6%，N=5 ⇒ 52%；控制点是 MAD² ⇒ 噪声加倍，而 §5 令 N=5 与 N=64 同权进无权重 LS 平面 ⇒ 权重场噪声由最小 patch 主导，§11 的 Gaussian 合成门只在满样本下有意义。
- 处置 min_samples 提升或在 §5 声明按 N_retained 加权；与 L07-005（5 vs 64）并案交负责人。
- related L07-005、A-03、M7-A-105；判定 **仍成立**；置信度 高

## M7-A-105 SCI-PHOT 合成注入门的参考量 F_instr=k·F_syn 未给 k 的定义与真值生成式 ⇒ 门的残差不可计算（L19-009，原类别 F_TEST_GAP，本代理改归 A_SCI_DEF 并在此说明）
- 位置 `docs/science/PHOTOMETRY.md::§11 合成注入`（:107）、`::§10 Python 参考`（:110 rtol 1e-9）、`::§2/§3 F_instr 行`
- 问题说明 该门用 F_instr 作参考，但 F_instr 被定义为「由 scale 加到 F_syn 的已知量」⇒ location=−log10 scale 是输入的代数反演（**同义反复、非独立 Oracle**，宪章 §13.1）；唯一有判别力的 σ_mag 依赖 F_true，而 F_true 在全文无定义。注：L19 原把此条记 F_TEST_GAP P2，本代理按「根因是科学量未定义」改归 A_SCI_DEF P1（改档理由随此登记）。
- 处置 独立真值改由无畸变通量真表（Gaia 合成 F_syn）给出；或显式写 `σ_mag = √(σ_res² + σ_ref²/n)` 并给 σ_ref 的生成式。
- related L07-006、L07-016、M1a-F-1/M2a-F-1（同「Oracle 共源」主题，各条自留）、L21-002（snr 无辞典条目）；判定 **仍成立**；置信度 高

## M7-A-106 SCI-PHOT sigma_cal_rel = ln10·sigma_residual 把回归散布当均值标准误，1/√n 因子全链缺失（L19-010）
- 位置 `docs/science/PHOTOMETRY.md::§2 符号表`（:29）、`::§4/§5 输出与不确定度`
- 问题说明 链式后果本代理独立复算：`|r_inliers|≥2` 起算的 σ̂ 偏 −16%（1.8773 vs 2.2255，0.8435=−15.7%）、散布 76%；σ_cal_rel 未经 1/√n 折算 ⇒ n=100 星下高估 10× ⇒ HISS 全局项相对逐像素项被压 100×；门 rtol 1e-4 对 σ=0.01 有 ~1e-11 余量 ⇒ 不可证伪（家族成员，见 `findings/G_GOV_GATE/p0/M7_G_GOV_GATE_p0.md`）。
- 处置 改 σ_cal_rel/√n_ref 并声明 n_ref 与 |r|≥2 起算偏差界。
- related L07-003、L19-008、M7-G-001、L04-016；判定 **仍成立**；置信度 高

## M7-A-108 SCI-REJ 对 auto 主用方法 percentile 未定义参考分布／经验分位口径／tie 规则，确定性门不可证伪（L19-012）
- 位置 `docs/science/REJECTION.md::§5 n<6 分支`（:46）、`::§9a/§12/§15`
- 问题说明 同一文档把 crreject 明确到 R-7 型分位数，却对 percentile 只给 (low 0.2/high 0.1)；n=3 时 R-1 与 R-7 可差一个样本值；实现若用 np.percentile 线性插值又与「取样本点」口径不同；§15 登记 SYN 时又无「同 seed 逐位一致」的可判据。
- 处置 钉死经验分位类型 + tie 规则 + 返回样本点（禁插值），登记为 SYN 判据。
- related L09-004、L09-007、M7-A-109、M7-A-125；判定 **仍成立**；置信度 高

## M7-A-109 SCI-REJ winsorized 的 1.134 被声明「流量中性」，实测为幅度中性（L19-013）
- 位置 `docs/science/REJECTION.md::§5 方法表`、`::§7`
- 问题说明 本代理按文档自身 MAD 式复算：`w=min(1,c·S_MAD/|x−median|)`（c=1.5）下 Σw≠n、Σw·x≠n·median ⇒ 收缩后均值相对中位有 O(偏度·σ²) 漂移；1.134 只是正态尺度因子（与 `L20.md::§5-A` 独立推导 W=0.882323⇒1/W=1.1334 一致，差 5e-4 为二阶）。
- 处置 删「流量中性」或补按 w 归一（/Σw）；把 1.134 的来源改成本仓自己的 ±1.5σ 推导。
- related M7-A-112（1.134 常数无出处）、L09-007；判定 **仍成立**；置信度 高

## M7-A-110 SCI-REJ percentile 的 scale=|median| 在 UPM 加性归零后的空背景主用例上退化（L19-014）
- 位置 `docs/science`：`docs/science/REJECTION.md::§5`（:46）对照 `docs/science/INTEGRATION.md::§6`
- 问题说明 输入是 calibrated=raw−C_f 的样本，空背景 median≈0±σ/√n ⇒ scale→0 ⇒ 阈→0（全拒风暴）或跨零不连续。
- 处置 scale 下限 max(|median|,σ_bg) 或改绝对阈。
- related L09-004、M7-A-113、SCI-UPM §5 加性模型；判定 **仍成立**；置信度 高

## M7-A-112 NOISE_ESTIMATION 的 k_corr 查表缺维：只给 pixfrac×scale 六值，未声明其覆盖 `::§5 式` 的 N 与 σ_k 两维，表值与冻结 1.4 无界对照（L20-013 的 SCI 侧另一半；本条与 L20-013 不同侧）
- 位置 `docs/algorithms/NOISE_ESTIMATION.md::§5 控制方差块`、`docs/algorithms/PHASE2_SAMPLER.md::§5 kcorr_lookup`（:189-193）、`docs/science/PHASE2_UPM.md::§5 k_corr=1.4 冻结`（:52）
- 问题说明 表是 pixfrac×scale 的二维查表（域外 clamp），而控制方差式的统计误差项依赖 N_retained 与核型 σ_k；文档未声明表值是否随二者变化、也未给表值与 1.4 的偏差界 ⇒ 「保守冻结」不可核；且 `scale 未知→300 档` 反保守（300 档因子大于 600 档）。
- 处置 表头补「本表仅表 N→∞、核型=drizzle 相关」的适用域，或给三维表；把「未知 scale 回退 300」的保守性方向写成可核判据。
- related L08-001、L20-013（同一主题的枚举值数与保守向，另条）、M7-A-205、A-12；判定 **仍成立**；置信度 中高（表形状按 ALG 文本，未读 MC 测试数据）

## M7-A-113 SCI-UNC 的 Var(median)≈πσ²/2N 无适用域声明，却被用于 N=10 的帧级不确定度（L19-016）
- 位置 `docs/science/UNCERTAINTY_AND_COVARIANCE.md::§n=1 与 §帧级/小样本`、`::§协方差`
- 问题说明 渐近式在 N=10 的精确值本代理复算为 0.236σ²（渐近 0.157σ²，低估 33%）；N=10 是 n=1 退化分支外的常规值；文档另声明用精确 order-statistic 方差表却无表。
- 处置 声明 N≥30 才可用渐近式，≤30 用精确表。
- related L09-003、L19-008、M7-A-110；判定 **仍成立**；置信度 高

## M7-A-114 SCI-UNC 的 Phase3 bilinear 方差传播式丢相邻协方差项，与同文档自家 ρ̄≈0.19 实测相抵（L19-018）
- 位置 `docs/science/UNCERTAINTY_AND_COVARIANCE.md::§Phase3 重采样方差传播`（:79）对照 `::§协方差`
- 问题说明 本代理独立复算（等权 4 点、全对同相关）：var_true=u(Σc²+2ρΣΣ_{k<l}c_kc_l)=u(0.25+0.75ρ)=0.3925u（ρ=0.19）⇒ 冻结式低估 36%；ρ=0.57 时低估 71%。输入像元恰是 drizzle 产物（本文自证相关）。
- 处置 标注 iid 近似 + 0.75ρ 偏差界，或对 drizzle 输入加保守膨胀。
- related L04-016、L02-008、M7-A-119；判定 **仍成立**；置信度 高

## M7-A-115 SCI-UNC 的 V19R3 括注把 k_corr 写成 N_eff，与同节 k_corr=N/N_eff≈1.4 相差约 130 倍（L19-019）
- 位置 `docs/science/UNCERTAINTY_AND_COVARIANCE.md::§V19R3 引言括注`（:32）对照同节 :43-44
- 问题说明 同节自证 N_eff≈181、k_corr≈1.4；按括注「乘 k_corr=N_eff 缩放」实现 ⇒ control_variance 差 129 倍、control_ivar 全毁。
- 处置 括注改 `k_corr=N_retained/N_eff`。
- related M7-A-205、L08-001、M7-A-001；判定 **仍成立**（grep「乘k_corr=N_eff缩放」命中 :32）；置信度 高

## M7-A-116 SCI-CAL 的「median≤0 平场保持原样不归一」分支令 flat_norm 回到 ADU 量纲，cal 被 ~1/10³ 静默缩且无状态码（L19-020）
- 位置 `docs/science/CALIBRATION.md::§3 量纲表`、`::§4 平场条`（:38）、`::§5 双分支`、`::§8/§10`
- 问题说明 §4 分支使 flat_norm:=flat 原值（ADU），与 §3「flat_norm 无量纲（median=1.0, floor 0.1）」断链；同为「平场不可用」，NULL 跳过除法、坏 flat 强除，无错误码区分。
- 处置 median≤0 显式拒绝或视同 NULL（除 1）；§10 的禁令补另一半。
- related L05-001、L05-004、A-04（flat median≈1 前提裁决）、M7-A-117；判定 **仍成立**；置信度 高

## M7-A-117 SCI-P3 跨 tile bilinear 未定义 NESTED 面邻接的轴翻转/镜像映射；leaf_order=+9 与 hips_tile_width∈{2^k} 值域冲突（L19-023）
- 位置 `docs/science/PHASE3_HIPS_TO_FITS.md::§2 leaf_order 行`（:28）、`::§4/§5 采样行`（:63）、`::§7 常数场不变量`、`::§9a-6`
- 问题说明 NESTED 的 tile-local 索引在面界处按面朝向存在 90°/180° 旋转或镜像；「邻域 4 像素、权重和=1」不解决朝向 ⇒ 按 local 直读在一切面界处取到天球错位的位置（非伪影级）；另 `leaf_order=tile_order+9` 只对 W=512 成立（log2 512=9），而 §4 允许任意 2 的幂，W=1024 时应 +10（同文档 §5 自己写 2·log2 W）。
- 处置 补跨面邻接映射（或把 nearest 降级为合同行为）；leaf_order 写回 `+2·log2(W)` 一般式。
- related L02-003、L15-016、L16-002、L20-021（HEALPIX_MAPPING 无公式，另条）；判定 **仍成立**；置信度 高

## M7-A-141 候选缓冲三系数（1.25 / 3.0 / 1.15）的关系未论证，「零漏选」以「极区实测最坏 1.044」代替推导（L20-006）
- 位置 `docs/algorithms/DRIZZLE_GEOMETRY.md::§/buffer 块`（复核时 :65-66、:149、:231-236，grep 1.25 四处命中）
- 问题说明 本代理独立几何复算（修正 L19 自身的推导错误后）：以 hp_res=√(π/3)/N 为**等面积平方根**、赤道菱形两全对角 D_φ=π/(2N)、D_z=4/(3N)，则赤道行中心→角点最远 = max(π/(4N), 2/(3N))，换算成 hp_res 单位 = **0.7675**（赤道）／**0.8796**（|z|=2/3 边缘，球面精确）⇒ 1.25·hp_res 对**像素外接圆**是够的（与实现注释 `spherical_overlap.cpp::HP_CIRCUMRADIUS_FACTOR` 自述的「解析上界 1.0068、实测最坏 1.043827」一致）；但 ALG 侧仍只给「实测最坏」而无**推导**，且未写 1.25 与 3.0、1.15 三者之间的大小关系要求（1.15 是赤道 delta 畸变系数还是面积放大因子，两读法都读得通）。
- 影响 三个常数中任一处被改（§10 已把 1.25 列为不可轻改项）时，无推导可依以判断新的安全边界。
- 处置 给 1.25 ≥ sup(外接半径) 的推导（或引用实现侧 scan_circumradius 为证据锚），并声明 1.15 与 3.0 的关系式。
- related L19-004（本代理改判，见 `findings/A_SCI_DEF/p2`）、L04-004、M2a（会签）、`_merge/M7.md` §几何重算；判定 **仍成立**；置信度 高


