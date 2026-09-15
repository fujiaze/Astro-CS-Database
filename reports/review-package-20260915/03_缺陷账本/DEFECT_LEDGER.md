# DEFECT_LEDGER · 缺陷账本（逐条落点）

- 生成器：`python3 run/review-package/digest-findings/_tools/extract.py` → `DEFECT_LEDGER.json`；本文件由 `render.py` 渲染。
- 条目集：`问题扫描/findings/**/*.md` 的全部 `##/###` 条目标题，抽取口径与 `问题扫描/_tools/gen_fix_ledger.py` 的 `ID_RE` 完全一致（实测重抽 = 785 条，与 `FIX_LEDGER.csv` 逐 id 一致，0 差异）。
- **判词权威**：单条事实以 `findings/<类别>/p<N>/*.md` 与 `_merge/M*.md` 处置表为准；`INDEX.md` 仅导航、`SUMMARY.md` 仅综合、`账本/FIX_LEDGER.csv` 为修复台账。
- 处置态来源与优先级见 `METHOD.md`；本表不改判、不合并、不删除任何 `id`。

## 〇 规模与处置态总览

| 处置态 | 条数 | 说明 |
|---|---:|---|
| 需负责人裁决 | 83 | 条目连到 `40_OWNER_DECISIONS.md` 的 A/B/C/D 编号，或条目正文提请负责人裁决 |
| 已修 | 29 | `FIX_LEDGER` 的 `verified_state`/`fix_state` 记为已修（含 FIXED/PARTIAL/残余），或 `_merge` 记为“已被修复” |
| 无法复现 | 2 | `_merge` 记为“无法判定（需执行/权限）”，或 `verified_state=CANNOT_REPRODUCE/CLOSED-ANCHOR-DEAD` |
| 判定非缺陷 | 4 | `verified_state=NOT_A_DEFECT/REJECT-NEVER-EXISTED/MOVED`（R 层复验结论） |
| 已登记待修 | 667 | 其余全部（`_merge` 仍成立/部分修复残余，或 V/W/SA 验证波无 `_merge` 处置行） |
| **合计** | **785** | |

> 重要：审计—合并层（`_merge`）判为“已被修复”的**叶子事实不从 findings 出条**，故 785 条定稿里绝大多数是当下仍成立/待修项；
> “已修/判定非缺陷/无法复现”主要来自 R 层复验（`FIX_LEDGER.verified_state`，71 条已填）与并发提交期修复。

## 一、需负责人裁决全清单（83 条）

| id | 类别 | P | owner | 一句问题 | _merge 态 |
|---|---|---|---|---|---|
| M1a-A-003 | A_SCI_DEF | P0 | A-07 | CAR/AIT 投影丢失 Paper II fiducial 偏移（CRVAL2 不进映射）且 Y=−θ 手性与 CD 合成北南镜像 | 仍成立 |
| M2a-A-1 | A_SCI_DEF | P0 | A-06 | SCI-DRZ-001 的 pixfrac 归一化与其自身"常数场流量守恒"不变量互斥：常数面亮度 B0 的输出为 B0/pixfrac² | 仍成立 |
| M3-A-001 | A_SCI_DEF | P0 | A-01 | SNR 无唯一定义：进入生产权重的「SNR」实为已退休的 PSF 拟合质量比 | 仍成立 |
| M3-A-002 | A_SCI_DEF | P0 | A-02 | SCI-CW 把 `weight_mode=2` 像素权重定义为 `support × snr²`，与实现、另两份冻结合同及其自身条款互斥 | 仍成立 |
| M3b-A-02 | A_SCI_DEF | P0 | — | star_det flux 列实为峰值振幅/m00 两链不同量，合同却以流量名义登记 | 仍成立 |
| M7-A-001 | A_SCI_DEF | P0 | A-20 | 「科学权重」唯一性被三处不同对象宣称，且 SCI-UPM §5 两式在实现侧被证明不等价（Σ 域裁决 = 单 control 内跨帧） | 仍成立 |
| M7-A-002 | A_SCI_DEF | P0 | — | ALG 权威层把「ivar 缺失时回退 support」写成 weight_mode=2 的唯一语义（L21-001，维持 P0） | 仍成立 |
| M1a-B-001 | B_STD_MISMATCH | P0 | A-09 | 注册表把 41×41/81×81 的 AP/BP 网格拟合登记为"7×7 网格…SCI 层已显式冻结该口径"并评severity低（M1a  | 仍成立 |
| M2b-B-07 | B_STD_MISMATCH | P0 | A-23 | 「order ≤ 29」被写成 Górski 标准条款，代码实际无 order 上界且 npix 在 2^31 处静默回绕 | 部分修复 |
| M1a-C-001 | C_DOC_CODE_GAP | P0 | A-09 | SIP 逆向冻结口径三方割裂（SCI/ALG 说 7×7、代码是 41×41 阶5 + 81×81 阶7）；SCI 冻结 1e-4 px 门 | 仍成立 |
| M1a-C-004 | C_DOC_CODE_GAP | P0 | A-08 | alpha 适用 FOV ≤ 20° 为冻结约束但全仓零强制点；三处合同对"是否硬门"互斥；超限/半球外像素静默留 NaN 仍计数发布 | 仍成立 |
| M2b-C-01 | C_DOC_CODE_GAP | P0 | A-22 | 【部分修复】HiPS 生产写出仍为直写（无 staging/rename/fsync/COMPLETE），AIO 树内其它子系统已用原子原语 | 部分修复 |
| M3-C-003 | C_DOC_CODE_GAP | P0 | A-03 | 冻结 SCI 的 `min_samples` 默认 5 与实现 64 相差 12.8 倍，ALG 径行「不改 SCI，以代码为准」= 权威层 | 仍成立 |
| M4-C-02 | C_DOC_CODE_GAP | P0 | — | SCI 冻结弱零锚 1e-3 在两条生产装配均为 0；ALG「同值生产装配」为不实陈述 | 仍成立 |
| M6a-C-001 | C_DOC_CODE_GAP | P0 | A-20 | SCI-P3 冻结的 order_sel「读层级 leaf_nside=2^(order_sel+9)」在两条通道都不参与执行：order_ | 仍成立 |
| M2a-F-1 | F_TEST_GAP | P0 | C-02 | SCI-DRZ-001 §11 的五个验证 Oracle 中三个所指测试文件从未注册进任何构建/CTest/CI 面，注册表还以其中未注册的 | 仍成立 |
| M2b-F-01 | F_TEST_GAP | P0 | A-31 | D.healpix 判 CONFORMANT 所依的「astropy-healpix 百万点独立 oracle / 往返 ≤1e-12 de | 仍成立 |
| M8-F-004 | F_TEST_GAP | P0 | A-24 | 被 STANDARDS_REGISTRY / TEST_MATRIX / TRACEABILITY 当作符合性证据的测试源从不在任何执行面出 | 仍成立 |
| M5b-G-01 | G_GOV_GATE | P0 | C-12 | CLI 命令树与协议门禁全部打在 COMPATIBILITY 二进制上；二进制缺失时门静默零检查 | 仍成立 |
| M6a-G-001 | G_GOV_GATE | P0 | C-11 | 注释卫生门 CON-COMMENTS 是空壳机器门：宪章 §12.2/§12.3-10 要求的四类判据中三类无实现，唯一有实现的判据被「冻结 | 仍成立 |
| M6b-G-002 | G_GOV_GATE | P0 | A-30 | 发布与完成状态在仓库内无单一事实源：四份 RELEASE_STATUS 并存、状态词三套、AGENTS.md 状态行被自家校验器判非法却被另 | 仍成立 |
| M7-G-001 | G_GOV_GATE | P0 | — | 「不可达验收门」家族汇总与升档判定：冻结门在自身冻结公式或 IEEE-754 下数学不可满足（第 1 类；第 2 类见 M7-G-105）， | 无逐条处置行（合并层新增/未列） |
| M8a-G-001 | G_GOV_GATE | P0 | A-31 | 依赖与许可登记面三处失真：GPL 组件进入产品链接闭包、五处登记面零覆盖、派生代码挂自著作权 MIT | 仍成立 |
| V11-N-04 | G_GOV_GATE | P0 | — | （P0）ctypes **少传第 9 个参数** `out_status` ⇒ C 侧把未初始化栈槽当指针，**既往垃圾地址写、又据其垃圾值 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V2-N-01 | G_GOV_GATE | P0 | C-09 | IpvParams 公共 C 结构体布局已改而仓内 ctypes 镜像未同步 ⇒ 72 字节越界写 + 字段全错位 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M2a-H-2 | H_NUMERIC | P0 | A-11 | variance≤0 的源像素被整颗丢弃（信号/覆盖/nContrib 一并丢失，无计数）：不确定度面被当 validity 掩膜，且该语义 | 仍成立 |
| M1a-A-009 | A_SCI_DEF | P1 | — | §30.4 对 ivar==0 给出双强制行为（NaN 传播态 vs 产品损坏显式错误），实现自行裁决其一 | 仍成立 |
| M7-A-112 | A_SCI_DEF | P1 | A-05 | NOISE_ESTIMATION 的 k_corr 查表缺维：只给 pixfrac×scale 六值，未声明其覆盖 `::§5 式` 的 N | 仍成立 |
| M7-A-120 | A_SCI_DEF | P1 | — | GLOSSARY 自称唯一术语权威，却缺宪章 §4.1 十量中 6/10 条（L21-002） | 仍成立 |
| M7-A-137 | A_SCI_DEF | P1 | A-08 | 「CAR 无投影奇点」为假（极点整行塌缩未排除）；§15.3 中 θ 在两族投影间承担两个互斥几何量（L20-023） | 仍成立 |
| V12-N-01 | A_SCI_DEF | P1 | C-09 | （P1·**总述根因条**）科学常数**无承载层**：`include/` 下无任何 constants 头，权威点数 = 0 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V12-N-06 | A_SCI_DEF | P1 | C-09 | （P1·依据虚构）6 处注释把「α=1.3→0.2885」的偏差登记指向**不存在的路径** ⇒ 「已登记故不动冻结文档」的理由不成立 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V12-N-08 | A_SCI_DEF | P1 | C-09 | （P1·**门禁判定**）§10.5 冻结门有**两套实现、判据集合不同、10 秒边界开闭相反**；`90.0` 与 `0.70` **不在 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| W2-N-03 | A_SCI_DEF | P1 | C-09 | （P1）`DATA-P1-FLUX` 端口**声明 ELECTRON 而载荷为 ADU**，而单位门因**两端同标恒不红（自我印证）** | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| W2-N-05 | A_SCI_DEF | P1 | C-09 | （P1）`photscal` **只乘像素、不乘 variance/ivar**；唯一 α² 实现**生产零调用** ⇒ 跨帧 ivar 求 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| W2-N-07 | A_SCI_DEF | P1 | C-09 | （P1）HiPS 层级父元方差归约**丢弃协方差交叉项**（方差被当作可加）⇒ 父元方差低估 2×/4× | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M1a-B-003 | B_STD_MISMATCH | P1 | A-14 | STD-F1 残余：注册表"已闭环/CONFORMANT"与同树测试"待 owner 裁决"并存，报告字符串方向与代码相反，ipv 侧像素口 | 部分修复 |
| M1a-B-005 | B_STD_MISMATCH | P1 | — | hips_frame 写死 'equatorial' 非 HiPS 1.0 枚举值，却被读取侧与 ICRS 恒等消费；SCI 冻结文本仍带未 | 仍成立 |
| V7-N-05 | C_ALG_IMPL | P1 | C-09 | （P3）`p1_snr.json` 不回显生效 SNR 科学配置，九键缺键→0.0 后**改了什么数值不可归因** | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M3-C-005 | C_DOC_CODE_GAP | P1 | B-04 | 退化 master flat 的 fail-closed 只覆盖 IR 节点通道（并发修复已落地并有回归测试），p1_session 会话通 | 部分修复 |
| M3-C-010 | C_DOC_CODE_GAP | P1 | A-05 | ALG-PHOT 声明输出含 `zero_point` 字段，接口与实现均无该字段 | 仍成立 |
| M6b-C-002 | C_DOC_CODE_GAP | P1 | — | 冻结 ALG 权威把已在代码修复并有回归门的缺陷登记为"现行缺陷语义"，逐符号锚表随之整体错位 | 仍成立 |
| M6b-C-003 | C_DOC_CODE_GAP | P1 | — | 同一 ACTIVE 文档内冻结投影集新旧两版并存：§0 非目标仍写「不实现 SIN/ZEA/CAR/AIT」，§15 已按负责人裁决冻结 T | 仍成立 |
| M7-C-101 | C_DOC_CODE_GAP | P1 | — | 产品 manifest 的溯源面只有 SCI-* 没有 ALG-*/TST-* ⇒ 算法级溯源在产品出口处断裂（L21-008） | 仍成立 |
| M8a-C-001 | C_DOC_CODE_GAP | P1 | — | 已废弃第二调度器被 README 写成「正式科学运行只有一条命令」，并整套冻结第二套 `ASTROCS_*` 退出码 | 仍成立 |
| M8a-C-002 | C_DOC_CODE_GAP | P1 | A-32 | 五源状态滞后簇：同一模块的 README / module.yaml / registry 页 / 构建安装 / 产品清单对「有没有源码、D | 仍成立 |
| M8a-C-005 | C_DOC_CODE_GAP | P1 | — | module_id 多套词汇并存：21 份 manifest 与 22 个生产 descriptor 的 module_id **精确交集为 | 仍成立 |
| M8a-C-006 | C_DOC_CODE_GAP | P1 | A-31 | 发布包 NOTICE/SBOM 由脚本内硬编码字符串生成，与实际链接闭包脱钩且含三处错标（M8a 新立） | 仍成立 |
| M9-C-3 | C_DOC_CODE_GAP | P1 | — | ipv_polygon 视场参数以角秒命名声明、生产选择器已全面改度口径字段；两函数无生产调用方，宽视场路径为 3600 族复发点（含只存在 | 无逐条处置表（M9 §1 定稿清单） |
| V1-N-04 | C_DOC_CODE_GAP | P1 | C-09 | 授权订正后的 SCI 文本一落地即与代码相反：宣称「当前实现不产出 sigma_F、不把任何标量称为科学 SNR」，而 P8 正把 `snr | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M6a-D-001 | D_COMMENT | P1 | C-05 | 生产注释承担「未登记的裁决请求」角色：SCI 冻结 1e-4 px 门被同文件注释自证一步法数学不可达；裁决与偏差 ID 只活在注释与控制包 | 仍成立 |
| V8-N-01 | D_COMMENT | P1 | C-09 | （P1·判据②锚无宿主）死码保留的"防清理凭据"是一条**不存在的测试符号** ⇒ 保留决定无凭据 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V8-N-07 | D_COMMENT | P1 | C-09 | （P1·判据③，**已挂 `A-44` 待负责人确认**）「负责人裁决 2026-09-14」被 39 行援引，其中**两族在权威登记面 0 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| L28e-E-001 | E_TRACE_BREAK | P1 | C-09 | 16 枚 TEST-UPMW/TEST-PR-UPM 被 TRACEABILITY.csv 记 VERIFIED，但 ID 在全仓真源零字面 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M6b-E-002 | E_TRACE_BREAK | P1 | C-09 | 行锚/引用锚系统性漂移的机器侧根因：锚门判据只有「可解析 + 界内 + 41 条符号绑定」，作用域仅 41 篇、语法仅带文件名锚，且 git | 仍成立 |
| FD-F-001 | F_TEST_GAP | P1 | C-09 | 「禁复制漂移」的守卫是用 `strstr` 查源文件名 —— 它测的是文本，不是行为 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M8-F-005 | F_TEST_GAP | P1 | — | 60 个 C/C++ 测试源零注册；而唯一针对注册面的门方向相反（只审「已注册目标是否登记」），此类缺口对 CI 永久隐形 | 仍成立 |
| M8-F-009 | F_TEST_GAP | P1 | C-09 | 资源门测试把 0.80 系数钉成期望值、用被测函数自造通过输入，且元测试把「注册表不得含 --gate-required」钉为期望（退不改假 | 仍成立 |
| V1-N-09 | F_TEST_GAP | P1 | C-09 | 同构 Oracle 抓不住公式错，且宣称的 NumPy oracle 脚本已失踪而证据清单仍指向它 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V15-N-16 | F_TEST_GAP | P1 | C-09 | （P1·需运行期定性）`UT-BACKEND` 的 command 只有 `unittest discover`、**无 build 依赖声 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| FD-G-001 | G_GOV_GATE | P1 | A-34 | 影子树内的 agent 指令文件与冻结宪章在「唯一入口」上正面冲突，且对全部机器门永久不可见 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M2b-G-01 | G_GOV_GATE | P1 | A-21 | HiPS 写出被建成两个独立顶层模块面（lib/hips + lib/hips_p2），与宪章 §5.2/§6.2 规定的 AIO 归属与「 | 仍成立 |
| M3-G-001 | G_GOV_GATE | P1 | B-04 | 产品 manifest 把 cosmetic 报为 `available`，但两通道给检测的 Dark/Bias 恒为 `nullptr`  | 仍成立 |
| M5b-G-08 | G_GOV_GATE | P1 | — | AGENTS-GOV 只做关键词全含：强制 AGENTS.md 保留台账验证器判为非法的状态字面量，且不校验条款号真实性 | 仍成立 |
| M9-G-6 | G_GOV_GATE | P1 | — | HiPS properties 由返回 void 的函数直写正式路径：元数据写失败静默、且无 tmp+rename（同库已有 W 系 + W | 无逐条处置表（M9 §1 定稿清单） |
| V13-N-01 | G_GOV_GATE | P1 | C-09 | MATRIX 权威侧自铸 3 枚 `EVID`，其 VERIFIED 在结构上不可被证伪 ⇒ 双视图共 5 行假 evidence | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V13-N-06 | G_GOV_GATE | P1 | C-09 | `c3452d48` 删掉的 4 行 VERIFIED 合同行**无任何对应登记动作** ⇒ 合同面与追溯面永久分叉且恒不红 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V14-N-01 | G_GOV_GATE | P1 | C-09 | Windows 产品清单把两个 SKELETON unit 登记为 IMPLEMENTED ⇒ 过度声明随安装树进入发布评审，且无任何门比对 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V14-N-02 | G_GOV_GATE | P1 | C-09 | `MOD-NOOP` 三面矛盾：两个清单都登记 IMPLEMENTED，而它自己的 README/module.yaml/CMake 三处都 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V14-N-03 | G_GOV_GATE | P1 | C-09 | 逐列 diff 两份 12 行 kernel 表：编进交付 `.so` 的那份有 **6/12 行 precision 标错**（F64 v | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V19-N-05 | G_GOV_GATE | P1 | — | （P1·即负责人裁决 A-28 的施工面）21 道门真调用 g++/gcc/nm/objdump/tar/bash/taskset，而这些名 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V2-N-09 | G_GOV_GATE | P1 | C-09 | `psf_mode` 是无条件字面量 `"fast"` ⇒ 走精确路径也标 fast，锁只查键名不查值 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V6-N-04 | G_GOV_GATE | P1 | C-09 | 冻结 SCI 文档的推导权威指向 gitignored 目录 ⇒ 干净检出无法复核任何一条新科学锚 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V6-N-10 | G_GOV_GATE | P1 | C-09 | 有修无账：账本 20 条的 fix_commit 全集与本轮 9 个真改代码的提交完全不相交 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M3-A-008 | A_SCI_DEF | P2 | A-12 | SCI-NOISE 的 MAD→σ 精度断言算术为假，同一常数三套字面值且无单一权威定义点 | 仍成立 |
| V12-N-02 | A_SCI_DEF | P2 | C-09 | （P2·**反向钉死的机器门**）`snr_constants` 把**截断串在场**固化为过门必要条件 ⇒ 按 `S-1` 统一精度反而会 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V7-N-02 | C_ALG_IMPL | P2 | C-09 | （P2）SIP 缺键→36 个系数全零但 `present=true` ⇒ 交付**声明"畸变已修"而实际未修**；判别位记声明值不记生效值 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V7-N-03 | C_ALG_IMPL | P2 | C-09 | （P2，**机制级**，本卷最值一条）typed-artifact 消费面「缺键 ⇒ 就地默认」全仓规模：兜底把**上游显式拒绝的非法值** | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V3-N-03 | C_DOC_CODE_GAP | P2 | C-09 | upm-fit 读旧版采样 artifact 缺 `control_grid_per_tile` 键时静默按 8 ⇒ 跨版本面的静默错位镜像 | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V13-N-07 | G_GOV_GATE | P2 | C-09 | 制度缺口：登记面对新编号**零承载要求** ⇒ 「新造未登记」在制度上无可违之规（附一条 SUPERSEDED 自毁） | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| V2-N-07 | H_NUMERIC | P2 | C-09 | 三处小残余（同批登记以免散落） | 无 _merge 处置表（V/W/SA/补轴 验证波） |
| M8a-I-001 | I_DOC_HYGIENE | P2 | A-33 | 「每子库都要有 README」这一负责人直接要求在本仓无任何条文支撑，35/92 目录单元因此落在规范空白里 | 仍成立 |
| M8a-I-006 | I_DOC_HYGIENE | P2 | A-32 | 「entrypoint」同词至少两义，仓内无权威定义 — 登记为术语缺口，不按猜测定档 | 仍成立 |

## 二、已修全清单（29 条，含 commit 与回归锁）

| id | 类别 | P | fix_state | verified | commit | 回归锁 | 一句问题 |
|---|---|---|---|---|---|---|---|
| M1a-C-002 | C_DOC_CODE_GAP | P0 | FIXED | FIXED | c3452d48 | ADDED:ipv_extract_wcs_sip_failclosed | trans 线性奇异：SCI 承诺"返回参数错误"，实现仅 warn 并恒设 success=true，可回写冒 |
| M2a-C-1 | C_DOC_CODE_GAP | P0 | FIXED | FIXED | dce8abd4 | ADDED:tests/unit/gaia_adapter_test.c::by_coord | DLL 通道把 out_match_idx 截断为 matched_count 个元素，破坏"坐标序 + −1  |
| M3-C-001 | C_DOC_CODE_GAP | P0 | FIXED | FIXED | c3452d48 | ADDED:p1phot_fixgates | SCI-PHOT 冻结的参考星数门（`/r_consistent/>=3`/`/r_inliers/>=2`）在 |
| M3-C-002 | C_DOC_CODE_GAP | P0 | PARTIAL | VERIFIED | c3452d48 | ADDED:p1phot_fixgates | SCI-PHOT 的饱和/质量标志判据在接口与实现中都不存在，饱和星可进零点拟合 |
| M4-C-01 | C_DOC_CODE_GAP | P0 | FIXED | FIXED | dce8abd4 | ADDED:phase2_sampler.kcorr.corner_exact | kcorr_lookup 把非均匀 pixfrac 网格当均匀插值：生产默认 0.8 列返回表外值，偏差 +1. |
| M4-C-03 | C_DOC_CODE_GAP | P0 | FIXED | FIXED | dce8abd4 | ADDED:phase2_ivar_wiring.Phase2IvarWiring.Ivar | stage2 ivar tile 读失败逐像素静默换成 support 作权重：无开关、无计数；文档「显式 fa |
| M7-C-001 | C_DOC_CODE_GAP | P0 | FIXED | PARTIAL | dce8abd4 | ADDED:phase2_synthetic_gate.Phase2Upm.ControlG | 采样器控制网格边长可配（1..64）而 UPM 把 grid=8/cell_side=64/tile_shift |
| M3-E-001 | E_TRACE_BREAK | P0 | FIXED | FIXED-PARTIAL | c3452d48 | NOT_NEEDED:同 M3b-F-02 的追溯校验器覆盖 | 校准 / 测光 / 噪声三域的追溯行与合同尾注存在行级失真：VERIFIED 行的测试锚不覆盖被验门，合同尾注用 |
| M3b-F-01 | F_TEST_GAP | P0 | PARTIAL | FIXED | c3452d48 | ADDED:p1psf_prodpath_centroid | 新失效形态：质心验收门按「非生产初始化配置」定义判据，生产路径质心无门且脚本零登记 |
| M3b-F-02 | F_TEST_GAP | P0 | FIXED | VERIFIED | c3452d48 | NOT_NEEDED:追溯锚订正由既有 tools/quality/check_tracea | SCI-PSF-001 的 VERIFIED 声明全部证据不成立：§11 承诺的 scipy/curve_fit |
| M8-F-001 | F_TEST_GAP | P0 | FIXED | FIXED | adaeb531 | ADDED:ci/validate_registry.py::R11 | `tests/abi` 四个验收脚本被 UT-ABI 以「0 用例」方式采集，该门的加载/注册/echo 三面永 |
| M8-F-002 | F_TEST_GAP | P0 | FIXED | FIXED | adaeb531 | ADDED:tests/unit/io_ownership_test.cpp::failur | `tests/unit/io_ownership_test.cpp` 在 ctest 下**不可能失败**：唯一 |
| M8-F-003 | F_TEST_GAP | P0 | FIXED | FIXED | adaeb531 | ADDED:tests/cli/test_cli_single_install.py::ci | 单入口安装树合同门（`tests/cli/test_cli_single_install.py`）在 CI 侧恒 |
| M5a-G-001 | G_GOV_GATE | P0 | FIXED | FIXED | adaeb531 | ADDED:tests/unit/p2_workers_test.cpp::case_avg | 生产利用率门沿用 §18.2 已废止的 80%/75%/50%，宪章冻结的 85%/60% 无实现 |
| M5a-G-002 | G_GOV_GATE | P0 | FIXED | FIXED | adaeb531 | ADDED:tests/unit/p2_workers_test.cpp::case_all | CPU 利用率门的分母是 1 核：85%/90% 实为 0.85/0.90 核下限，四处注释自述相反口径 |
| V2-N-08 | G_GOV_GATE | P0 | OPEN | FIXED | — | — | FAST 模式的裁决前提已被同批另一提交推翻 ⇒ `psf.max_stars` 静默决定交付的 SNR／极限星 |
| M2a-H-1 | H_NUMERIC | P0 | FIXED | FIXED | dce8abd4 | ADDED:tests/unit/drizzle_precision_default_tes | precision_mode 不参与累加域选择：核心 IR 通道恒 FP32 累加，config 要 FP64  |
| M3b-H-01 | H_NUMERIC | P0 | FIXED | VERIFIED | c3452d48 | ADDED:p1star_mad | mad 列实为 RMSE 再乘 MAD→σ 系数 1.4826 充当残差 σ，排异门等效收紧 |
| M9-H-1 | H_NUMERIC | P0 | FIXED | FIXED | 07eb229b | ADDED:gaia_module_manifest_bounds | Gaia 模块 manifest 拼接：snprintf 返回「本应写入数」推进指针 + 转义函数无容量入参 → |
| M9-H-2 | H_NUMERIC | P0 | FIXED | FIXED | 07eb229b | ADDED:xpsd_spectrum_count_bounds | XPSD 自报 spectrumCount 无任何上限即用作分配步长与每星 memcpy 长度 → 堆越界读 + |
| M2a-C-7 | C_DOC_CODE_GAP | P1 | OPEN | FIXED | — | — | 块缓存 4GB 上限在文档按全局声明、实现按每文件私有（MAX_FILES=32），资源验收断言与实现不同径 |
| FD-F-002 | F_TEST_GAP | P1 | OPEN | FIXED-PARTIAL | — | — | 同机制第二实例（由前台当场实测补入，与 FD-F-001 合为「子串断言守卫」家族） |
| M4-F-06 | F_TEST_GAP | P1 | OPEN | FIXED-PARTIAL | — | — | backend「每节点执行证据」测试断言依赖 p2_session 日志串——当前生产 IR 链不发这些日志；5 |
| FD-G-002 | G_GOV_GATE | P1 | OPEN | FIXED | — | — | 唯一执行「禁止运行产物落项目根目录」的门被永久容忍为红，且该检查自身声明 `waivable=False` |
| V2-N-10 | G_GOV_GATE | P1 | OPEN | FIXED | — | — | 极限星等迭代：不收敛为 fail-open、`capped` 靠 `fmod` 猜、`converged`/`q |
| V6-N-05 | G_GOV_GATE | P1 | OPEN | FIXED | — | — | README 自述与**同一提交**的 diff 直接矛盾：声明「未改仓库树 lib/core」，实际该提交给  |
| V5-N-03 | H_NUMERIC | P1 | FIXED | FIXED | 0e061182 | ADDED:gaia_magnitude_range_bounds | 畸形 `magnitudeRange` 经裸 `atof` 进入新剪枝谓词 ⇒ 整文件 100% 静默漏星；该字 |
| V1-N-11 | C_DOC_CODE_GAP | P2 | OPEN | FIXED | — | — | 逐源重建轮廓的串行 CPU 重算风险（§17.6/§10.5 候选，数值需实测） |
| M3-I-003 | I_DOC_HYGIENE | P2 | OPEN | FIXED-RESIDUAL | — | — | `lib/photometric_calib/docs/algorithm.md` 以「状态：已实现」呈现，且其 |

## 三、判定非缺陷 / 无法复现全清单（6 条）

| id | 处置 | 依据 | 一句问题 |
|---|---|---|---|
| M7-A-134 | 无法复现 | merge=无法判定（需执行/权限，非 CANNOT_REPRODUCE） | ALG 对同一代码给出互斥的冷/热分支符号描述（L20-012） |
| V6-N-02 | 判定非缺陷 | verified_state=MOVED（本 id 并入他条） | 给 MATRIX 的 CSV 注入 UTF-8 BOM 令整条同构校验提前 return，且只改视图未改被自己声明为权威的 JSON |
| M3-I-002 | 无法复现 | verified_state=CLOSED-ANCHOR-DEAD | `docs/science/PHASE1_API.md:8` 称 `ac_set_num_threads` 属「遗留（不在本契约）」，但该符 |
| V12-N-14 | 判定非缺陷 | verified_state=REJECT-NEVER-EXISTED | （P2）Moffat-β 先验**三套数值**：`2.5` / `2.5532` / `2.5531628` |
| M8-H-001 | 判定非缺陷 | verified_state=NOT_A_DEFECT | `gaia_client.c` 的 GaiaTraceCtx 注释声称「只原子更新本查询上下文」，实为多线程共享同一栈对象上的裸 ++ /  |
| V7-N-09 | 判定非缺陷 | verified_state=NOT_A_DEFECT | **判否负清单**（V7 逐行核为 fail-closed / 非缺陷，各轴与后续扫描**不得重报**） |

## 四、全量逐条账本（785 条，P0→P2，按类别与编号）

| id | 类别 | P | 产出方 | 处置 | _merge 态 | 账本 fix/verified | owner | 一句问题 | 证据指针 |
|---|---|---|---|---|---|---|---|---|---|
| M1a-A-001 | A_SCI_DEF | P0 | M1a | 已登记待修 | 仍成立 | OPEN | — | SCI 前向 WCS 公式把像素域 SIP 加在天球域中间坐标上，量纲不闭合，且同节自称"与实现一致" | 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md｜:§5 连续定义（"前向 WCS (像素→天球)"代码块）；对照 lib/plate_solve/cpp |
| M1a-A-002 | A_SCI_DEF | P0 | M1a | 已登记待修 | 仍成立 | OPEN | — | 天测精度门只约束内部拟合 RMS 域；仓内唯一外部闭环实测超 ALG F1 门 1.8× 且判 PASS、无偏差登记 | 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md｜:11.4 冻结门 F1；docs/modules/registry/astrocs.phase1.wc |
| M1a-A-003 | A_SCI_DEF | P0 | M1a | 需负责人裁决 | 仍成立 | OPEN | A-07 | CAR/AIT 投影丢失 Paper II fiducial 偏移（CRVAL2 不进映射）且 Y=−θ 手性与 CD 合成北南 | 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md｜:car_pix2world / ::car_world2pix / ::ait_pix2world / |
| M2a-A-1 | A_SCI_DEF | P0 | M2a | 需负责人裁决 | 仍成立 | OPEN | A-06 | SCI-DRZ-001 的 pixfrac 归一化与其自身"常数场流量守恒"不变量互斥：常数面亮度 B0 的输出为 B0/pix | 问题扫描/findings/A_SCI_DEF/p0/M2a_L04_L10.md｜:§5 球面 drizzle 算法（权重/累加式）；docs/science/DRIZZLE.md::§ |
| M2b-A-01 | A_SCI_DEF | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | HiPS 写入口对 nside 零校验：非 2 的幂与 order>29 均被接受并静默产出错产品；lib/hips 的正确校验 | 问题扫描/findings/A_SCI_DEF/p0/M2b_L03_L15.md｜lib/astro_image_io/src/hips/aio_hips_writer.cpp::aio |
| M2b-A-02 | A_SCI_DEF | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | tile 几何合同三源互斥：层级 tile NSIDE 卡（代码 2^(k+9) vs ALG 2^k vs IO_002 2^ | 问题扫描/findings/A_SCI_DEF/p0/M2b_L03_L15.md｜lib/astro_image_io/src/hips/aio_hips_writer.cpp::fin |
| M3-A-001 | A_SCI_DEF | P0 | M3 | 需负责人裁决 | 仍成立 | OPEN/FIXED | A-01 | SNR 无唯一定义：进入生产权重的「SNR」实为已退休的 PSF 拟合质量比 | 问题扫描/findings/A_SCI_DEF/p0/M3_L05_L07.md｜:§9a（SNR 构成条款）`（复核时点 :98） ；`lib/snr_estimator/cpp/sr |
| M3-A-002 | A_SCI_DEF | P0 | M3 | 需负责人裁决 | 仍成立 | OPEN | A-02 | SCI-CW 把 `weight_mode=2` 像素权重定义为 `support × snr²`，与实现、另两份冻结合同及其自 | 问题扫描/findings/A_SCI_DEF/p0/M3_L05_L07.md｜:§4 连续定义（代码块首行注释与权重式）`（复核时点 :38、:42；符号表 :27 `w_snr`） |
| M3b-A-01 | A_SCI_DEF | P0 | M3b | 已登记待修 | 仍成立 | OPEN | — | 生产检测拟合内核实为椭圆高斯，SCI 冻结基线却写 Moffat4 且禁止高斯主路径 | 问题扫描/findings/A_SCI_DEF/p0/M3b_L06.md｜:§1(:12-14)/§3(:41-42)；docs/algorithms/STAR_DETECTIO |
| M3b-A-02 | A_SCI_DEF | P0 | M3b | 需负责人裁决 | 仍成立 | OPEN | — | star_det flux 列实为峰值振幅/m00 两链不同量，合同却以流量名义登记 | 问题扫描/findings/A_SCI_DEF/p0/M3b_L06.md｜:StarRecord.flux(:238)/::rec.flux(:2268)/::旧路径(:1501 |
| M3b-A-03 | A_SCI_DEF | P0 | M3b | 已登记待修 | 仍成立 | OPEN | — | 每像元截尾残差被合同命名为 flux_uncertainty，DISP-PSF-005 自认无协方差 | 问题扫描/findings/A_SCI_DEF/p0/M3b_L06.md｜:star_measurements 装配(:2470,:2493-2495,:2506,:2512,: |
| M4-A-01 | A_SCI_DEF | P0 | M4 | 已登记待修 | 仍成立 | OPEN | — | N≤4 全拒像素被静默全接受为 UNDERDETERMINED，FROZEN SCI 未订正且零测试 | 问题扫描/findings/A_SCI_DEF/p0/M4_L08_L09.md｜:「容错：小栈全拒回退为 UNDERDETERMINED」分支（函数 p2_reject_stack_e |
| M4-A-02 | A_SCI_DEF | P0 | M4 | 已登记待修 | 部分修复 | OPEN | — | 冻结 SCI 内 support reducer 两口径互斥：代码取 accepted 口径并有门，§5 公式未订正、旧 ALG | 问题扫描/findings/A_SCI_DEF/p0/M4_L08_L09.md｜58（vs :21,63,75）；lib/phase2/src/integrate.cpp::「B2-A |
| M7-A-001 | A_SCI_DEF | P0 | M7 | 需负责人裁决 | 仍成立 | OPEN | A-20 | 「科学权重」唯一性被三处不同对象宣称，且 SCI-UPM §5 两式在实现侧被证明不等价（Σ 域裁决 = 单 control 内 | 问题扫描/findings/A_SCI_DEF/p0/M7_A_SCI_DEF_p0.m｜:§5 科学权重块`（复核时 :46 三因子积式 vs :47 raw/normalized 两行）；` |
| M7-A-002 | A_SCI_DEF | P0 | M7 | 需负责人裁决 | 仍成立 | OPEN | — | ALG 权威层把「ivar 缺失时回退 support」写成 weight_mode=2 的唯一语义（L21-001，维持 P0 | 问题扫描/findings/A_SCI_DEF/p0/M7_A_SCI_DEF_p0.m｜:§/weight_mode 块`（本代理复核时该篇在位；L21 记「复核时 :」以符号锚为准）；对照  |
| M1a-B-001 | B_STD_MISMATCH | P0 | M1a | 需负责人裁决 | 仍成立 | OPEN | A-09 | 注册表把 41×41/81×81 的 AP/BP 网格拟合登记为"7×7 网格…SCI 层已显式冻结该口径"并评severity | 问题扫描/findings/B_STD_MISMATCH/p0/M1a_L01_L02.｜:SIP 行（:66）与::偏差登记表（:76、:241）；对照 lib/plate_solve/cpp |
| M1a-B-002 | B_STD_MISMATCH | P0 | M1a | 已登记待修 | 仍成立 | OPEN | — | Paper II 相关三行注册表判 CONFORMANT/偏差"无"，与 CAR/AIT 实质偏离及"仅 TAN 受控"现状互斥 | 问题扫描/findings/B_STD_MISMATCH/p0/M1a_L01_L02.｜:D.spherical 表（:62、:64、:65）+::偏差登记（:76）+::§（:241）；对照 |
| M2a-B-1 | B_STD_MISMATCH | P0 | M2a | 已登记待修 | 仍成立 | OPEN | — | 星表位置历元：注册表条款面要求 J2016.0，产品面四处声称 J2000，模块零历元处理且无天测列可传播——CONFORMAN | 问题扫描/findings/B_STD_MISMATCH/p0/M2a_L04_L10.｜:D.catalog（CLAUSES 行 + 判定式首行）；docs/science/ASTROMETR |
| M2b-B-01 | B_STD_MISMATCH | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | HiPS tile 目录名写「商」、文件名写「余数」，与 IVOA §4.1 算式相反 | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.｜:tile_rel_path；lib/astro_image_io/src/hips/aio_hips_ |
| M2b-B-02 | B_STD_MISMATCH | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | Moc.fits 缺 MOC Table 3 强制键 ORDERING=NUNIQ / COORDSYS=C，注册表仍判 CON | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.｜:write_moc_fits；docs/standards/STANDARDS_REGISTRY.md |
| M2b-B-03 | B_STD_MISMATCH | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | hips_pixel_scale 写角秒，标准规定单位为度（相差 3600 倍） | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.｜:finalize_image_product；docs/algorithms/HIPS_WRITER. |
| M2b-B-04 | B_STD_MISMATCH | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | HiPS 域条款锚整体错挂；properties 必需键集用自定 5 键替代标准 9 键；STD-F4 引用不存在的 obs_b | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.｜:D.hips CLAUSES + 清单行 1/3/5/6 + 偏差表 STD-F4；docs/inte |
| M2b-B-07 | B_STD_MISMATCH | P0 | M2b | 需负责人裁决 | 部分修复 | OPEN | A-23 | 「order ≤ 29」被写成 Górski 标准条款，代码实际无 order 上界且 npix 在 2^31 处静默回绕 | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.｜:require_valid_nside；lib/common/healpix/healpix_core |
| M2b-B-09 | B_STD_MISMATCH | P0 | M2b | 已登记待修 | 仍成立 | OPEN | — | 交付 FITS 可恒带 CHECKSUM 全零占位卡（注释谎称「HDU checksum updated」），且 verify  | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.｜:fio_write_header_block（恒预留 CHECKSUM 槽并写全零值）、::acs_f |
| M9-B-1 | B_STD_MISMATCH | P0 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | SIP 前向 A/B/AP/BP 系数以「像素域」数值直写 FITS 标准键 A_i_j/B_i_j；同一数组在合同面有四处互斥 | 问题扫描/findings/B_STD_MISMATCH/p0/M9_L24_L26.m｜:extract_wcs_sip（系数生成与单位注释）、lib/core/src/module_adap |
| M1a-C-001 | C_DOC_CODE_GAP | P0 | M1a | 需负责人裁决 | 仍成立 | OPEN | A-09 | SIP 逆向冻结口径三方割裂（SCI/ALG 说 7×7、代码是 41×41 阶5 + 81×81 阶7）；SCI 冻结 1e- | 问题扫描/findings/C_DOC_CODE_GAP/p0/M1a_L01_L02.｜:§5（自述）与::§7 不变量与::§9 实测口径、::§11 验收门；docs/algorithms |
| M1a-C-002 | C_DOC_CODE_GAP | P0 | M1a | 已修 | 仍成立 | FIXED/FIXED | — | trans 线性奇异：SCI 承诺"返回参数错误"，实现仅 warn 并恒设 success=true，可回写冒称 TAN-SI | 问题扫描/findings/C_DOC_CODE_GAP/p0/M1a_L01_L02.｜:§8 退化与错误语义表（奇异 trans 行）；docs/algorithms/PLATESOLVE. |
| M1a-C-003 | C_DOC_CODE_GAP | P0 | M1a | 已登记待修 | 仍成立 | OPEN | — | p1_op_wcs 自检以 0-based 数组下标喂 1-based 契约的 WcsTan；两条门与"独立参考解"共用同一原点 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M1a_L01_L02.｜:p1_op_wcs（显式配置分支自检环 + 求解分支自检环）与::p1_tan_forward_ref |
| M1a-C-004 | C_DOC_CODE_GAP | P0 | M1a | 需负责人裁决 | 仍成立 | OPEN | A-08 | alpha 适用 FOV ≤ 20° 为冻结约束但全仓零强制点；三处合同对"是否硬门"互斥；超限/半球外像素静默留 NaN 仍计 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M1a_L01_L02.｜:§9a-12 与::§15；docs/standards/STANDARDS_REGISTRY.md: |
| M2a-C-1 | C_DOC_CODE_GAP | P0 | M2a | 已修 | 仍成立 | FIXED/FIXED | — | DLL 通道把 out_match_idx 截断为 matched_count 个元素，破坏"坐标序 + −1 表示未匹配"合同 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M2a_L04_L10.｜:§8.2 输出行 out_match_idx 行 + shape 契约段；lib/gaia_xpsd_ |
| M2b-C-01 | C_DOC_CODE_GAP | P0 | M2b | 需负责人裁决 | 部分修复 | OPEN | A-22 | 【部分修复】HiPS 生产写出仍为直写（无 staging/rename/fsync/COMPLETE），AIO 树内其它子系统 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M2b_L03_L15.｜:write_fits_image、::write_moc_fits、::make_dirs、::fin |
| M3-C-001 | C_DOC_CODE_GAP | P0 | M3 | 已修 | 仍成立 | FIXED/FIXED | — | SCI-PHOT 冻结的参考星数门（`/r_consistent/>=3`/`/r_inliers/>=2`）在实现中不存在：一 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M3_L05_L07.m｜:§4 输入有效域（参考星数条）`（复核时点 :38）、`§8 极端/退化条件表`（:82）、`§5 定 |
| M3-C-002 | C_DOC_CODE_GAP | P0 | M3 | 已修 | 仍成立 | PARTIAL/VERIFIED | — | SCI-PHOT 的饱和/质量标志判据在接口与实现中都不存在，饱和星可进零点拟合 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M3_L05_L07.m｜:§4 输入有效域（饱和星条）`（复核时点 :37）、`§6 假设`（:69）、`§10 不可接受变化` |
| M3-C-003 | C_DOC_CODE_GAP | P0 | M3 | 需负责人裁决 | 仍成立 | OPEN | A-03 | 冻结 SCI 的 `min_samples` 默认 5 与实现 64 相差 12.8 倍，ALG 径行「不改 SCI，以代码为准 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M3_L05_L07.m｜:§4 输入有效域`（复核时点 :37）、`§5 连续定义`（:48） ；`lib/snr_estima |
| M3b-C-01 | C_DOC_CODE_GAP | P0 | M3b | 已登记待修 | 仍成立 | OPEN | — | 生产 PSF 质心 ~0.5px 系统性偏差：已知未闭合的 BLOCKER 只活在归档层，规范层反宣称「无 0.5px 量化损失 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M3b_L06.md｜docs/archive/history/memory_V18R2-V19_operational_lo |
| M3b-C-02 | C_DOC_CODE_GAP | P0 | M3b | 已登记待修 | 仍成立 | OPEN | — | 注册节点 star-psf 实装第三套未登记检测器；「唯一生产源」三处宣称被证伪（第三次同模式） | 问题扫描/findings/C_DOC_CODE_GAP/p0/M3b_L06.md｜docs/contracts/DATA_SEMANTICS.md::§17(:644-646「现行实现唯 |
| M4-C-01 | C_DOC_CODE_GAP | P0 | M4 | 已修 | 仍成立 | FIXED/FIXED | — | kcorr_lookup 把非均匀 pixfrac 网格当均匀插值：生产默认 0.8 列返回表外值，偏差 +1.5%/+2.1% | 问题扫描/findings/C_DOC_CODE_GAP/p0/M4_L08_L09.m｜:kcorr_lookup（复核时点 :88-109；冻结表值 :91-94；k_corr 冻结注释 : |
| M4-C-02 | C_DOC_CODE_GAP | P0 | M4 | 需负责人裁决 | 仍成立 | FIXED/FIXED | — | SCI 冻结弱零锚 1e-3 在两条生产装配均为 0；ALG「同值生产装配」为不实陈述 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M4_L08_L09.m｜:「P2UpmBuildConfig uc{}」装配段（复核时点 :183-202）；lib/core/ |
| M4-C-03 | C_DOC_CODE_GAP | P0 | M4 | 已修 | 仍成立 | FIXED/FIXED | — | stage2 ivar tile 读失败逐像素静默换成 support 作权重：无开关、无计数；文档「显式 fallback 并 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M4_L08_L09.m｜:并行权重构造（复核时点 :1106-1116，回退行 :1115）、串行同构（复核时点 :1359-1 |
| M5b-C-01 | C_DOC_CODE_GAP | P0 | M5b | 已登记待修 | 仍成立 | OPEN | — | API 合同表 422 行全部标 VERIFIED、396 行共用未定义占位 test_ids，门只比 4 列且自述"真实校验由 | 问题扫描/findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.｜:（表头 + 全部记录）`（现 :1-423；样本行 :5）；`tools/quality/contra |
| M5b-C-02 | C_DOC_CODE_GAP | P0 | M5b | 已登记待修 | 仍成立 | OPEN | — | 两平台交付同一 manifest：Windows 侧把 Linux 侧标 SKELETON 的两个平台库标为 IMPLEMENT | 问题扫描/findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.｜:units[PLATFORM-RUNTIME/PLATFORM-IO]`（现 :9-10）；`pack |
| M6a-C-001 | C_DOC_CODE_GAP | P0 | M6a | 需负责人裁决 | 仍成立 | OPEN | A-20 | SCI-P3 冻结的 order_sel「读层级 leaf_nside=2^(order_sel+9)」在两条通道都不参与执行： | 问题扫描/findings/C_DOC_CODE_GAP/p0/M6a_L13_L14.｜:p3_session_run`（复核时点 :196-199 order_sel 计算、:358 ord |
| M7-C-001 | C_DOC_CODE_GAP | P0 | M7 | 已修 | 仍成立 | FIXED/PARTIAL | — | 采样器控制网格边长可配（1..64）而 UPM 把 grid=8/cell_side=64/tile_shift=9 编成常数， | 问题扫描/findings/C_DOC_CODE_GAP/p0/M7_C_DOC_COD |
| M3-E-001 | E_TRACE_BREAK | P0 | M3 | 已修 | 仍成立 | FIXED/FIXED-PARTIAL | — | 校准 / 测光 / 噪声三域的追溯行与合同尾注存在行级失真：VERIFIED 行的测试锚不覆盖被验门，合同尾注用非法测试 ID | 问题扫描/findings/E_TRACE_BREAK/p0/M3_L05_L07.md｜:第 20 行（requirement_id=SCI-CAL-001）` —— 逐列读取结果：`titl |
| M6b-E-001 | E_TRACE_BREAK | P0 | M6b | 已登记待修 | 仍成立 | OPEN/STILL | — | 追溯矩阵双头：权威明文是 SPEC JSON，但全部活动路由面（README-DOCS / DEVELOPER_GUIDE /  | 问题扫描/findings/E_TRACE_BREAK/p0/M6b_L16_L18.m｜:§1 事实源声明`（现 :15-16） ；`docs/README-DOCS.md::L0-L5 表  |
| FD-F-003 | F_TEST_GAP | P0 | FD | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | 唯一跑 Windows C++ 单测的门连续 10 次"0 用例 PASS"，且空输出被 `EMPTY_OUTPUT_SILEN | 问题扫描/findings/F_TEST_GAP/p0/FD_windows_zero_｜:stage_plan:344-356`（缺"用例数>0"断言）；`ci/run.py:103-105  |
| M1a-F-001 | F_TEST_GAP | P0 | M1a | 已登记待修 | 仍成立 | OPEN | — | CAR/AIT"独立 Oracle"与被测实现同源（解析式取自同一推导），错误口径被测试钉死为正确 | 问题扫描/findings/F_TEST_GAP/p0/M1a_L01_L02.md｜:CAR/AIT oracle 段；tests/backend/test_p3_projection_o |
| M2a-F-1 | F_TEST_GAP | P0 | M2a | 需负责人裁决 | 仍成立 | OPEN | C-02 | SCI-DRZ-001 §11 的五个验证 Oracle 中三个所指测试文件从未注册进任何构建/CTest/CI 面，注册表还以 | 问题扫描/findings/F_TEST_GAP/p0/M2a_L04_L10.md｜:§11 验证 Oracle、::§13 追溯与测试、::§15；docs/standards/STAN |
| M2b-F-01 | F_TEST_GAP | P0 | M2b | 需负责人裁决 | 仍成立 | OPEN/STILL | A-31 | D.healpix 判 CONFORMANT 所依的「astropy-healpix 百万点独立 oracle / 往返 ≤1e | 问题扫描/findings/F_TEST_GAP/p0/M2b_L03_L15.md｜:D.healpix COMPLIANCE 字段 + 清单行 1/2/3（§5.1/§5.2/§5.3  |
| M3-F-001 | F_TEST_GAP | P0 | M3 | 已登记待修 | 仍成立 | OPEN | — | SCI-NOISE §11/§15 承诺的「NumPy rtol 1e-9 复算」在仓库内不存在，唯一 Python 对拍面是容 | 问题扫描/findings/F_TEST_GAP/p0/M3_L05_L07.md｜:§11 验证 Oracle / §15 Acceptance`（复核时点 :117、:140） ；`t |
| M3b-F-01 | F_TEST_GAP | P0 | M3b | 已修 | 仍成立 | PARTIAL/FIXED | — | 新失效形态：质心验收门按「非生产初始化配置」定义判据，生产路径质心无门且脚本零登记 | 问题扫描/findings/F_TEST_GAP/p0/M3b_L06.md｜:冻结门(:7-8)/::生产路径 offset(:248-252)/::converged 对照(:2 |
| M3b-F-02 | F_TEST_GAP | P0 | M3b | 已修 | 部分修复 | FIXED/VERIFIED | — | SCI-PSF-001 的 VERIFIED 声明全部证据不成立：§11 承诺的 scipy/curve_fit 复算不存在、追 | 问题扫描/findings/F_TEST_GAP/p0/M3b_L06.md｜:§11(:96-101)/§13(:108-113)/§15(:121-126)；docs/TRACE |
| M3b-F-03 | F_TEST_GAP | P0 | M3b | 已登记待修 | 仍成立 | OPEN | — | 冻结验收项在测试面零覆盖：SNR 谱段缺口按测试自身常数可判、空值守卫、永真虚警门（升 P0） | 问题扫描/findings/F_TEST_GAP/p0/M3b_L06.md｜:F1(:40-68)/::F2(:120-151)；lib/star_detector/tests/p |
| M4-F-01 | F_TEST_GAP | P0 | M4 | 已登记待修 | 仍成立 | OPEN | — | UPM 硬门/持久化门/MC 标定门全部不挂产品构建，SCI「Oracle 全过」无运行时证据 | 问题扫描/findings/F_TEST_GAP/p0/M4_L08_L09.md｜1-9（BLD-002 构建拓扑契约，:6-8 compatibility 降级句）、:389-397（ |
| M4-F-02 | F_TEST_GAP | P0 | M4 | 已登记待修 | 仍成立 | OPEN | — | SCI 声称的 NumPy 独立 Huber-IRLS Oracle 真源不存在，「Python 参考 rtol 1e-9」为空 | 问题扫描/findings/F_TEST_GAP/p0/M4_L08_L09.md｜108,147；检索面=tests/**（grep huber/IRLS/k_corr/numpy）、l |
| M8-F-001 | F_TEST_GAP | P0 | M8 | 已修 | 仍成立 | FIXED/FIXED | — | `tests/abi` 四个验收脚本被 UT-ABI 以「0 用例」方式采集，该门的加载/注册/echo 三面永不失败；文档仍以 | 问题扫描/findings/F_TEST_GAP/p0/M8_L22_L23.md｜:UT-ABI.command`（复核时点 :1642-1671，`waivable=false`、pr |
| M8-F-002 | F_TEST_GAP | P0 | M8 | 已修 | 仍成立 | FIXED/FIXED | — | `tests/unit/io_ownership_test.cpp` 在 ctest 下**不可能失败**：唯一 CHECK 的 | 问题扫描/findings/F_TEST_GAP/p0/M8_L22_L23.md｜:main`（全文件仅 32 行；`::failures` :9、`::CHECK` 宏定义 :10-1 |
| M8-F-003 | F_TEST_GAP | P0 | M8 | 已修 | 仍成立 | FIXED/FIXED | — | 单入口安装树合同门（`tests/cli/test_cli_single_install.py`）在 CI 侧恒 SKIP：前置 | 问题扫描/findings/F_TEST_GAP/p0/M8_L22_L23.md｜:TestCliSingleInstall.setUpClass`（复核时点 :16 skipUnles |
| M8-F-004 | F_TEST_GAP | P0 | M8 | 需负责人裁决 | 仍成立 | OPEN | A-24 | 被 STANDARDS_REGISTRY / TEST_MATRIX / TRACEABILITY 当作符合性证据的测试源从不在 | 问题扫描/findings/F_TEST_GAP/p0/M8_L22_L23.md｜`docs/standards/checks/check_standards_registry.py:: |
| M9-F-1 | F_TEST_GAP | P0 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | 本域八个恶意输入回归 TU 从不进构建：它们恰是全部 M9 边界缺陷的唯一复现件，却被 docs 当符合性证据引用 | 问题扫描/findings/F_TEST_GAP/p0/M9_L24_L26.md｜:p1_io_hardening_test、::hips_publish_atomic_test；引用面 |
| M3b-G-01 | G_GOV_GATE | P0 | M3b | 已登记待修 | 仍成立 | OPEN | — | 未闭合 BLOCKER（PSF-001 ~0.5px）只活在归档文档：规范限制层零提及、偏差登记层零条目、门禁层零登记 | 问题扫描/findings/G_GOV_GATE/p0/M3b_L06.md｜1/:2038-2040/:2051；docs/KNOWN_LIMITATIONS.md（PSF/质心/ |
| M5a-G-001 | G_GOV_GATE | P0 | M5a | 已修 | 仍成立 | FIXED/FIXED | — | 生产利用率门沿用 §18.2 已废止的 80%/75%/50%，宪章冻结的 85%/60% 无实现 | 问题扫描/findings/G_GOV_GATE/p0/M5a_L11_L17.md｜:compute_cores_threshold`（现 :181-186，0.80 系数在 :185）  |
| M5a-G-002 | G_GOV_GATE | P0 | M5a | 已修 | 仍成立 | FIXED/FIXED | — | CPU 利用率门的分母是 1 核：85%/90% 实为 0.85/0.90 核下限，四处注释自述相反口径 | 问题扫描/findings/G_GOV_GATE/p0/M5a_L11_L17.md｜:ResourceRecorder::record`（:101 采集式）与文件头口径声明（:5、`Res |
| M5a-G-003 | G_GOV_GATE | P0 | M5a | 已登记待修 | 仍成立 | OPEN | — | 唯一逐字实现 §10.5 的门禁零应用点，L0 口径却称「缺失即 fail-closed」 | 问题扫描/findings/G_GOV_GATE/p0/M5a_L11_L17.md｜:evaluate_frozen_gate`（常量 :451-456；函数 :459-602；CLI 旗 |
| M5a-G-005 | G_GOV_GATE | P0 | M5a | 已登记待修 | 仍成立 | OPEN | — | THREAD-BUDGET 机器门空转：扫描面、正则与行级豁免三重漏检并被单测固化 | 问题扫描/findings/G_GOV_GATE/p0/M5a_L11_L17.md｜:SCAN_ROOTS`（:6）、`::EXEMPT`（:7-11）、`::PATTERNS`（:69- |
| M5b-G-01 | G_GOV_GATE | P0 | M5b | 需负责人裁决 | 仍成立 | OPEN | C-12 | CLI 命令树与协议门禁全部打在 COMPATIBILITY 二进制上；二进制缺失时门静默零检查 | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md｜:（cli 子图构建步）`（现 :21-25）——`cmake -S cli -B build/cli` |
| M5b-G-02 | G_GOV_GATE | P0 | M5b | 已登记待修 | 仍成立 | OPEN | — | 追溯门 fail-open：`tools/quality/check_traceability.py` 无条件 return 0 | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md｜:main`（现 :142-159，`return 0` 在 :159） ；`tools/check_t |
| M5b-G-03 | G_GOV_GATE | P0 | M5b | 已登记待修 | 仍成立 | OPEN | — | AST-API 门为同一头文件自反比对（恒真），参数数提取后弃用，且不读任何 API 文档 | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md｜:main`（现 :40-68）；`ci/checks.json::AST-API`（现 :577-59 |
| M5b-G-04 | G_GOV_GATE | P0 | M5b | 已登记待修 | 仍成立 | OPEN | — | 版本单源簇：交付清单/安装树合同/依赖锁/schema 并存 4 份手工版本副本，两道版本门的扫描面都不覆盖它们 | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md｜:product_version`（现 :3，0.11.0-alpha.1） ；`packaging/i |
| M5b-G-05 | G_GOV_GATE | P0 | M5b | 已登记待修 | 仍成立 | OPEN | — | CON-BUILD-GRAPH 只做子串存在性检查：订正 BUILD_GRAPH.md 反而变红，产品构建面无任何机器校验 | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md｜:main`（现 :18-46）；`docs/architecture/BUILD_GRAPH.md:: |
| M5b-G-06 | G_GOV_GATE | P0 | M5b | 已登记待修 | 仍成立 | OPEN/STILL-SUBITEM-EXPIRED | — | §18.4「只加载随产品签名清单发布的官方模块」在机器上无实现：产品不加载、hash 全 null、唯一闭环测试不进 CI | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md｜:target_link_libraries(astrocs ...)`（现 :605-609，静态链全 |
| M6a-G-001 | G_GOV_GATE | P0 | M6a | 需负责人裁决 | 仍成立 | OPEN | C-11 | 注释卫生门 CON-COMMENTS 是空壳机器门：宪章 §12.2/§12.3-10 要求的四类判据中三类无实现，唯一有实现的 | 问题扫描/findings/G_GOV_GATE/p0/M6a_L13_L14.md｜:main`（复核时点 :9-13 STALE_PATTERNS、:15 REQUIRE_ID_NEAR |
| M6b-G-001 | G_GOV_GATE | P0 | M6b | 已登记待修 | 仍成立 | OPEN/STILL-SUBITEM-EXPIRED | — | 未经证据支撑的「已验证」声明（单一根因，四组实例）：八层矩阵在 TEST / EVIDENCE / SRC / 整行四个层面给出 | 问题扫描/findings/G_GOV_GATE/p0/M6b_L16_L18.md｜:_check_refs`（现 :258-274，:262-265 `if code in ("MOD" |
| M6b-G-002 | G_GOV_GATE | P0 | M6b | 需负责人裁决 | 仍成立 | OPEN | A-30 | 发布与完成状态在仓库内无单一事实源：四份 RELEASE_STATUS 并存、状态词三套、AGENTS.md 状态行被自家校验器 | 问题扫描/findings/G_GOV_GATE/p0/M6b_L16_L18.md｜:§0 状态词阶梯 + §3`（现 :18-27、:70-:71、:82-84） ；`docs/RELE |
| M6b-G-003 | G_GOV_GATE | P0 | M6b | 已登记待修 | 仍成立 | OPEN | — | 「本版实测」类无据证据：ACTIVE 文档以完成时宣称机器门 PASS 9/9 与 mismatches=[]，既无 SHA/命 | 问题扫描/findings/G_GOV_GATE/p0/M6b_L16_L18.md｜:§7 Machine Consistency (S8 gate, 本版实测)`（现 :114-125） |
| M7-G-001 | G_GOV_GATE | P0 | M7 | 需负责人裁决 | 无逐条处置行（合并层新增/未列） | OPEN | — | 「不可达验收门」家族汇总与升档判定：冻结门在自身冻结公式或 IEEE-754 下数学不可满足（第 1 类；第 2 类见 M7-G | 问题扫描/findings/G_GOV_GATE/p0/M7_G_GOV_GATE_p0 |
| M8a-G-001 | G_GOV_GATE | P0 | M8a | 需负责人裁决 | 仍成立 | OPEN | A-31 | 依赖与许可登记面三处失真：GPL 组件进入产品链接闭包、五处登记面零覆盖、派生代码挂自著作权 MIT | 问题扫描/findings/G_GOV_GATE/p0/M8a_L25_L27.md｜:astrocs_p1_sdet`（复核 535-546）→ `::astrocs_module_ada |
| V11-N-01 | G_GOV_GATE | P0 | V11 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `AioHipsSnrPoint` 镜像落后 C 头两字段 ⇒ 越界读 24 字节，且**交付 HiPS SNR 目录第 2/3 | 问题扫描/findings/G_GOV_GATE/p0/V11.md｜84-92` = **6 字段**（`ra_deg`/`dec_deg`/`snr`/`star_id` |
| V11-N-04 | G_GOV_GATE | P0 | V11-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | （P0）ctypes **少传第 9 个参数** `out_status` ⇒ C 侧把未初始化栈槽当指针，**既往垃圾地址写、 | 问题扫描/findings/G_GOV_GATE/p0/V11-b.md |
| V11-N-05 | G_GOV_GATE | P0 | V11-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P0，结构性）ABI 协商与自检的 6 个合同函数**生产面零实现、零调用**，唯一实现住在**测试自己的探针副本**里 ⇒  | 问题扫描/findings/G_GOV_GATE/p0/V11-b.md |
| V13-N-03 | G_GOV_GATE | P0 | V13 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 审计状态铸造面：713/713 行机械铸 `VERIFIED` 并铸 `findings_p0..3=0`、7 个 `*_ok= | 问题扫描/findings/G_GOV_GATE/p0/V13.md |
| V18-N-12 | G_GOV_GATE | P0 | V18 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P0·**生产者与守卫同处的最纯形态**）`tests/backend/test_isa_avx.py`：自己写 CSV（de | 问题扫描/findings/G_GOV_GATE/p0/V18.md |
| V19-N-01 | G_GOV_GATE | P0 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | （P0·**新机制：不是漏登记，是登记不了**）CI-REG-002 闭包面只认 `add_test(NAME …)` ⇒ ** | 问题扫描/findings/G_GOV_GATE/p0/V19.md |
| V2-N-01 | G_GOV_GATE | P0 | V2 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | FIXED/FIXED | C-09 | IpvParams 公共 C 结构体布局已改而仓内 ctypes 镜像未同步 ⇒ 72 字节越界写 + 字段全错位 | 问题扫描/findings/G_GOV_GATE/p0/V2.md｜61-80`（ctypes 镜像，`:73` 仍含已删的 `m_lim_step`）→ `:192` 调 |
| V2-N-08 | G_GOV_GATE | P0 | V2-D2 | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED | — | FAST 模式的裁决前提已被同批另一提交推翻 ⇒ `psf.max_stars` 静默决定交付的 SNR／极限星等，且无任何 p | 问题扫描/findings/G_GOV_GATE/p0/V2-D2.md｜1761-1765`（`psf.max_stars` 默认 **5000**）与 `:1772` `co |
| V9-N-07 | G_GOV_GATE | P0 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P0·机制①最纯形态）**5 道非豁免门在 CI 里只跑 `--selftest`**：真实判定模式需要交付物参数，而 CI  | 问题扫描/findings/G_GOV_GATE/p0/V9-b.md |
| V9-N-16 | G_GOV_GATE | P0 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P0·机制①最纯 + 机制③合成）`TRACEABILITY` 门**末行无条件 `return 0`**，且登记 `muta | 问题扫描/findings/G_GOV_GATE/p0/V9-c.md |
| V9-N-17 | G_GOV_GATE | P0 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P0·**本时段最需要隔壁立刻看到的一条**）新建门 `CTEST-P1DRZ-TASKSET-INVARIANCE` 的被测 | 问题扫描/findings/G_GOV_GATE/p0/V9-c.md |
| M2a-H-1 | H_NUMERIC | P0 | M2a | 已修 | 部分修复 | FIXED/FIXED | — | precision_mode 不参与累加域选择：核心 IR 通道恒 FP32 累加，config 要 FP64 也拿不到 flo | 问题扫描/findings/H_NUMERIC/p0/M2a_L04_L10.md｜:run_drizzle_internal（use_f64 选路段）、::hp_drizzle_run（ |
| M2a-H-2 | H_NUMERIC | P0 | M2a | 需负责人裁决 | 仍成立 | OPEN | A-11 | variance≤0 的源像素被整颗丢弃（信号/覆盖/nContrib 一并丢失，无计数）：不确定度面被当 validity 掩 | 问题扫描/findings/H_NUMERIC/p0/M2a_L04_L10.md｜:drizzleTiled 主循环（variance/weight ≤0 continue 段）；::p |
| M3b-H-01 | H_NUMERIC | P0 | M3b | 已修 | 仍成立 | FIXED/VERIFIED | — | mad 列实为 RMSE 再乘 MAD→σ 系数 1.4826 充当残差 σ，排异门等效收紧 | 问题扫描/findings/H_NUMERIC/p0/M3b_L06.md｜:sdet_lm_fit(:415)/::reject_star(:202-206)/::rmse 累积 |
| M9-H-1 | H_NUMERIC | P0 | M9 | 已修 | 无逐条处置表（M9 §1 定稿清单） | FIXED/FIXED | — | Gaia 模块 manifest 拼接：snprintf 返回「本应写入数」推进指针 + 转义函数无容量入参 → 512 字节栈 | 问题扫描/findings/H_NUMERIC/p0/M9_L24_L26.md｜:gaia_execute（head 缓冲与推进点）、::gaia_inspect（同型第二站点）、:: |
| M9-H-2 | H_NUMERIC | P0 | M9 | 已修 | 无逐条处置表（M9 §1 定稿清单） | FIXED/FIXED | — | XPSD 自报 spectrumCount 无任何上限即用作分配步长与每星 memcpy 长度 → 堆越界读 + 数百 MB 级 | 问题扫描/findings/H_NUMERIC/p0/M9_L24_L26.md｜:load_xpsd_file（自报值读入点）、::spec_collector_init、::spec |
| M1a-A-004 | A_SCI_DEF | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | cd_inv 在 SCI 符号表被定义为 CD⁻¹ 却注单位 pixel/arcsec，与同文 §5 及实现 inv(trans | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:§2 符号表（cd_inv 行）/::§3 单位表/::§5 SIP 前向/::§10 不可接受变化； |
| M1a-A-005 | A_SCI_DEF | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | 容差族口径断裂：0.2% 各向异性被挪用为"CD 元素相对误差 ≤2%"（十倍），尺度窗四套并存，多数生效阈值无 SCI 出处 | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:9 容差族 与 ::11.4 F1；lib/plate_solve/cpp/ipv/include/i |
| M1a-A-006 | A_SCI_DEF | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | 历元/自行/视差传播全缺：参考历元 J2016.0 与"ICRS/J2000（Gaia DR3 同系）"混用，pm/parall | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:§3a 假设 与 ::§6 缺失项；docs/standards/STANDARDS_REGISTRY |
| M1a-A-007 | A_SCI_DEF | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | ALG 施工伪代码两处公式级错误：极点排除条件方向反转、像素→天球方向误用 CD⁻¹ | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:G1/G2 施工规格 |
| M1a-A-008 | A_SCI_DEF | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | coverage 不变量定义"足迹内存在有限像素"与同文"NaN→C=1"字面冲突（SCI 与 DATA 双处） | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:§5 coverage 定义 与 ::§8 缺失/异常表；docs/contracts/DATA_SE |
| M1a-A-009 | A_SCI_DEF | P1 | M1a | 需负责人裁决 | 仍成立 | OPEN | — | §30.4 对 ivar==0 给出双强制行为（NaN 传播态 vs 产品损坏显式错误），实现自行裁决其一 | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:§30.4 条目1/条目3；lib/phase3_session/p3_resample.cpp::不 |
| M1a-A-010 | A_SCI_DEF | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | TAN 半球界推导错误："r≥π/2 与 denom>0 数学等价"不成立，正反映射有效域不对称（真边界 ≈57.5° 而非 9 | 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md｜:p3_wcs_pix2world / ::p3_wcs_world2pix；docs/contract |
| M2a-A-2 | A_SCI_DEF | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | 面积/方差/信号单位声明链 px²↔sr 混用，variance 单位与公式维度不符，HiPS tile FITS 无 BUNI | 问题扫描/findings/A_SCI_DEF/p1/M2a_L04_L10.md｜:§3 单位表、::§3a 坐标 frame 段、::§5 累加式；docs/contracts/DAT |
| M2a-A-3 | A_SCI_DEF | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | by_coords 的最近邻/平手/重复命中/半径域界判据在 SCI/ALG/DATA 全无定义，仅存在实现事实，且平手判定依赖 | 问题扫描/findings/A_SCI_DEF/p1/M2a_L04_L10.md｜:§1/§2 全节、::§3.1 execute 段；docs/contracts/DATA_SEMAN |
| M3-A-003 | A_SCI_DEF | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | SCI-CAL 把 `flat_norm` 定义写进校准公式，但生产 `calibrate` 只做 floor；median≈1 | 问题扫描/findings/A_SCI_DEF/p1/M3_L05_L07.md｜:§5 连续定义 / §2 符号表 / §10 不可接受变化`（复核时点 :44-53、:18、:99） |
| M3-A-004 | A_SCI_DEF | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | SCI-NOISE 符号表把 `ivar` 单位写成 `pixel⁻²·ADU⁻²`，与同文 §3/§7 及 DATA/GLOS | 问题扫描/findings/A_SCI_DEF/p1/M3_L05_L07.md｜:§2 符号表 ivar 行`（复核时点 :16）对照 `同文 §3 :29`、`§7 :76`、`§9 |
| M3-A-005 | A_SCI_DEF | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 平面 LS 退化判据 `/det/>1e-24` 无出处、量纲不闭合，退化求解仍报 `has_spatial_field=1` | 问题扫描/findings/A_SCI_DEF/p1/M3_L05_L07.md｜:fill_impl`（复核时点 :392-398，关键语句 :393 `const double de |
| M3-A-006 | A_SCI_DEF | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 测光/噪声诊断公式在注释与单测中以相反增益方向书写，且与 SCI §5 诊断式量纲不符 | 问题扫描/findings/A_SCI_DEF/p1/M3_L05_L07.md｜:gain_variance 声明注释`（复核时点 :33） ；`lib/phase1/noise/no |
| M3b-A-04 | A_SCI_DEF | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | 5σ 检测阈的 σ 取自原图、判决作用于 σ=2 平滑图，虚警预算不自洽 | 问题扫描/findings/A_SCI_DEF/p1/M3b_L06.md｜:sdet_detect_impl(:1736-1738 平滑、:1747-1754 bgnoise/m |
| M3b-A-05 | A_SCI_DEF | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | θ 消歧候选集不是 Moffat4 的对称群，SCI「旋转简并不变量」恒真不可证伪 | 问题扫描/findings/A_SCI_DEF/p1/M3b_L06.md｜:moffat4_residual 二次型(:88-90)/::消歧(:389-401)/::fwhm  |
| M3b-A-06 | A_SCI_DEF | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | 有效域缺最小源间距/密度条件；SCI-PSF「块状共享假设」与实现方向相反 | 问题扫描/findings/A_SCI_DEF/p1/M3b_L06.md｜:§1(:15-18，无间距条件)；docs/science/PSF.md::§6(:59)；docs/ |
| M3b-A-07 | A_SCI_DEF | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | mag 实现统一为 box_sum，文档与偏差登记仍描述已消失的饱和星公式；零点/误差全未定义 | 问题扫描/findings/A_SCI_DEF/p1/M3b_L06.md｜:mag 分支(:2283,:2291-2313)/::StarRecord.mag 注释(:238)； |
| M4-A-03 | A_SCI_DEF | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | MAD=0 patch 的 σ→1e-12 floor 与 SCI §8「不计 control」直接冲突，未登记偏差 | 问题扫描/findings/A_SCI_DEF/p1/M4_L08_L09.md｜87；lib/phase2/src/sampler.cpp::pass1_cell sigma floo |
| M4-A-04 | A_SCI_DEF | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | SCI-REJ 宣称 7 方法，实现暴露 10 方法且 median_sigma 无科学定义，三份文档计数互斥 | 问题扫描/findings/A_SCI_DEF/p1/M4_L08_L09.md｜20,40；lib/phase2/include/astro/phase2/rejection.h:45 |
| M4-A-05 | A_SCI_DEF | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | 宪章接缝四项验收缺二（法向梯度跳变、support/weight 连续性全仓无定义），既有门槛容差冻结于测试文件内无 SCI 出 | 问题扫描/findings/A_SCI_DEF/p1/M4_L08_L09.md｜194（§6.3）；tests/api/test_seam_metric_gate.py:10,23,5 |
| M5a-A-001 | A_SCI_DEF | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | ACR 等价容差 1e-6 的推导量纲不成立且自身乘积已超其宣称包络 | 问题扫描/findings/A_SCI_DEF/p1/M5a_L11.md｜:§9 精度策略`（:84-88） ；`docs/algorithms/ACR_EQUIVALENCE. |
| M7-A-101 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-WCS §5 前向式把 SIP 写成世界坐标加数，与本文 §2/§3 声明的像素域单位及实现互斥（L19-001 改判： | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§5 连续定义`（复核时 :48 SIP 加在 (u,v) 上、:52-53 `A=cd_inv·tra |
| M7-A-102 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-CW:42 断言 snr²≡x²·ivar，在 x=0 与 B≠0 两族像元上为假（L19-002） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§4 权重进质量场式`（复核时 :42） |
| M7-A-103 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-NOISE 对 variance=0/饱和/无合格 patch 同文三套互斥强制，「全帧饱和」分支在其自身输入域内不可判 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§5 ivar 式`（:53）、`::§7 Floor 夹逼不变量`、`::§8 极端表行1-2`、`: |
| M7-A-104 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-NOISE 5 样本控制点的 MAD² 方差与满 patch 同权进无权重平面 LS（L19-008） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§4 min_samples`（:37）、`::§5 控制点/空间场` |
| M7-A-105 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-PHOT 合成注入门的参考量 F_instr=k·F_syn 未给 k 的定义与真值生成式 ⇒ 门的残差不可计算（L19 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§11 合成注入`（:107）、`::§10 Python 参考`（:110 rtol 1e-9）、`: |
| M7-A-106 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-PHOT sigma_cal_rel = ln10·sigma_residual 把回归散布当均值标准误，1/√n 因子 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§2 符号表`（:29）、`::§4/§5 输出与不确定度` |
| M7-A-108 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-REJ 对 auto 主用方法 percentile 未定义参考分布／经验分位口径／tie 规则，确定性门不可证伪（L1 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§5 n<6 分支`（:46）、`::§9a/§12/§15` |
| M7-A-109 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-REJ winsorized 的 1.134 被声明「流量中性」，实测为幅度中性（L19-013） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§5 方法表`、`::§7` |
| M7-A-110 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-REJ percentile 的 scale=/median/ 在 UPM 加性归零后的空背景主用例上退化（L19-01 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜:§5`（:46）对照 `docs/science/INTEGRATION.md::§6` |
| M7-A-112 | A_SCI_DEF | P1 | M7 | 需负责人裁决 | 仍成立 | OPEN | A-05 | NOISE_ESTIMATION 的 k_corr 查表缺维：只给 pixfrac×scale 六值，未声明其覆盖 `::§5  | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§5 控制方差块`、`docs/algorithms/PHASE2_SAMPLER.md::§5 kco |
| M7-A-113 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-UNC 的 Var(median)≈πσ²/2N 无适用域声明，却被用于 N=10 的帧级不确定度（L19-016） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§n=1 与 §帧级/小样本`、`::§协方差` |
| M7-A-114 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-UNC 的 Phase3 bilinear 方差传播式丢相邻协方差项，与同文档自家 ρ̄≈0.19 实测相抵（L19-0 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§Phase3 重采样方差传播`（:79）对照 `::§协方差` |
| M7-A-115 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-UNC 的 V19R3 括注把 k_corr 写成 N_eff，与同节 k_corr=N/N_eff≈1.4 相差约 1 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§V19R3 引言括注`（:32）对照同节 :43-44 |
| M7-A-116 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-CAL 的「median≤0 平场保持原样不归一」分支令 flat_norm 回到 ADU 量纲，cal 被 ~1/10 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§3 量纲表`、`::§4 平场条`（:38）、`::§5 双分支`、`::§8/§10` |
| M7-A-117 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-P3 跨 tile bilinear 未定义 NESTED 面邻接的轴翻转/镜像映射；leaf_order=+9 与 h | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§2 leaf_order 行`（:28）、`::§4/§5 采样行`（:63）、`::§7 常数场不变 |
| M7-A-118 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-PSF 的 FWHM 冻结常数 1.230310 与解析值 1.2303076 在第 6 位分歧，三条合同语句不能同真（ | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§2/§5/§7/§10/§11`（复核时 :20/:51/:63/:86/:118，grep 1.23 |
| M7-A-119 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 星点通量维度链 PSI→PHOT 三套标签互斥（ADU·px² / ADU·px / ADU），px² 在 ∫I dA=Σpix | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§2 flux 行/§3/§5`（复核时 :21/:28）、`docs/science/PHOTOMET |
| M7-A-120 | A_SCI_DEF | P1 | M7 | 需负责人裁决 | 仍成立 | OPEN | — | GLOSSARY 自称唯一术语权威，却缺宪章 §4.1 十量中 6/10 条（L21-002） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜表体`（18 行）、`ASTROCS_PROJECT_CONSTITUTION.md::§4.1 表` |
| M7-A-121 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 「quality」在两份 FROZEN SCI 中类型互斥（[0,1] 浮点因子 vs uint32 位掩码），flags→因子 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§3/§5`、`docs/science/CONTROL_WEIGHT_SNR.md::§/qualit |
| M7-A-122 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | NOISE_MODEL 符号表给 ivar 记「pixel⁻²·ADU⁻²」，同篇 §3 与全部合同记 ADU⁻²（L21-00 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§2 符号表`（:16）对照 `::§3`（:29）、`docs/contracts/DATA_SEMA |
| M7-A-123 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 交换合同要求三 Phase 最小平面集含「mask」，而 mask 在三个 DATA 节都被判「非产品输出」且语义三方不一（L2 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§mask/rejection 相关节`（复核时 :174/:214/:314-315）、`docs/i |
| M7-A-124 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | GLOSSARY variance/ivar 条目未随 DATA-UNC-001 双值消解同步，仍写单值口径（L21-010） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜variance/ivar 行`（:10/:11）对照 `docs/contracts/DATA_SEM |
| M7-A-125 | A_SCI_DEF | P1 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | ALG 层把 ivar 缺失回退 support 写成 weight_mode=2 的唯一语义（L21-001 的 ALG 侧独 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b |
| M7-A-126 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | ALG 中的 σ 未逐法定义：linear_fit 以平均绝对残差充当 σ 且未乘 √(π/2)（L20-016） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/linear_fit 块`（:14/:17/:98/:106） |
| M7-A-127 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | Generalized ESD 的 α=0.05 被当冻结统计判据引用，却未纳入 NIST 自述的近似有效性域（n≥15/25） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/ESD 块`（:18/:37/:57/:61） |
| M7-A-128 | A_SCI_DEF | P1 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 冻结完备性门「SNR≥10 召回≥99%」在本文阈值统计下数学不可达（L20-008 的 SCI/ALG 侧定档；家族成员） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b |
| M7-A-129 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | bilinear「Σw=1 精确成立」在跳象限、FP64 舍入、bitpix=−64 三处不成立，而它是「常数场 max_abs | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§6.5 p3_sample_bilinear`（:193 附近）、`::§6.6 值语义表`；`doc |
| M7-A-130 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | FWHM=1.230310·sx/sy 是量纲错误式（乘/除轴向宽度），自反于本文 F2 与 SCI-PSF 不变量（L20-0 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/FWHM 行`（复核时 :7/:54/:75/:151）；实现侧 `dpsf_psf.cpp` 的  |
| M7-A-131 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | MAD=0 时热/冷检测阈值退化为「高于中位即热、低于中位即冷」，文档两次判为良性（L20-011） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/MAD 分支`（:177/:181/:403） |
| M7-A-134 | A_SCI_DEF | P1 | M7 | 无法复现 | 无法判定 | OPEN | — | ALG 对同一代码给出互斥的冷/热分支符号描述（L20-012） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/冷像素块` 对照 `docs/algorithms/COSMETIC_ALGORITHMS.md:: |
| M7-A-135 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | peaker 在非极大值分支固定 x+=5 跳步可漏检；§3 与 §6 对跳步时机描述互斥（L20-009） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§3 阶段 4`（:93）、`::§6`（:148） |
| M7-A-136 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | PERCENTILE 方法既无分位数也不含噪声尺度，拒绝强度随背景绝对电平线性变化；「全接受」兜底是其症状（L20-015） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/percentile 块`（:14/:99）、`docs/algorithms/REJECTION_ |
| M7-A-137 | A_SCI_DEF | P1 | M7 | 需负责人裁决 | 仍成立 | OPEN | A-08 | 「CAR 无投影奇点」为假（极点整行塌缩未排除）；§15.3 中 θ 在两族投影间承担两个互斥几何量（L20-023） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§/CAR 行与 §15.3`（复核时 grep CAR 命中 :20/:107/:190/:348） |
| M7-A-138 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | HEALPIX_MAPPING 仅 35 行却承担 ALG-HEALPIX-* 全域：无公式、无单位/坐标系、无无效值语义（L2 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜全篇`（:23「单调」等） |
| M7-A-139 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | Phase3 重采样按全图平面 + 逐 HDU 全帧临时缓冲分配（L20-022） | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b｜§10/内存块`（:32 等） |
| M7-A-141 | A_SCI_DEF | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 候选缓冲三系数（1.25 / 3.0 / 1.15）的关系未论证，「零漏选」以「极区实测最坏 1.044」代替推导（L20-00 | 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a｜§/buffer 块`（复核时 :65-66、:149、:231-236，grep 1.25 四处命中） |
| M9-A-1 | A_SCI_DEF | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN/STILL | — | 「60″ 双路径切换阈值无数学依据」的缺陷已被本仓注释自证并在面积路径修复，但同一无依据阈值仍以 use_adaptive 判据 | 问题扫描/findings/A_SCI_DEF/p1/M9_L24_L26.md｜:DrizzleRunContext::cos_thresh_60 + ::run_drizzle_in |
| M9-A-2 | A_SCI_DEF | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | k_corr 标定表的像素尺度轴（300–600″）与生产源尺度（在册用例最大 3.22″）脱节约两个数量级：std::clam | 问题扫描/findings/A_SCI_DEF/p1/M9_L24_L26.md｜:kcorr_lookup（含 scale 未知回退）、lib/healpix_db/healpix_d |
| V1-N-12 | A_SCI_DEF | P1 | V1-N12 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 新 SNR 模型残留五处量化偏差（V1 独立复算） | 问题扫描/findings/A_SCI_DEF/p1/V1-N12.md |
| V12-N-01 | A_SCI_DEF | P1 | V12 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1·**总述根因条**）科学常数**无承载层**：`include/` 下无任何 constants 头，权威点数 = 0 | 问题扫描/findings/A_SCI_DEF/p1/V12.md |
| V12-N-03 | A_SCI_DEF | P1 | V12 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·科学口径为假且落在交付量上）`NOISE_MODEL.md:135` 声明「0.6745 与 1.48260221850 | 问题扫描/findings/A_SCI_DEF/p1/V12.md |
| V12-N-05 | A_SCI_DEF | P1 | V12 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·公式双头 + **死码占合同位**）曝光→初始极限星等公式两份字面实现，"权威函数"零调用者，两路径对同一输入差 **4 | 问题扫描/findings/A_SCI_DEF/p1/V12.md |
| V12-N-06 | A_SCI_DEF | P1 | V12 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1·依据虚构）6 处注释把「α=1.3→0.2885」的偏差登记指向**不存在的路径** ⇒ 「已登记故不动冻结文档」的理由 | 问题扫描/findings/A_SCI_DEF/p1/V12.md |
| V12-N-08 | A_SCI_DEF | P1 | V12-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1·**门禁判定**）§10.5 冻结门有**两套实现、判据集合不同、10 秒边界开闭相反**；`90.0` 与 `0.70 | 问题扫描/findings/A_SCI_DEF/p1/V12-b.md |
| V12-N-11 | A_SCI_DEF | P1 | V12-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`200000` 一具名（`.c` 私有）+ **三处独立复制**；缓存预算乘积在两个 TU 各算一遍 | 问题扫描/findings/A_SCI_DEF/p1/V12-b.md |
| W2-N-01 | A_SCI_DEF | P1 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）孔径通量**误差式与同域冻结 CCD 方程三方互斥** ⇒ σ_F 结构性低 12.86%、SNR 恒虚高 | 问题扫描/findings/A_SCI_DEF/p1/W2.md |
| W2-N-02 | A_SCI_DEF | P1 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）e⁻↔ADU 换算因子在读取边界被**实例化为 1.0 且无判别位** ⇒ 合同层「不可得」被实现成产品层「已知=1」 | 问题扫描/findings/A_SCI_DEF/p1/W2.md |
| W2-N-03 | A_SCI_DEF | P1 | W2 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1）`DATA-P1-FLUX` 端口**声明 ELECTRON 而载荷为 ADU**，而单位门因**两端同标恒不红（自我印 | 问题扫描/findings/A_SCI_DEF/p1/W2.md |
| W2-N-04 | A_SCI_DEF | P1 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`PHOTAPPL=1` **无条件硬编码** ⇒ 退化（恒等拷贝未定标）帧被声明为已应用测光，违冻结合同明文「禁硬编码 | 问题扫描/findings/A_SCI_DEF/p1/W2.md |
| W2-N-05 | A_SCI_DEF | P1 | W2 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1）`photscal` **只乘像素、不乘 variance/ivar**；唯一 α² 实现**生产零调用** ⇒ 跨帧  | 问题扫描/findings/A_SCI_DEF/p1/W2.md |
| W2-N-06 | A_SCI_DEF | P1 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）方差面 NaN 像素「**半累加**」：面积进分母、方差不进分子 ⇒ 交付方差系统性偏低 | 问题扫描/findings/A_SCI_DEF/p1/W2.md |
| W2-N-07 | A_SCI_DEF | P1 | W2-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1）HiPS 层级父元方差归约**丢弃协方差交叉项**（方差被当作可加）⇒ 父元方差低估 2×/4× | 问题扫描/findings/A_SCI_DEF/p1/W2-b.md |
| W2-N-08 | A_SCI_DEF | P1 | W2-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）交付的源像素角尺度 provenance 用 `/CD1_1/·3600`——**旋转下不是像素角尺度**，且同一量三处 | 问题扫描/findings/A_SCI_DEF/p1/W2-b.md |
| W2-N-09 | A_SCI_DEF | P1 | W2-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）**`PCi_j` 读入支路全仓缺位**，注册表仍判 Paper I §3 `CONFORMANT`、偏差「无」⇒ 旋转 | 问题扫描/findings/A_SCI_DEF/p1/W2-b.md |
| W2-N-10 | A_SCI_DEF | P1 | W2-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）星表通量**三套定义并存**：5×5 窗截断和被当「总通量」消费并被 `f_in` 二次惩罚，另有**负残差整流** | 问题扫描/findings/A_SCI_DEF/p1/W2-b.md |
| M1a-B-003 | B_STD_MISMATCH | P1 | M1a | 需负责人裁决 | 部分修复 | OPEN | A-14 | STD-F1 残余：注册表"已闭环/CONFORMANT"与同树测试"待 owner 裁决"并存，报告字符串方向与代码相反，ip | 问题扫描/findings/B_STD_MISMATCH/p1/M1a_L01_L02.｜:D.spherical STD-F1 行（:62/:74）；tests/unit/p1wcs/p1wc |
| M1a-B-004 | B_STD_MISMATCH | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | 关键字文本面 '=' 卡列位不符 FITS 卡格式，合同示例与代码相反 | 问题扫描/findings/B_STD_MISMATCH/p1/M1a_L01_L02.｜:p3_wcs_fits_keywords；lib/phase3_proj/p3_projection. |
| M1a-B-005 | B_STD_MISMATCH | P1 | M1a | 需负责人裁决 | 仍成立 | OPEN | — | hips_frame 写死 'equatorial' 非 HiPS 1.0 枚举值，却被读取侧与 ICRS 恒等消费；SCI 冻 | 问题扫描/findings/B_STD_MISMATCH/p1/M1a_L01_L02.｜:properties 序列化；lib/phase3_session/hips_properties.c |
| M2a-B-2 | B_STD_MISMATCH | P1 | M2a | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 注册表判定式与自身条款面不一致：D.catalog 条款面写"参考历元 J2016.0"，判定行的"标准要求"列丢掉历元却给 C | 问题扫描/findings/B_STD_MISMATCH/p1/M2a_L04_L10.｜:D.catalog（CLAUSES 行 vs 判定式首行）、::D.fits（CLAUSES 章节号行 |
| M2a-B-3 | B_STD_MISMATCH | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | pixfrac 有效域在接口层被扩为 [0,1] 并被迁移面固化为接口词表，SCI 禁值可过 validate()/plan() | 问题扫描/findings/B_STD_MISMATCH/p1/M2a_L04_L10.｜:§4 输入有效域、::§9a pixfrac 行；lib/healpix_db/healpix_dri |
| M2a-B-4 | B_STD_MISMATCH | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | fits_core 发布路径恒写 CHECKSUM 全零占位卡、verify 的占位豁免条件落点错误，D.fits §6 仍记  | 问题扫描/findings/B_STD_MISMATCH/p1/M2a_L04_L10.｜:fio_write_hdu（CHECKSUM 卡写出）、::acs_fio_writer_end_v1 |
| M2b-B-05 | B_STD_MISMATCH | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | SNR metadata.xml 命名空间 URI 含字面空格，产出即非合规 VOTable | 问题扫描/findings/B_STD_MISMATCH/p1/M2b_L03_L15.｜:finalize_snr_product（VOTABLE 头 fprintf 常量串，共 4 处 "h |
| M2b-B-06 | B_STD_MISMATCH | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | hips_status 缺标准必填的第三组词表、hips_hierarchy 写布尔值，均偏离标准词表 | 问题扫描/findings/B_STD_MISMATCH/p1/M2b_L03_L15.｜:finalize_image_product；docs/standards/STANDARDS_REG |
| M2b-B-08 | B_STD_MISMATCH | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | 注册表偏差登记面系统性失真：CONFORMANT 行挂开放偏差、8 个偏差 ID 不挂任何清单行、§3 索引条款与语义错挂、ST | 问题扫描/findings/B_STD_MISMATCH/p1/M2b_L03_L15.｜:D.hips 清单行 2/5/6/7；::D.drizzle 清单行 2/5；::§3 偏差索引（DI |
| M2b-B-10 | B_STD_MISMATCH | P1 | M2b | 已登记待修 | 部分修复 | OPEN | — | 注册表六域清单的「条款」列与标准章节内容整体错挂（fits 5 行 + healpix 2 行 + spherical-proj | 问题扫描/findings/B_STD_MISMATCH/p1/M2b_L03_L15.｜:D.fits CLAUSES 与清单行 1/2/3/4/5/6；::D.healpix 清单行 1/3 |
| M6a-B-001 | B_STD_MISMATCH | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | AIT 逆映射的「合法域」被实现与合同写成 A<2，而由代码自己声明的恒等式可证天球像恰为 A≤1：环带 1<A≤2 的像素被折 | 问题扫描/findings/B_STD_MISMATCH/p1/M6a_L13_L14.｜:ait_pix2world`（复核时点 :224-245：:231-232 `const double |
| M8-B-001 | B_STD_MISMATCH | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | 同一 DATA-004「运行事实不参与 digest」合同存在两套互不兼容的实现：Python 侧合规且有可失败测试，C++ 侧 | 问题扫描/findings/B_STD_MISMATCH/p1/M8_L22_L23.m｜`docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md |
| M9-B-2 | B_STD_MISMATCH | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | CDELT+CROTA2→CD 构造在两个 TU 里逐字复制、旋转方向口径全仓未固定/未登记、且无 CROTA2≠0 对照测试； | 问题扫描/findings/B_STD_MISMATCH/p1/M9_L24_L26.m｜:hp_drizzle_run（同型复制块）；口径面 docs/contracts/DATA_SEMAN |
| V10-N-01 | C_ALG_IMPL | P1 | V10 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | FITS **短读后仍 return true**：`img.pixels` 按实读量收缩、而 `width/height` 用 | 问题扫描/findings/C_ALG_IMPL/p1/V10.md |
| V10-N-02 | C_ALG_IMPL | P1 | V10 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `A_ORDER`/`AP_ORDER` 未做 `[0,5]` 校验 ⇒ SIP 求值按 order 索引 36 元素数组：`o | 问题扫描/findings/C_ALG_IMPL/p1/V10.md |
| V10-N-03 | C_ALG_IMPL | P1 | V10 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | gaia XPSD 的 `itemSize` 仍是裸 `atoi`，而它是**字节逆置换的步长与循环界** ⇒ 三条 fail- | 问题扫描/findings/C_ALG_IMPL/p1/V10.md |
| V10-N-04 | C_ALG_IMPL | P1 | V10-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `BSCALE`/`BZERO` 解析 `catch(...)` 吞错取默认、**不吃 FITS 的 D 指数记法** ⇒ 同一 | 问题扫描/findings/C_ALG_IMPL/p1/V10-b.md |
| V10-N-05 | C_ALG_IMPL | P1 | V10-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | HiPS SNR TSV 坏行**只 ++ 一个全仓无消费者的计数器** ⇒ 交付星表少行完全不可观测；`%lf` 还接受 Na | 问题扫描/findings/C_ALG_IMPL/p1/V10-b.md |
| V10-N-06 | C_ALG_IMPL | P1 | V10-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`readFits` 无 NAXIS 上限：声明尺寸直乘成分配量，而同族两个读面都设 65535 | 问题扫描/findings/C_ALG_IMPL/p1/V10-b.md｜315-321` 仅 `if (naxis1 <= 0 // naxis2 <= 0)`；`:333-3 |
| V20-N-01 | C_ALG_IMPL | P1 | V20 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1｜面1+面3）`lib/drizzle/src/module_entry.cpp`（sha `ca6c28c885d0`） | 问题扫描/findings/C_ALG_IMPL/p1/V20.md |
| V21-N-01 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`acs_cap_detect_v1` 的**四码值域被塌缩成两码**：6 站点全写成「非 ACS_CAP_OK → A | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-02 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`aio_healpix_io.cpp:732,759,786,813,846`（+`:493-498`）：HISS 的 | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-05 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`tools/check_warning_suppression.py:79-81`（判定 `:102-104`）：** | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-06 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`tools/quality/contracts/check_full_integration.py:20,29-34` | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-07 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）打包链 rc 全丢：`scripts/package_audit.py:29-30,34,62` + `tools/qu | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-12 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）产品 manifest 的完整性两面在整条链上被「**期望为空**」短路 ⇒ 换掉 `modules/*.dll` 只要 | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-13 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`module_adapters.cpp` 节点 manifest 有 **14 个键写出后全仓零回读**（含 docs | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-15 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·**§10.5 门禁的第一根因**）`cli/commands.cpp:912` `g.has_stage_annota | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-16 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`cli/commands.cpp:901` `g.kind = astrocs::ResKind::Compute;` | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V21-N-17 | C_ALG_IMPL | P1 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）内存回压**双哨兵恒假** ⇒ §17.6「无界内存增长属发布禁止项」在产品面**失去执行者** | 问题扫描/findings/C_ALG_IMPL/p1/V21.md |
| V7-N-04 | C_ALG_IMPL | P1 | V7 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | drizzle 引擎 `has_cd` 只看 CD 对角 ⇒ 合法「对角双零」WCS 被**静默替换**为 CDELT·CROT | 问题扫描/findings/C_ALG_IMPL/p1/V7.md｜428` `has_cd = (cd11 != 0 // cd22 != 0)`；退化分支 `:433- |
| V7-N-05 | C_ALG_IMPL | P1 | V7 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P3）`p1_snr.json` 不回显生效 SNR 科学配置，九键缺键→0.0 后**改了什么数值不可归因** | 问题扫描/findings/C_ALG_IMPL/p1/V7.md |
| V7-N-06 | C_ALG_IMPL | P1 | V7 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P3）`calibrate` 交付面 `dark_scale` 双语义：「应用了 K=1.0」与「暗优化未执行」逐字段不可分 | 问题扫描/findings/C_ALG_IMPL/p1/V7.md |
| V7-N-07 | C_ALG_IMPL | P1 | V7 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P3）`nested` 的类型强制与同文件同批纪律相反 ⇒ 用户 `"0"` 得到 NESTED，**tile ipix 全重 | 问题扫描/findings/C_ALG_IMPL/p1/V7.md |
| W1-N-01 | C_ALG_IMPL | P1 | W1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）ipv_select.cpp 触顶判据的**重写基于错误前提**：新注释断言每文件顺序截断，实为跨文件全局硬顶 | 问题扫描/findings/C_ALG_IMPL/p1/W1.md |
| W1-N-04 | C_ALG_IMPL | P1 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）未跟踪新代码 `lib/snr_estimator/src/module_entry.cpp`：JSON 整数字段**无 | 问题扫描/findings/C_ALG_IMPL/p1/W1-b.md |
| W3-R2-001 | C_ALG_IMPL | P1 | W3 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`aio_upm` 原子晋升在 Windows 正常路径＝**先删旧再 rename**，双失败时**新旧模型同灭** | 问题扫描/findings/C_ALG_IMPL/p1/W3.md |
| W3-R2-002 | C_ALG_IMPL | P1 | W3 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）**取消通道整体断线**：`astrocs_host_state_set_cancel` 全仓零调用 ⇒ `cancel | 问题扫描/findings/C_ALG_IMPL/p1/W3.md |
| W3-R2-003 | C_ALG_IMPL | P1 | W3 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）进程级峰值观测单例**无 epoch 复位** ⇒ node trace 的 `workers` 被记成**全进程历史峰 | 问题扫描/findings/C_ALG_IMPL/p1/W3.md |
| W3-R2-005 | C_ALG_IMPL | P1 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）provider 租约释放不配对：未 acquire 必 release、acquire(1) 失败被忽略、host_r | 问题扫描/findings/C_ALG_IMPL/p1/W3-b.md｜69-84；providers/cpu/baseline/src/baseline_provider.c |
| W3-R2-006 | C_ALG_IMPL | P1 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）主线程在采样线程存活期内无同步调用 ProcessMonitor::summary() ⇒ 数据竞争（UB） | 问题扫描/findings/C_ALG_IMPL/p1/W3-b.md｜244-320（全类零锁零 atomic：grep -cE 'std::(atomic/mutex)/l |
| W6-N-01 | C_ALG_IMPL | P1 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）三面对账：CTest 登记面把 **6 个「结构上永不可生成」的用例名当存量在册**，门无「可达性」维度 | 问题扫描/findings/C_ALG_IMPL/p1/W6.md |
| W6-N-02 | C_ALG_IMPL | P1 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`V19-N-05` 复验：**21 道门的宿主工具声明一条未补**；按更宽口径本轴另得 **74 门**（两口径不可混 | 问题扫描/findings/C_ALG_IMPL/p1/W6.md |
| W6-N-03 | C_ALG_IMPL | P1 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）安装面白名单**不闭合**：实装文件超出合同 2 个，且唯一白名单校验器**三面零注册** | 问题扫描/findings/C_ALG_IMPL/p1/W6.md |
| W6-N-04 | C_ALG_IMPL | P1 | W6-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`C-14` 扩散复验：**struct_size 三家未做、V11 四站点全未修、镜像面仍零采集** | 问题扫描/findings/C_ALG_IMPL/p1/W6-b.md |
| W6-N-05 | C_ALG_IMPL | P1 | W6-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）导出面合同**无机器消费者**：ARC-001 只有 schema 无实例、Windows 符号门判据「非空即过」、Li | 问题扫描/findings/C_ALG_IMPL/p1/W6-b.md |
| M1a-C-005 | C_DOC_CODE_GAP | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | ALG §3 施工伪代码与生产链步骤错位：容差挂错步骤、生产 build_sip 不含 IRLS/Huber 却按 legacy | 问题扫描/findings/C_DOC_CODE_GAP/p1/M1a_L01_L02.｜:§3 流程伪代码 与::§11.1 DISP-WCS-003；lib/plate_solve/cpp/ |
| M1a-C-006 | C_DOC_CODE_GAP | P1 | M1a | 已登记待修 | 部分修复 | OPEN | — | 选星口径三处互斥（残余事实）：ALG/头注称"flux 降序、默认 20、自适应 20→40→60"，实现为 box-mag 升 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M1a_L01_L02.｜:§5 选星；lib/plate_solve/cpp/ipv/src/ipv_select.cpp::（ |
| M1a-C-007 | C_DOC_CODE_GAP | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | Phase3 重采样实现级合同与现行 p3_resample.{h,cpp} 全面脱节：非目标已被实现推翻、10 符号清单与签名 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M1a_L01_L02.｜:§1 非目标、::§4 符号表、::§6.5 coverage 台账；lib/phase3_sessi |
| M1a-C-008 | C_DOC_CODE_GAP | P1 | M1a | 已登记待修 | 部分修复 | OPEN | — | HISTORY provenance 残余（部分修复定稿）：节点面 manifest 哈希已真实化，会话面仍恒 nullptr、 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M1a_L01_L02.｜:（provenance 装配）；lib/phase3_session/p3_output.cpp::H |
| M2a-C-10 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | registry 登记的 `astrocs.calibrated_frame.v1` 不满足自家 type_id 词法，该类型一 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:types[3]；contracts/data/artifact_manifest.schema.js |
| M2a-C-11 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | §30 合同 JSON 仍把两个 AIO 通道标 PENDING，与已落地的 AIO writer 能力直接矛盾 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:pending_aio_channels；lib/astro_image_io/include/aio |
| M2a-C-12 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 部分修复 | OPEN | — | 恒零天测列以 published 行 schema 下发，而合同写"不输出/未初始化"；0 落在合法值域内，无 invalid  | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:§8.2 不输出段；lib/gaia_xpsd_client/README.md::§9 已知限制 3 |
| M2a-C-2 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 部分修复 | OPEN | — | 产品元数据的 precision_mode/signal_dtype 记录"请求值"而非实际累加域，帧头 PRECISION 与 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:writeHis（hmeta.precision_mode/signal_dtype 赋值处）、::w |
| M2a-C-3 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 部分修复 | OPEN | — | 值像素 NaN/Inf 的"静默掩膜"登记在六处合同/注册面仍存在，而代码现状已是"不掩膜、直接传播"——登记本身失实 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:§5 输入校验表（值像素行）、::§10 DISP 表（DISP-DRZ-004 行）；docs/co |
| M2a-C-4 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | lib/drizzle 模块 README/module.yaml 的"当前事实"仍写无源码/DLL 未建/未接节点，与构建接线 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:§1 身份表、::§9 构建段；lib/drizzle/module.yaml::状态注释与 entr |
| M2a-C-5 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | Gaia 模块 README 与 ALG-GAIA-001 的现状陈述整体滞后于迁移落地（无测试/无 target/无 plan | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:§1 身份表、::§8 测试、::§9 构建与已知限制；lib/gaia_xpsd_client/mo |
| M2a-C-6 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN/STILL | — | 同名科学参数 pixfrac 的两条生产入口默认值分裂（0.8 vs 1.0），且"API-DRZ-001 默认 1.0"是被杜 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:DrizzleConfig::pixfrac；configs/stage1.template.json |
| M2a-C-7 | C_DOC_CODE_GAP | P1 | M2a | 已修 | 仍成立 | OPEN/FIXED | — | 块缓存 4GB 上限在文档按全局声明、实现按每文件私有（MAX_FILES=32），资源验收断言与实现不同径 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:§7 内存与资源段、::§9 测试门资源行；lib/gaia_xpsd_client/README.m |
| M2a-C-8 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 部分修复 | OPEN | — | 并发模型合同仍写"单写者"，实现自陈该前提对 match 类查询失效；lease/cancel 注入是进程级全局 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:§4 并发模型；lib/gaia_xpsd_client/src/gaia_client.c::块缓存 |
| M2a-C-9 | C_DOC_CODE_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | 字段撒谎族零覆盖：header_len 读后弃用、rootPosition/nodeCount 不设界、child 索引不校验； | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.｜:load_xpsd_file（魔数/头长/树段解析）、::parse_tree（rootPositio |
| M2b-C-02 | C_DOC_CODE_GAP | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | lib/hips README 与 module.yaml / 构建 / 打包面互相否定（DLL 目标是否存在、迁移是否开始、C | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2b_L03_L15.｜:§1 身份表 + ::§0 前言状态块；lib/hips/module.yaml::模块定位段 + e |
| M2b-C-03 | C_DOC_CODE_GAP | P1 | M2b | 已登记待修 | 部分修复 | OPEN | — | lib/hips 构建注释仍宣称「aio_hips_reader 全部不进本 DLL（grep 0 命中）」，与被 RESCUE | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2b_L03_L15.｜:头注释「零依赖剔除」段；同文件 ::[RESCUE-V3 FD-01] 更正注释与 target_so |
| M2b-C-04 | C_DOC_CODE_GAP | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | hips_version 口径四层并存（必须含 1.4 / 只要存在 / 完全不要求 / 夹具写 1.0），注册表基线又引用被自 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2b_L03_L15.｜:D.hips VERSION 字段 + D.hips 清单行 3；lib/astro_image_io |
| M3-C-004 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | `lib/calibration/CALIBRATION_PROCESS.md` 与冻结 SCI/现行实现冲突，文件自身无历史/ | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜:§2 算法流程 / §4 Dark 优化 / §5 技术细节`（复核时点 :52-62、:66-79、 |
| M3-C-005 | C_DOC_CODE_GAP | P1 | M3 | 需负责人裁决 | 部分修复 | OPEN | B-04 | 退化 master flat 的 fail-closed 只覆盖 IR 节点通道（并发修复已落地并有回归测试），p1_sessi | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜`lib/core/src/module_adapters.cpp::p1_master_flat_va |
| M3-C-006 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 公共符号数量在 ALG/README 写 14、头文件与构建注释写 12（实测 12 个 `AC_API`） | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜:§1 / §13 / 附录`（复核时点 :16、:207、:415）、`lib/calibration |
| M3-C-007 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | registry 页引用两个全库未定义的合同 ID，并与 `module.yaml`/`ac_version()` 形成版本与状 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜::7 upstream 行 / :56`；对照 `lib/calibration/module.yam |
| M3-C-008 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 模块 ID 存在 `astrocs.phase1.*` 与 `astrocs.p1.*` 两套词表，同一模块在 descript | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜:descriptor 赋值`（复核时点 :467 `d.module_id = "astrocs.ph |
| M3-C-009 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 部分修复 | OPEN | — | 编排器（A 线）的 `calibration.mode/fallback/light_exposure_s/dark_expos | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜:阶段 2 校准设置`（复核时点 :945-975）、`::run_stage_calibration` |
| M3-C-010 | C_DOC_CODE_GAP | P1 | M3 | 需负责人裁决 | 仍成立 | OPEN/FIXED | A-05 | ALG-PHOT 声明输出含 `zero_point` 字段，接口与实现均无该字段 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜::9 输出行`；对照 `lib/snr_estimator/cpp/include/snr_estim |
| M3-C-011 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | `sigma_residual=0` 一值三义、`fit_status` 编码冲突，且暴露一个实现完全不读取的配置字段 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜:cleanAndScale`（初值 :552；S==0 跳过 IRLS 的分支 :488-491；`m |
| M3-C-012 | C_DOC_CODE_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | `mag_max` 入参被静默丢弃，实际生效的是无出处的自适应星等锥（`{12,13,14,15,16}` + `n_gaia> | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.m｜:pc_calibrate_simple / _f64 / _with_gaia 三处形参`（复核时点  |
| M3b-C-03 | C_DOC_CODE_GAP | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | 饱和平台星中心输出与合同相反、整数除法两处不同式、拟合失败静默丢星无痕迹 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3b_L06.md｜:rec.cx(:2266)/::饱和局部分支(:1943-1952)/::edge_walking_c |
| M3b-C-04 | C_DOC_CODE_GAP | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | PSF [N,9] 双布局「布局 A」两列无对应缓冲，两份 L1 合同的 A/B 标签相反 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3b_L06.md｜:§1.1(:12-23)/§1.2(:25-45)；docs/contracts/DATA_SEMAN |
| M3b-C-05 | C_DOC_CODE_GAP | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | SDetParams 五字段中四个全仓零读取而文档称有消费面；检测阈三套并存；伪代码与实现相反 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M3b_L06.md｜:SDetParams(:17-27)；lib/star_detector/src/sdet_detec |
| M4-C-04 | C_DOC_CODE_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | 「单帧区 harmonic continuation 填」在 CLI 会话默认装配不可发生（P2 会话无 nodes、λs=0； | 问题扫描/findings/C_DOC_CODE_GAP/p1/M4_L08_L09.m｜88,134；lib/phase2/include/astro/phase2/upm.h:99-101； |
| M4-C-05 | C_DOC_CODE_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | 采样器并发声明「并行路径不经 g_aio_mu」不实：全部 tile 读经全局锁串行 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M4_L08_L09.m｜291,:383（§11.5）；lib/phase2/src/sampler.cpp::g_aio_mu |
| M4-C-06 | C_DOC_CODE_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | 排异非有限值语义：SCI「INVALID_* hard fail」vs 实现静默剔除/不检查 weights；oracle 测试 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M4_L08_L09.m｜34,61,87；docs/algorithms/REJECTION_ALGORITHMS.md:50； |
| M4-C-07 | C_DOC_CODE_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | 双份 reject/integrate/write 实现语义分叉，四份 slice ALG 权威面整体锚定已退出生产的 stag | 问题扫描/findings/C_DOC_CODE_GAP/p1/M4_L08_L09.m｜9（「唯一权威生产源」）vs 同文件 :497-499（自认 LEG-004）；docs/contrac |
| M5a-C-001 | C_DOC_CODE_GAP | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | --cpu-profile 在三个 phaseN run 路径被解析但从不消费，ISA 与 workers 未按 profile | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5a_L11.md｜:kAllowedFlags`（:52、:56、:60 三处 phaseN run 允许 `--cpu- |
| M5a-C-002 | C_DOC_CODE_GAP | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | FROZEN 的 EXECUTION_MODEL 并发表与代码相反：OpenMP 16、P2_ENABLE_OPENMP、sam | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5a_L11.md｜:§1 串/并行分层`（:9 Stage1 calibrate 行、:10 Stage1 drizzle |
| M5a-C-003 | C_DOC_CODE_GAP | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | 宪章 §10.5 必采的 PSS/每线程 CPU/I/O wait 未采集，资源工件名与 §11 清单两套且未登记偏差 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5a_L11.md｜:ProcSample::pss_bytes`（:70 注释声明来源 `/proc/self/smaps |
| M5a-C-004 | C_DOC_CODE_GAP | P1 | M5a | 已登记待修 | 部分修复 | OPEN | — | ARCH 文档称变体 Oracle 与 Python 参考比对，实为两个生产 provider 互比，独立性仅靠援引未做误差合成 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5a_L11.md｜:§2 变体注册`（:73） ；`tests/cpu/avx2/run_provider_avx2_ch |
| M5b-C-03 | C_DOC_CODE_GAP | P1 | M5b | 已登记待修 | 仍成立 | OPEN/STILL-SUBITEM-EXPIRED | — | 顶层幽灵命令 drizzle 在分发表却不在 help 与冻结命令树；未接线用 ARGS(2) 表达；诊断指向已删选项 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.｜:kRules`（现 :61）；`cli/parser.cpp::kHelp`（现 :66-92，无 d |
| M5b-C-04 | C_DOC_CODE_GAP | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | 实例生命周期合同要求的并发语义与 double-destroy 报错在实现中不存在，且检测本身读已释放内存 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.｜:（状态机/并发规则/错误映射）`（现 :37、:44-49、:70-73）；`include/astr |
| M5b-C-05 | C_DOC_CODE_GAP | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | 退出码在合同文档/C++ 唯一源/Runtime 映射三处互不覆盖，文档还规范化了已废弃的 orchestrator 码表 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.｜:编排退出码`（现 :22-34）；`docs/architecture/ERROR_MODEL.md: |
| M5b-C-06 | C_DOC_CODE_GAP | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | run manifest 的 provider 字段为常量 "baseline"、started/finished 取同一秒，p | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.｜:（4 处常量 provider）`（现 :4214、:4384、:5128、:5333；对照 :521 |
| M5b-C-07 | C_DOC_CODE_GAP | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | ALG 投影文档把宪章未冻结的 ZEA 列为扩展集合并漏掉已冻结的 AIT，与同文档另一节自相矛盾 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.｜:非目标/§11`（现 :20、:190）；`::正确节`（现 :106-109）；`ASTROCS_P |
| M6a-C-002 | C_DOC_CODE_GAP | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | FROZEN 架构文档仍写「TileCache 共享读+互斥加载 / LRU」，实现是每 worker 自含无锁 FIFO 缓存 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M6a_L13_L14.｜:§1 单元表 TileCache 行`（复核时点 :16「LRU(容量=配置 max_tiles),  |
| M6a-C-003 | C_DOC_CODE_GAP | P1 | M6a | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 「现状宣称」横向对账：三件套/README/头注释里以现在时书写的 6 组现状陈述与注册表、节点表、根 CMake 三源对账不符 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M6a_L13_L14.｜:文件头`（复核时点 :23-25）、`::p1_op_writer`（:2417-2421）、`::p |
| M6b-C-001 | C_DOC_CODE_GAP | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 限制层与排障层把宪章 §6.3 明令禁止的「support 当权重回退」写成既有行为，而实现默认是硬错误：三处文档口径互斥 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.｜:§6.3 科学硬约束`（现 :191） ；`docs/KNOWN_LIMITATIONS.md::第  |
| M6b-C-002 | C_DOC_CODE_GAP | P1 | M6b | 需负责人裁决 | 仍成立 | OPEN | — | 冻结 ALG 权威把已在代码修复并有回归门的缺陷登记为"现行缺陷语义"，逐符号锚表随之整体错位 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.｜:§2 sup_max 行 / §3 门序表 / §11.3 现状缺陷清单`（现 :42、:64、:17 |
| M6b-C-003 | C_DOC_CODE_GAP | P1 | M6b | 需负责人裁决 | 仍成立 | OPEN | — | 同一 ACTIVE 文档内冻结投影集新旧两版并存：§0 非目标仍写「不实现 SIN/ZEA/CAR/AIT」，§15 已按负责人 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.｜:§0 非目标 / §11 注 / §15 冻结四投影表`（现 :20、:35、:107、:190、:2 |
| M7-C-101 | C_DOC_CODE_GAP | P1 | M7 | 需负责人裁决 | 仍成立 | OPEN | — | 产品 manifest 的溯源面只有 SCI-* 没有 ALG-*/TST-* ⇒ 算法级溯源在产品出口处断裂（L21-008） | 问题扫描/findings/C_DOC_CODE_GAP/p1/M7_C_DOC_COD｜两处锚**均在位**，见下） |
| M8-C-001 | C_DOC_CODE_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | THREAD_BUDGET_ARCH.md §4 把「与线程数无关 / 无裸 data race」列为契约，却与同文 §4 自己 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8_L22_L23.m｜`docs/architecture/THREAD_BUDGET_ARCH.md::§4`（复核时点 : |
| M8-C-002 | C_DOC_CODE_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | `upm.cpp` D1 契约注释声称「worker 数无关，同一 worker 数下位精确」，而同合同在 synthetic_ | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8_L22_L23.m｜:D1 注释`（复核时点 :514「worker 数无关, 同一 worker 数下位精确」）；`lib |
| M8a-C-001 | C_DOC_CODE_GAP | P1 | M8a | 需负责人裁决 | 仍成立 | OPEN | — | 已废弃第二调度器被 README 写成「正式科学运行只有一条命令」，并整套冻结第二套 `ASTROCS_*` 退出码 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.｜:题头/唯一运行方式/退出码表/Stage1 流水线`（复核 1、3、5-13、60-75、76）；对照 |
| M8a-C-002 | C_DOC_CODE_GAP | P1 | M8a | 需负责人裁决 | 仍成立 | OPEN | A-32 | 五源状态滞后簇：同一模块的 README / module.yaml / registry 页 / 构建安装 / 产品清单对「有 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.｜:现状段`（复核 7-10、19、23）vs `lib/cosmetic/src/module_entr |
| M8a-C-003 | C_DOC_CODE_GAP | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | lib/astro_image_io README 停在 2026-07-12 快照：「零外部依赖」与自身目录、根构建图同时矛盾 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.｜:题头/特性/编译/目录结构/尾注`（复核 3、5、7、9-12、19、28、43-54、106-131 |
| M8a-C-004 | C_DOC_CODE_GAP | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | lib/healpix_db README 系列把生产真源定性为「独立仓库本地副本、.gitignore 忽略」，与根构建图、安 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.｜:GitHub 仓库/模块表/关联仓库表/依赖`（复核 7-10、14-21、31-37、45）；`li |
| M8a-C-005 | C_DOC_CODE_GAP | P1 | M8a | 需负责人裁决 | 仍成立 | OPEN | — | module_id 多套词汇并存：21 份 manifest 与 22 个生产 descriptor 的 module_id * | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.｜:题头`（1）、`lib/snr_estimator/module.yaml::module_id`（3 |
| M8a-C-006 | C_DOC_CODE_GAP | P1 | M8a | 需负责人裁决 | 仍成立 | OPEN | A-31 | 发布包 NOTICE/SBOM 由脚本内硬编码字符串生成，与实际链接闭包脱钩且含三处错标（M8a 新立） | 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.｜:license_text/SBOM 段`（复核 100-127）；`tools/make_linux_ |
| M9-C-1 | C_DOC_CODE_GAP | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | 「路径均为 UTF-8」合同下 Windows 侧无 UTF-8→UTF-16 适配层，发布原语与 IO-001/IO-002  | 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.m｜:原语 v1 段、runtime/io/fits_core.c::fio_file_open、::acs |
| M9-C-2 | C_DOC_CODE_GAP | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | 公开 C-ABI aio_wcs_pixel_scale 返回角秒/像素而单位零声明；同头兄弟 aio_wcs_rotation | 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.m｜:aio_wcs_pixel_scale、docs/contracts/API_CONTRACTS.cs |
| M9-C-3 | C_DOC_CODE_GAP | P1 | M9 | 需负责人裁决 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | ipv_polygon 视场参数以角秒命名声明、生产选择器已全面改度口径字段；两函数无生产调用方，宽视场路径为 3600 族复发 | 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.m｜:build_hex_descriptor/::geometric_vote 族（fov_diag 三签 |
| V1-N-02 | C_DOC_CODE_GAP | P1 | V1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL-SUBITEM-EXPIRED | — | SNR 物理量改了但二进制版本位没改 ⇒ 新旧 `.hiss` 同标签承载不同物理量；JSON 面加了 `snr_schema` | 问题扫描/findings/C_DOC_CODE_GAP/p1/V1.md｜52/58/64`（仍注 `snr_psf=(A-B)/mad`、`snr_phot=1/(ln10*s |
| V1-N-04 | C_DOC_CODE_GAP | P1 | V1 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 授权订正后的 SCI 文本一落地即与代码相反：宣称「当前实现不产出 sigma_F、不把任何标量称为科学 SNR」，而 P8 正 | 问题扫描/findings/C_DOC_CODE_GAP/p1/V1.md｜::44`（新写句）与 `::§7`（新红线句）；生产者 `lib/snr_estimator/**sn |
| V2-N-03 | C_DOC_CODE_GAP | P1 | V2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 算法文档把被修掉的错误口径钉成规范：`RMSE = mad·1.4826 / A <= 0.2` 与 MAD 语义画等号，代码已 | 问题扫描/findings/C_DOC_CODE_GAP/p1/V2.md｜111`（`#   RMSE=mad·1.4826/A<=0.2`）；同病 `lib/star_dete |
| V2-N-04 | C_DOC_CODE_GAP | P1 | V2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 参考星门只做了库层一半：`NO_DATA` 之后仍返回成功码、编排层无条件写 `STATUS=OK` ⇒ 未定标的恒等拷贝在交付 | 问题扫描/findings/C_DOC_CODE_GAP/p1/V2.md｜513-536`（`size<3` ⇒ `fit_used=0`/`scale_factor=1.0`/ |
| L28b-D-001 | D_COMMENT | P1 | L28b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | sdet_api 生产常量块用 8 处裸「(line NN)」自指锚，同号行是别的常量：注释给出的可定位性全假 | 问题扫描/findings/D_COMMENT/p1/L28b.md｜1783、:1784、:1785、:1786、:1787、:1788、:1789；被指锚点实际内容 同文 |
| L28b-D-002 | D_COMMENT | P1 | L28b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 生产 CPU provider 公共头把 kernel 索引表当语义权威，其中 ALG-008/ALG-009 在 docs 登 | 问题扫描/findings/D_COMMENT/p1/L28b.md｜86（一一对应/冻结承诺）、:88-:99（ALG-001/004/002/005/006/008/00 |
| L28b-D-003 | D_COMMENT | P1 | L28b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | C 边界异常屏障整改注释自报的 extern "C" 入口数与自身文件不符（reader 12↔13、writer 9↔12、a | 问题扫描/findings/D_COMMENT/p1/L28b.md｜264；lib/astro_image_io/src/hips/aio_hips_writer.cpp: |
| L28b-D-005 | D_COMMENT | P1 | L28b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | ABI 合同头以「SCI-xxx / ALG-xxx」占位与「如 "ALG-P3-003"」示例值描述运行时溯源字段：伪锚会骗过 | 问题扫描/findings/D_COMMENT/p1/L28b.md｜45、:46、:117；include/astrocs/core/module.h:21、:36、:37 |
| L28c-D-001 | D_COMMENT | P1 | L28c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | avx2/avx512 provider 合同头把 op 的入出槽位语义指派为「另一头的注释」权威，而该权威自身携带 docs  | 问题扫描/findings/D_COMMENT/p1/L28c.md｜47、:24、:80、:88；providers/cpu/avx512/include/astrocs/ |
| L28d-D-001 | D_COMMENT | P1 | L28d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 生产求解器的绝对剔除阈被注释与对比文档写成「角秒」，实现按像素比较：有效阈是宣称值的 2 倍，诊断日志把像素值印成角秒 | 问题扫描/findings/D_COMMENT/p1/L28d.md｜1016（常量定义 + 单位宣称） ；同文件 :942（被减量的真实量纲自述「残差 (像素)」）、:94 |
| L28e-D-001 | D_COMMENT | P1 | L28e | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 注释以「X-001..0NN」范围锚代逐 ID 锚：锚串本身不是任何已注册 ID | 问题扫描/findings/D_COMMENT/p1/L28e.md｜7、lib/snr_estimator/tests/p1noise/CMakeLists.txt:8、l |
| M1a-D-001 | D_COMMENT | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | inlier 权威缓冲的坐标域声明三处互斥：ALG/README 称"+0.5 契约解读"，C ABI 字段注释称"图像中心原点 | 问题扫描/findings/D_COMMENT/p1/M1a_L01_L02.md｜:ipv_get_last_inliers 字段表；docs/algorithms/PLATESOLVE |
| M3-D-001 | D_COMMENT | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | `calibrator.cpp` 文件头登记了不存在的搜索算法与固定线程数，与同文件正文自相矛盾 | 问题扫描/findings/D_COMMENT/p1/M3_L05_L07.md｜:文件头注释块`（复核时点 :3-26，关键行 :5、:14-16、:25）对照 `::calibrat |
| M3-D-002 | D_COMMENT | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 公共头用「全链路 double 不降级」总括 FP64 路径，与同段细则和 API 合同相反 | 问题扫描/findings/D_COMMENT/p1/M3_L05_L07.md｜:FP64 变体段落头注`（复核时点 :110-120，关键 :112 vs :115-116）；`li |
| M3-D-003 | D_COMMENT | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | SNR 公共头缺线程安全/生命周期注释，且 `star_ids` 承诺回填而实现直接丢弃、`fit_status` 把 NaN  | 问题扫描/findings/D_COMMENT/p1/M3_L05_L07.md｜:snr_psf_fit_quality 声明块`（复核时点 :81-87；对照 `同头 :279-28 |
| M4-D-01 | D_COMMENT | P1 | M4 | 已登记待修 | 部分修复 | OPEN | — | 公共头保留已废「SNR-aware 权重」语义与 workers 自动 OpenMP 承诺（后者与 M4-C-05 同根因：自动 | 问题扫描/findings/D_COMMENT/p1/M4_L08_L09.md｜10、:176；lib/phase2/src/upm.cpp:1387、:1478；lib/phase2 |
| M4-D-02 | D_COMMENT | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | rejection.h 公共头默认值/合法值注释与实现不符（linear_fit 4.0/3.0 vs 5.0/3.5；prof | 问题扫描/findings/D_COMMENT/p1/M4_L08_L09.md｜106-110、:172-180；实现锚 lib/phase2/src/rejection.cpp:10 |
| M5a-D-001 | D_COMMENT | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | 三个跨模块公共头把 cpu_workers=0 释义为 auto/OpenMP，实现却一律退化为 1（串行） | 问题扫描/findings/D_COMMENT/p1/M5a_L11.md｜:P2SamplerConfig.cpu_workers`（:54-56） ；`lib/phase2/i |
| M6a-D-001 | D_COMMENT | P1 | M6a | 需负责人裁决 | 仍成立 | OPEN | C-05 | 生产注释承担「未登记的裁决请求」角色：SCI 冻结 1e-4 px 门被同文件注释自证一步法数学不可达；裁决与偏差 ID 只活在 | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜:extract_wcs_sip 可达性注记`（复核时点 :397-400） ；`lib/plate_s |
| M6a-D-002 | D_COMMENT | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 同一 host_lease 合同在四个适配器里被注释成三种互斥语义，calibration 文件头与其自身实现直接相反（写「ac | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜:文件头 注入途径/降级策略`（复核时点 :13-15）vs 同文件 `::heavy 路径 acqui |
| M6a-D-003 | D_COMMENT | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 同一文件的顶部「设计」注释块保留整改前参数（SIP AP/BP 仍写 NB_GRID=7），与同文件实现（41×41、阶 5；R | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜:文件头 设计清单`（复核时点 :17「// - SIP AP/BP: 网格反变换法 (NB_GRID= |
| M6a-D-004 | D_COMMENT | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 公共头把「不影响 FP64 全链路精度」写成结论性宣称，其依据（仅 orchestrator 旧通道不调用）已被两条已接线的 F | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜:FP64 ABI 段`（复核时点 :110-119，含「(这些函数用于 master 帧预生成与坏点修 |
| M6a-D-005 | D_COMMENT | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | C 合同头把 fill 写成纯函数并承诺「clamp 到 variance_floor」，真实语义依赖进程级指针键侧表并在未登记 | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜:snr_noise_model_v1_fill 段`（复核时点 :163-165「* 像素 (像素中心 |
| M6a-D-006 | D_COMMENT | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 「Phase1/2/3 节点唯一真实 operation 委托」的权威总述与同文件实现和根 CMake 归属四处不符（不接 dp | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜:文件头 委托总述`（复核时点 :2-12，逐项 :5-6 star-psf、:7-8 wcs-plat |
| M6a-D-010 | D_COMMENT | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 横向抽查「注释三要素」齐备性：被消费者/生产者标识缺失率最高（55%），算法来源次之（15%）；缺口最重的头恰是合同头本身（as | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md｜`lib/calibration/include/astro_calibration.h::ac_cal |
| V8-N-01 | D_COMMENT | P1 | V8 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1·判据②锚无宿主）死码保留的"防清理凭据"是一条**不存在的测试符号** ⇒ 保留决定无凭据 | 问题扫描/findings/D_COMMENT/p1/V8.md |
| V8-N-02 | D_COMMENT | P1 | V8 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·判据①④，**并推翻 V1 一项判定**）「科学公式零本地副本 / 全部数值调 C ABI」复算为假：4 个宣称函数只调 | 问题扫描/findings/D_COMMENT/p1/V8.md |
| V8-N-04 | D_COMMENT | P1 | V8-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·判据②⑤）证据外置的**新形态**：裸 `REPORT.md §N` 锚，**仓内连"去哪找"都没写**（15 处，其中 | 问题扫描/findings/D_COMMENT/p1/V8-b.md |
| V8-N-07 | D_COMMENT | P1 | V8-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P1·判据③，**已挂 `A-44` 待负责人确认**）「负责人裁决 2026-09-14」被 39 行援引，其中**两族在权 | 问题扫描/findings/D_COMMENT/p1/V8-b.md |
| W1-N-02 | D_COMMENT | P1 | W1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）P14 批次新增注释**虚构不存在的接口**：称 parity 锁直调测试专用钩子 p1_op_star_psf_jso | 问题扫描/findings/D_COMMENT/p1/W1.md |
| W5-N-01 | D_COMMENT | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）**抛异常的任务在观测面被记成 COMPLETED**：`lib/core/src/executor.cpp:137`（ | 问题扫描/findings/D_COMMENT/p1/W5.md |
| W5-N-02 | D_COMMENT | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`cpu_routing.cpp:412` 的 `catch (...) { /* handled */ }`——**注 | 问题扫描/findings/D_COMMENT/p1/W5.md |
| W5-N-03 | D_COMMENT | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`worker_advisor.cpp:72` 的 cgroup 限额解析：`catch (...) { return  | 问题扫描/findings/D_COMMENT/p1/W5.md |
| W5-N-04 | D_COMMENT | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`cli/runtime_client.cpp:389` `catch (...) {}` 与 `:416` `catc | 问题扫描/findings/D_COMMENT/p1/W5.md |
| W5-N-05 | D_COMMENT | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`cli/commands.cpp:867`/`:1051`/`:1149`/`:1377`/`:1727` + `:1 | 问题扫描/findings/D_COMMENT/p1/W5.md |
| W5-N-06 | D_COMMENT | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`cli/commands.cpp:120`/`:138`/`:466`/`:542`/`:553`/`:579` 六处 | 问题扫描/findings/D_COMMENT/p1/W5.md |
| L28c-E-001 | E_TRACE_BREAK | P1 | L28c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | runtime/pipeline/typed_dag_contract.h 以「编译期合同 + ABI 冒烟测试锁定」自我背书， | 问题扫描/findings/E_TRACE_BREAK/p1/L28c.md｜1、:61-62、:3-6、:43-44、:48、:52、:119-120 |
| L28c-E-002 | E_TRACE_BREAK | P1 | L28c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | spectrum_integrator.h 把合成流量 F_syn 的数值参考实现指向仓内不存在的 Python 路径，同头另一 | 问题扫描/findings/E_TRACE_BREAK/p1/L28c.md｜3、:7（并 :35-:36） |
| L28e-E-001 | E_TRACE_BREAK | P1 | L28e | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 16 枚 TEST-UPMW/TEST-PR-UPM 被 TRACEABILITY.csv 记 VERIFIED，但 ID 在全 | 问题扫描/findings/E_TRACE_BREAK/p1/L28e.md｜58-64（TEST-UPMW-001..007）、:41,43,45,47,49,51,53,55,5 |
| L28e-E-002 | E_TRACE_BREAK | P1 | L28e | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | DATA_ARTIFACTS.md schema 表 15 枚数据合同 ID 在册而全仓真源零承载；校验器只验表内自洽 | 问题扫描/findings/E_TRACE_BREAK/p1/L28e.md｜12-16（DATA-IMG-VAR/IVAR/WEIGHT/SUPPORT/MASK-001）、:18 |
| L28e-E-003 | E_TRACE_BREAK | P1 | L28e | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | PHASE2_UPM.md 把 ALG-UPM-002/003/004 与具名符号逐条绑定，实现文件只挂另一套语义式 ID | 问题扫描/findings/E_TRACE_BREAK/p1/L28e.md｜113-115；实现 lib/phase2/src/upm.cpp:10-12、:23、:495-496 |
| M1a-E-001 | E_TRACE_BREAK | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | 文档锚系统性失真 + 状态声明被现实反驳（本轮已按符号重锚，未因漂移剔除任何结论） | 问题扫描/findings/E_TRACE_BREAK/p1/M1a_L01_L02.m｜::27-33（构建/调用现状声明）；docs/algorithms/PLATESOLVE.md::符号 |
| M1a-E-002 | E_TRACE_BREAK | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | C ABI 注释与 ALG/SCI 引用的规范文件在仓内不存在（悬空引用族） | 问题扫描/findings/E_TRACE_BREAK/p1/M1a_L01_L02.m｜::126；lib/plate_solve/cpp/ipv/src/ipv_solver.h:::108 |
| M1a-E-003 | E_TRACE_BREAK | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | Phase3 实现合同的行数与逐符号锚系统性漂移（与 M1a-E-001 同族，独立文件域） | 问题扫描/findings/E_TRACE_BREAK/p1/M1a_L01_L02.m｜::11-12；docs/algorithms/PHASE3_RSMP_IMPL.md:::13-14； |
| M2a-E-1 | E_TRACE_BREAK | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | 根因：追溯 ID 与合同锚未经定义即被引用——drizzle/gaia/数据合同三条链上 ≥9 类引用对象在当下树内不存在 | 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.m｜:§5/§6/§7/§8（`api.cpp:N` 锚簇）、::§8 legacy 段；docs/scie |
| M2a-E-2 | E_TRACE_BREAK | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | GAIA 合同链的源码行锚系统性漂移（+120~170 行），"逐函数核对/与源码一致"声明因此不可复核 | 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.m｜:§2 各小节头（角距/unproject/bbox/polar 剪枝/常量表）、::§4 互斥行；li |
| M2a-E-3 | E_TRACE_BREAK | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | DATA-GAIA-001 的两处章节引用指向错误小节，且 ctest 已注册的 TEST-GAIA-001 面未回登 modu | 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.m｜:§6 DATA 合同行、::§追溯行；lib/gaia_xpsd_client/module.yaml |
| M2a-E-4 | E_TRACE_BREAK | P1 | M2a | 已登记待修 | 部分修复 | OPEN | — | HiPS 输入读取的发布归属失实：头文件与 IO 合同称经 astrocs_io.dll 对外，而该 DLL target 不含 | 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.m｜:文件头角色行；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFA |
| M2b-E-01 | E_TRACE_BREAK | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | ALG/DATA/PUBLIC_API/PHASE3 的源码行号锚系统性漂移（+25 ~ +423 行、导出符号数由 9 变 1 | 问题扫描/findings/E_TRACE_BREAK/p1/M2b_L03_L15.m｜:各 ALG 小节「源码锚」行；docs/contracts/DATA_SEMANTICS.md::DA |
| M3-E-002 | E_TRACE_BREAK | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 本域 14 组源码锚中 13 组不命中其所称符号或区间（校准 / 测光 / 噪声 / cosmetic 域实例清单） | 问题扫描/findings/E_TRACE_BREAK/p1/M3_L05_L07.md｜:§4:37` 的锚 `star_matcher.cpp:35-40` —— 该区间实为 `percen |
| M3b-E-01 | E_TRACE_BREAK | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | 追溯行以需求 ID 充当测试 ID 且指向无断言文件；sdet/dpsf 文档行锚成批漂移约 120-170 行 | 问题扫描/findings/E_TRACE_BREAK/p1/M3b_L06.md｜65；docs/traceability/TRACEABILITY_MATRIX.json:242-26 |
| M4-E-01 | E_TRACE_BREAK | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | PHASE2_COVERAGE.md 行数/符号锚全面漂移（「实测」自称失准），SCI §5/§12 一致性锚亦过期（根因条目： | 问题扫描/findings/E_TRACE_BREAK/p1/M4_L08_L09.md｜10-11、§11.1 锚表（:253-255 复核时点：inspect_frame :59-140/b |
| M4-E-02 | E_TRACE_BREAK | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | PHASE2_INTEGRATION / PHASE2_SESSION 锚表与「现状描述」整体过期（B2-A7/session- | 问题扫描/findings/E_TRACE_BREAK/p1/M4_L08_L09.md｜4-6、:64、§7 映射句（复核时点 :166-174）、§11.4 F5（:266-268）；对照  |
| M5a-E-001 | E_TRACE_BREAK | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | 线程/预算文档的 path:line 锚点批量漂移并断言「全部有效」，另有指向不存在路径的引用 | 问题扫描/findings/E_TRACE_BREAK/p1/M5a_L11.md｜:确定性锚点`（:20 upm.cpp:495、:21 CMakeLists.txt:18 与 af76 |
| M5b-E-01 | E_TRACE_BREAK | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | MODULE_MAP 与 L0 的 module_adapters 行锚全部错指；宪章 §F.1 不存在 | 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.m｜:模块注册表行`（现 :33，引 :4257/:4282/:4309）；`docs/architectu |
| M5b-E-02 | E_TRACE_BREAK | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | FROZEN 架构文档 PHASE3_MODULE_ARCH 以不存在的 lib/phase3 为模块根，四单元与已 VERIF | 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.m｜:头部`（现 :1、:3 FROZEN）、`::§1`（现 :6-11）、`::§4 TileCache |
| M5b-E-03 | E_TRACE_BREAK | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | CLI_PROTOCOL 声明的"退出码唯一源"与 JSONL schema 路径均不存在；检查器回退到第二路径，使悬空引用不可 | 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.m｜:§2 标题`（现 :25）、`::§6 规则 3`（现 :51）、`::§3`（现 :39）；`inc |
| M5b-E-04 | E_TRACE_BREAK | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | acs_status/acs_head 在 legacy 与 abi 头族重复定义，其声称的机器检查文件不存在 | 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.m｜:（权威声明 + acs_head + acs_status + 值域）`（现 :9-10、:25-29 |
| M5b-E-05 | E_TRACE_BREAK | P1 | M5b | 已登记待修 | 仍成立 | OPEN/STILL | — | 治理引用层把 ARCHIVED 文件的字母条款号写成"宪章"，且十项映射表与检查器双向不一致 | 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.m｜:§1 表`（现 :18，"Windows 优先：宪章 §H"）；`REVIEW.md::8 发布口径` |
| M6a-E-001 | E_TRACE_BREAK | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | ABI/加载器/provider/conformance 的注释与 README 把条款号写成控制包章节（「12 §6」「15  | 问题扫描/findings/E_TRACE_BREAK/p1/M6a_L13_L14.m｜:语义节标题`（复核时点 :14「## 语义(12_DLL_ABI_AND_LOADER_STANDAR |
| M6b-E-002 | E_TRACE_BREAK | P1 | M6b | 需负责人裁决 | 仍成立 | OPEN | C-09 | 行锚/引用锚系统性漂移的机器侧根因：锚门判据只有「可解析 + 界内 + 41 条符号绑定」，作用域仅 41 篇、语法仅带文件名锚 | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.m｜:main`（现 :193-249：C2/C3 判据；:58-62 `git_ls` 无 timeout |
| M6b-E-003 | E_TRACE_BREAK | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 追溯 ID 无单一登记处：矩阵 152 个非占位 ID 中 85 个不在 contracts/INDEX.yaml（其中 VER | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.m｜:modules[*].{science_id,algorithm_id,data_id,api_id, |
| M6b-E-004 | E_TRACE_BREAK | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 交叉引用失效面：六族全仓不存在的控制包文档名被真源 139 处 / 40 文件当权威依据，另八类不存在文件/目录名合计 61 处 | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.m｜`02_FROZEN_STAGE1_HISS_SPEC(.md)`（全仓 0 文件）、`00_COMMO |
| M8a-E-001 | E_TRACE_BREAK | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | L2 层引用的 SCI/ALG ID 有 2 个在 docs 全域零命中（未注册即被引用） | 问题扫描/findings/E_TRACE_BREAK/p1/M8a_L25_L27.m｜:descriptor 摘要段`（复核 174）、`lib/phase2/module.yaml::al |
| M8a-E-002 | E_TRACE_BREAK | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | healpix_browser_qt README 的「设计文档」三份全部指向仓内不存在的路径 | 问题扫描/findings/E_TRACE_BREAK/p1/M8a_L25_L27.m｜:## 设计文档`（复核 125-130）；存在性核验：glob `docs/superpowers/* |
| M9-E-1 | E_TRACE_BREAK | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | hips_pixel_scale 的单位违规已知（L15-002 定 P0）却未回写标准注册表：D.hips §6.3.1 行仍 | 问题扫描/findings/E_TRACE_BREAK/p1/M9_L24_L26.md｜:D.hips 清单（§6.3.1 行、§4.2.1 可选键行、偏差汇总表、条款映射表）、docs/al |
| FD-F-001 | F_TEST_GAP | P1 | FD | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 「禁复制漂移」的守卫是用 `strstr` 查源文件名 —— 它测的是文本，不是行为 | 问题扫描/findings/F_TEST_GAP/p1/FD_cpu_provider_｜:check_shared`（当场读到 `CHECK(s.find("baseline_kernels_ |
| FD-F-002 | F_TEST_GAP | P1 | FD | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED-PARTIAL | — | 同机制第二实例（由前台当场实测补入，与 FD-F-001 合为「子串断言守卫」家族） | 问题扫描/findings/F_TEST_GAP/p1/FD_cpu_provider_｜243`（生产者）＋ `tests/unit/mon001_recorder_test.cpp:98`（ |
| M1a-F-002 | F_TEST_GAP | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | F6 冻结门的单位域三方不一：合同写"deg"、所引测试断言像素域、节点措辞 px ⇒ 角域在合同精度上无断言 | 问题扫描/findings/F_TEST_GAP/p1/M1a_L01_L02.md｜:§11.4 F6；tests/unit/p1_wcs_phot_test.cpp::roundtrip |
| M1a-F-003 | F_TEST_GAP | P1 | M1a | 已登记待修 | 部分修复 | OPEN | — | ALG 冻结的实场/合成精度锚无可执行 fixture，新接线的端到端链只有产物存在性断言（残余事实） | 问题扫描/findings/F_TEST_GAP/p1/M1a_L01_L02.md｜:§11.4 F1/F2 实测锚；tests/cli/test_phase1_inprocess.py； |
| M1a-F-004 | F_TEST_GAP | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | 端到端 oracle 容差与合同声明不符（1e-3 声称 ↔ 1e-2 断言），常数场判据可空转 | 问题扫描/findings/F_TEST_GAP/p1/M1a_L01_L02.md｜:（文件头 B) 判据 + 常数场 + 解析场断言）；docs/algorithms/PHASE3_RS |
| M1a-F-005 | F_TEST_GAP | P1 | M1a | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 并发修复面缺回归保护（新增条目：改过但没人守住） | 问题扫描/findings/F_TEST_GAP/p1/M1a_L01_L02.md｜见"原判据/现状"列于 _merge/M1a.md §二「已被修复（并发提交期间）」表；本条登记其**未 |
| M2a-F-2 | F_TEST_GAP | P1 | M2a | 已登记待修 | 仍成立 | OPEN | — | by_coords 的深树路径零测试覆盖：全部 fixture 只建单叶树，四叉树递归分支从未被执行 | 问题扫描/findings/F_TEST_GAP/p1/M2a_L04_L10.md｜:xpsd_write_test_file / ::xpsd_write_spectrum_file（n |
| M2a-F-3 | F_TEST_GAP | P1 | M2a | 已登记待修 | 部分修复 | OPEN | — | 两处已落地修复均无在线回归保护：TSAN 竞态用例未注册、恒零天测列无任何断言 | 问题扫描/findings/F_TEST_GAP/p1/M2a_L04_L10.md｜:块缓存锁封装段。实例②（恒零天测列）lib/gaia_xpsd_client/src/gaia_cli |
| M2a-F-4 | F_TEST_GAP | P1 | M2a | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 球面 drizzle 的注册测试面不含任何 pixfrac<1 的正面数值用例，M2a-A-1 的偏差因子无人守住 | 问题扫描/findings/F_TEST_GAP/p1/M2a_L04_L10.md｜:add_test（在线门清单）；对照 docs/science/DRIZZLE.md::§11 验证  |
| M2b-F-02 | F_TEST_GAP | P1 | M2b | 已登记待修 | 部分修复 | OPEN | — | 在册 ctest p1_hips_writer 含恒真断言与「同一文件读两次当 hash」，并以 ALL_V19 声明 vari | 问题扫描/findings/F_TEST_GAP/p1/M2b_L03_L15.md｜:主用例；tests/unit/CMakeLists.txt::p1_hips_writer add_t |
| M2b-F-03 | F_TEST_GAP | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | SNR metadata.xml 与 properties 的产品级可解析性无判别测试：只 grep 根元素子串，且真值夹具与生 | 问题扫描/findings/F_TEST_GAP/p1/M2b_L03_L15.md｜:u5_snr 段；lib/astro_image_io/tests/p1hips/p1hips_tes |
| M3-F-002 | F_TEST_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 校准 Python Oracle 用 1e-3/2e-3/3e-3 容差与 40 点抽样，替代 SCI §11 冻结的 rtol | 问题扫描/findings/F_TEST_GAP/p1/M3_L05_L07.md｜:_approx`（:126-131）、`_approx_list`（:225-227 抽样 `idx  |
| M3-F-003 | F_TEST_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | SNR-006 测试注入 σ 梯度场并按相关性判定，无法兑现「平面系数 a,b,c 在 10% 内复现」 | 问题扫描/findings/F_TEST_GAP/p1/M3_L05_L07.md｜:§11:115`、`§15:140`；`lib/snr_estimator/cpp/test/nois |
| M3-F-004 | F_TEST_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | 噪声科学验证面未接入构建/CTest：SCI-NOISE 15 行全部锚到一个不被任何目标编译的文件，且真正独立的 p1nois | 问题扫描/findings/F_TEST_GAP/p1/M3_L05_L07.md｜:SCI-NOISE-001..015 行`（复核时点 :23-:37，`test_files` 一律  |
| M3-F-005 | F_TEST_GAP | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | SCI-CW-001..008 整族无测试、无追溯行，其「ALG 下游」指向 SCI 文件自身 | 问题扫描/findings/F_TEST_GAP/p1/M3_L05_L07.md｜::41`（唯一 `TEST-CW-*` 出现处）；`docs/TRACEABILITY.csv`（`S |
| M4-F-03 | F_TEST_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | 产品 ctest「p2_upm_synthetic」不链接生产库且以 CHECK(true) 冒名不变量门、打印 PASS 结论 | 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md｜:p2_upm_synthetic_test（复核时点 :668-671）；tests/unit/p2_ |
| M4-F-04 | F_TEST_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN/STILL-SUBITEM-EXPIRED | — | tests/api UPM 确定性/恢复/接缝门永久静默跳过；驱动 calibrate_sum 恒打印 0.0，worker 等 | 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md｜14,:55,:69（复核时点）与 :49-50；tests/api/test_upm_recovery |
| M4-F-05 | F_TEST_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | UPMW-005「MC 硬门」实为 [0.98,2.0] 宽区间判据，与文件头自述断言相反，且不锚定 1.3883/1.4 | 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md｜:判据段（复核时点 :217-219）与头注释 :16；docs/science/PHASE2_UPM. |
| M4-F-06 | F_TEST_GAP | P1 | M4 | 已修 | 仍成立 | OPEN/FIXED-PARTIAL | — | backend「每节点执行证据」测试断言依赖 p2_session 日志串——当前生产 IR 链不发这些日志；5/7 节点无逐节 | 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md｜:test_01（复核时点 :84-96，断言句 :91-92；自述降级句 :88-90/:113）；t |
| M4-F-07 | F_TEST_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | p2_rejection_test/p2_output_semantics 含空断言与名不副实节（枚举值大小序/局部字面量非空/ | 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md｜:§4（复核时点 :60-72）、§5（:74-83）、§6（:85-90）；tests/unit/p2 |
| M4-F-08 | F_TEST_GAP | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | test_p2004「生产 Oracle」实为 compat 弃用接口 + 伪独立断言；compat frame_ids 映射存 | 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md｜3-8,:41-48,:157-172；lib/phase2/include/astro/phase2/ |
| M5a-F-001 | F_TEST_GAP | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | SCI-ACR-EQUIV-001 声明的 TST-ACR-* 无实体，权威八层矩阵亦无该 SCI 行 | 问题扫描/findings/F_TEST_GAP/p1/M5a_L11.md｜:§13 追溯与测试`（:110-115） ；`docs/algorithms/ACR_EQUIVALE |
| M5b-F-01 | F_TEST_GAP | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | CI-BINDING-TESTS 的门禁模式只匹配 ci/tests 21 个元测试中的 2 个 | 问题扫描/findings/F_TEST_GAP/p1/M5b_L12_L17.md｜:CI-BINDING-TESTS.command`（现 :2380-2409，`-p\/test_ci |
| M6b-F-001 | F_TEST_GAP | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 本域全部"建议新增的判据"today 无回归防护：追溯试金石只覆盖结构类 6 个 fixture，锚门 11 个负例不含"界内错 | 问题扫描/findings/F_TEST_GAP/p1/M6b_L16_L18.md｜:全部`（实测 6 个：bad_id_format.json、chain_break.json、empt |
| M7-T-101 | F_TEST_GAP | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 冻结完备性门「SNR≥10 召回≥99%」在文档自身阈值下数学不可达，且该门无在册用例（L20-008） | 问题扫描/findings/F_TEST_GAP/p1/M7_F_TEST_GAP_p1｜§11.4 F1`（复核时 :257，grep「SNR≥10 星召回」命中）、`::§3 阶段 2/3/ |
| M7-T-102 | F_TEST_GAP | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 采样器 F1 用逐位断言锁死截断常数 1.4826，使 F3 的 control_variance 解析 oracle rtol | 问题扫描/findings/F_TEST_GAP/p1/M7_F_TEST_GAP_p1｜§/F1 与 F3 测试设计`（复核时 :50/:71/:161/:168 命中 1.4826） |
| M7-T-103 | F_TEST_GAP | P1 | M7 | 已登记待修 | 部分修复 | OPEN | — | 「修复未固化」族：DISP-COV-003 缺陷已关闭，但同篇「冻结测试设计」仍要求把已废弃的旧行为钉为行为锚（L20-024） | 问题扫描/findings/F_TEST_GAP/p1/M7_F_TEST_GAP_p1｜§/冻结测试设计`（复核时 :63/:301/:344 命中 DISP-COV-003） |
| M7-T-104 | F_TEST_GAP | P1 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | G≠8 网格错位无任何负面测试，且采样器实际 G 不进 geometry hash ⇒ 门在但门看的是自家常量（M7-C-001 | 问题扫描/findings/F_TEST_GAP/p1/M7_F_TEST_GAP_p1｜p2_upm_geometry_hash`（复核时 :1351-1352 只写 `grid=`/`cel |
| M8-F-005 | F_TEST_GAP | P1 | M8 | 需负责人裁决 | 仍成立 | OPEN | — | 60 个 C/C++ 测试源零注册；而唯一针对注册面的门方向相反（只审「已注册目标是否登记」），此类缺口对 CI 永久隐形 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜:collect_real`、`::ADD_TEST_NAME_RE`、契约块 C1-C6/S1-S7（ |
| M8-F-006 | F_TEST_GAP | P1 | M8 | 已登记待修 | 部分修复 | OPEN | — | `lib/**` 与 `tests/realdata` 的 Python 测试不在任何阻断采集面；其中一批 test_* 以 r | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜:test_sip_wcs`（复核时点 :269 `return t4_ok`）、`lib/photom |
| M8-F-007 | F_TEST_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | 「X 或 true」短路恒真断言：资源门判定与 HiPS tile 结构合同被写成永真（可绕过现有 CHECK(true) 字面 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜:(4) 2 核 heavy gate`（复核时点 :78）、`tests/unit/p1_hips_w |
| M8-F-008 | F_TEST_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | 十份同构 harness 的未知组名 fail-open：组名漂移即「0 检查仍打印 TESTS PASS 且 rc=0」（同仓 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜:run_all_groups`（复核时点 :82-121）、`lib/healpix_db/healp |
| M8-F-009 | F_TEST_GAP | P1 | M8 | 需负责人裁决 | 仍成立 | OPEN/PARTIAL | C-09 | 资源门测试把 0.80 系数钉成期望值、用被测函数自造通过输入，且元测试把「注册表不得含 --gate-required」钉为期 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜:(4) gate 阈值`（复核时点 :72/:76/:79）、`tests/unit/p1_resou |
| M8-F-010 | F_TEST_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | 修复未固化：B2-A14 新增 PHOTDEGRADE / uncalibrated_adu_allowed 显式降级路径已实现 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜935-938）、`lib/healpix_db/healpix_drizzle/drizzle_eng |
| M8-F-011 | F_TEST_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | tests/realdata 的真实数据验证（36 用例）不在任何阻断 profile，仅可能出现在可豁免的 linux-dee | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜L23 原锚 `test_realdata_index_v12.py` 全仓不存在）、`tests/re |
| M8-F-012 | F_TEST_GAP | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | PAR-004「Drizzle 线程本地积累/安全归约」测试的被验对象是 provider 演示内核，"逐位一致"退化为 1/9 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜:TestDrizzleParallel`（**重锚**，L23 原锚未给类名；复核时点 :56 类声明 |
| M8-F-013 | F_TEST_GAP | P1 | M8 | 已登记待修 | 部分修复 | OPEN | — | k_corr=1.4 的「项目自产 MC 证据」与 B4-22 原子修复的唯一回归守护均为构建孤儿；文档仍以 .exe 现状口吻 | 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md｜52 `# k_corr = 1.4 保守冻结 (MC 实测 1.3883, control_media |
| M9-F-2 | F_TEST_GAP | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | hips_pixel_scale 以角秒写入 IVOA degrees 单位标准键；所谓"独立重算"O4 与产品用同一角秒公式并 | 问题扫描/findings/F_TEST_GAP/p1/M9_L24_L26.md｜:O4（同构重算面）、tests/io/make_hips_fixture.py、lib/phase2/ |
| V1-N-09 | F_TEST_GAP | P1 | V1-N09 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 同构 Oracle 抓不住公式错，且宣称的 NumPy oracle 脚本已失踪而证据清单仍指向它 | 问题扫描/findings/F_TEST_GAP/p1/V1-N09.md｜54-121（内建「独立参考实现」与被测同公式同网格规则同常数：refHalf=autoHalf、kFw |
| V15-N-05 | F_TEST_GAP | P1 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`test_frozen_constants_pinned`：断的是**声明文本** ⇒ 实现里写死 `<<9` 或 ` | 问题扫描/findings/F_TEST_GAP/p1/V15.md |
| V15-N-06 | F_TEST_GAP | P1 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1，建议负责人裁是否升 P0）`test_kernel_ids_authoritative` 的正则交替集与白名单**是同一 | 问题扫描/findings/F_TEST_GAP/p1/V15.md｜470-479`（白名单 `:432-438`），docstring 自称「实现中出现的 kernel  |
| V15-N-08 | F_TEST_GAP | P1 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`p1snr_linux_test.cpp::group_contract` C6：注释写「未参与行必须是 NaN」，断 | 问题扫描/findings/F_TEST_GAP/p1/V15.md |
| V15-N-09 | F_TEST_GAP | P1 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）MON-001/002 两测**钉 printf 呈现格式**而非值 ⇒ 改 `%.3f`、加 `%`、加单位即假红；` | 问题扫描/findings/F_TEST_GAP/p1/V15.md |
| V15-N-14 | F_TEST_GAP | P1 | V15-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·恒真短路**新站**，在册普查口径外）`p3002_uncertainty_test.cpp:275` 读回助手 `re | 问题扫描/findings/F_TEST_GAP/p1/V15-b.md |
| V15-N-15 | F_TEST_GAP | P1 | V15-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·**反向钉死未达成状态**）`tests/arch/test_single_cli.py:31-32` 把"清单里 pr | 问题扫描/findings/F_TEST_GAP/p1/V15-b.md |
| V15-N-16 | F_TEST_GAP | P1 | V15-c | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL-SUBITEM-EXPIRED | C-09 | （P1·需运行期定性）`UT-BACKEND` 的 command 只有 `unittest discover`、**无 bui | 问题扫描/findings/F_TEST_GAP/p1/V15-c.md |
| FD-G-001 | G_GOV_GATE | P1 | FD | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | A-34 | 影子树内的 agent 指令文件与冻结宪章在「唯一入口」上正面冲突，且对全部机器门永久不可见 | 问题扫描/findings/G_GOV_GATE/p1/FD_shadow_agents｜:正式运行只有 orchestrator.exe`（207 行、UNTRACKED）；同文本第二份 `r |
| FD-G-002 | G_GOV_GATE | P1 | FD | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED | — | 唯一执行「禁止运行产物落项目根目录」的门被永久容忍为红，且该检查自身声明 `waivable=False` | 问题扫描/findings/G_GOV_GATE/p1/FD_shadow_agents｜:failures[0]`（`check_id=UT-CLI`、`kind=check`、`catego |
| FD-G-004 | G_GOV_GATE | P1 | FD | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 同一检查项在 `checks.json` 写 `waivable=false`、在证据里被记为 `SKIPPED(waivabl | 问题扫描/findings/G_GOV_GATE/p1/FD_shadow_agents｜:UT-CPU-AVX512`（前台当场 python3 解析：`waivable=False`；同族  |
| M1a-G-001 | G_GOV_GATE | P1 | M1a | 已登记待修 | 部分修复 | OPEN/STILL | — | 产品头/manifest 无「坐标 frame 与像素-采样语义」声明面（宪章 §4.3 强制项只在进程内描述符存在，不随产品落 | 问题扫描/findings/G_GOV_GATE/p1/M1a_L01_L02.md｜:§4.3；lib/phase3_session/p3_output.cpp::（FITS 头键面）；l |
| M1a-G-002 | G_GOV_GATE | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | API-P3-001 状态 FROZEN 却未随宪章 §18.1/DATA-UNC-001 订正；引用的请求 schema 在仓 | 问题扫描/findings/G_GOV_GATE/p1/M1a_L01_L02.md｜::3/:16；contracts/schemas/::（缺失项）；tests/api/test_p3_ |
| M2a-G-1 | G_GOV_GATE | P1 | M2a | 已登记待修 | 仍成立 | OPEN/STILL | — | 标准注册表的机器检查器存在但未登记进 ci/checks.json，D.catalog/D.fits 的失真 CONFORMAN | 问题扫描/findings/G_GOV_GATE/p1/M2a_L04_L10.md｜:§5 机器检查段；.github/workflows/build.yml::standards-reg |
| M2b-G-01 | G_GOV_GATE | P1 | M2b | 需负责人裁决 | 仍成立 | OPEN | A-21 | HiPS 写出被建成两个独立顶层模块面（lib/hips + lib/hips_p2），与宪章 §5.2/§6.2 规定的 AI | 问题扫描/findings/G_GOV_GATE/p1/M2b_L03_L15.md｜:§5.2（HiPS 归属条款）与 ::§6.2（禁为 Phase2 另起 HiPS writer）；l |
| M2b-G-02 | G_GOV_GATE | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | 标准符合性检查器与偏差登记合同脱节：checker 无 CI 执行面、docstring 宣称的 C8 从未实现、catalog | 问题扫描/findings/G_GOV_GATE/p1/M2b_L03_L15.md｜:§4 机器可读字段说明 + ::§5 机器检查 C1–C7 + ::§5 末段（已知缺口自述）；doc |
| M2b-G-03 | G_GOV_GATE | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | docs/standards L2 标准的权威来源写成已归档控制包规格并把 MinGW 写成正式工具链，与宪章 §1.1/§15 | 问题扫描/findings/G_GOV_GATE/p1/M2b_L03_L15.md｜:权威来源行 + ::MUST 第一条（C++17/toolchain）；docs/standards/ |
| M2b-G-04 | G_GOV_GATE | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | HEALPix 依赖合规簇：同一模块同时存在许可证声明冲突与出处/版本自述冲突（NOTICE 称未迁邻居/未复制 GPL，代码称 | 问题扫描/findings/G_GOV_GATE/p1/M2b_L03_L15.md｜:迁移范围段 + ::许可声明段；lib/common/healpix/healpix_core.h:: |
| M3-G-001 | G_GOV_GATE | P1 | M3 | 需负责人裁决 | 仍成立 | OPEN | B-04 | 产品 manifest 把 cosmetic 报为 `available`，但两通道给检测的 Dark/Bias 恒为 `nul | 问题扫描/findings/G_GOV_GATE/p1/M3_L05_L07.md｜:cosmetic 阶段`（复核时点 :390-420，调用 :401-404 `ac_correct_ |
| M4-G-01 | G_GOV_GATE | P1 | M4 | 已登记待修 | 仍成立 | OPEN | — | ALG 文档一面承认 SCI 权威、一面自称「语义权威=本文件」覆盖 FROZEN SCI——同文档权威声明自相矛盾且越权句生效 | 问题扫描/findings/G_GOV_GATE/p1/M4_L08_L09.md｜:DISP-P2REJ-002 行（复核时点 :436-437 与 :542-544 重复出现）vs 同 |
| M5a-G-004 | G_GOV_GATE | P1 | M5a | 已登记待修 | 部分修复 | OPEN | — | worker 观测已接租约峰值，但 active=runnable 同值与 higher-water 恒等令三判据失去判别力 | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md｜:run_with_resource_gate` 采样线程（:755-762 的 `recorder.s |
| M5a-G-006 | G_GOV_GATE | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | 同一进程存在第二线程预算源：OpenMP 隐式组队与 hardware_concurrency 自取绕过 Runtime 预算 | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md｜:default_cpu_workers / effective_cpu_workers / defau |
| M5a-G-007 | G_GOV_GATE | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | Windows 生产构建从不探测 OpenMP：三个遗留 target 并行区静默退化串行，同图内另两个 target 却硬链  | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md｜:OpenMP 探测`（:68-72） ；`CMakeLists.txt::astrocs_aio /  |
| M5a-G-008 | G_GOV_GATE | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | ACR 休眠的机器证明不成立：符号门在 CI 恒被跳过、注册表判据永不触发、可达性只跑 selftest | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md｜:check`（二进制面 :19-22 与 :67-71；符号判据 :70；注册表面 :73-76） ； |
| M5a-G-009 | G_GOV_GATE | P1 | M5a | 已登记待修 | 部分修复 | OPEN | — | 「baseline 零 AVX 污染」只在 selftest 下运行；docstring 声称的 nm/link map 校验无 | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md｜5-13；AVX2 助记符表 :32-38；AVX512 表 :41-45；`scan_avx512`  |
| M5a-G-010 | G_GOV_GATE | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | provider 无 executor 时把 workers 固定为 1 并串行整图，公共头还把该退化写成合同 | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md｜:run_banded`（:412-433 取值与兜底；:430-431 无 executor → 串行 |
| M5b-G-07 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN/STILL | — | DOC-L0 门校验 docs/review 而非宪章 §12.1 指定的 docs/owner；点名的 PHASE_OVERV | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:DOCS/main`（现 :11-29）；`ci/checks.json::DOC-L0`（现 :36 |
| M5b-G-08 | G_GOV_GATE | P1 | M5b | 需负责人裁决 | 仍成立 | OPEN | — | AGENTS-GOV 只做关键词全含：强制 AGENTS.md 保留台账验证器判为非法的状态字面量，且不校验条款号真实性 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:REQUIRED/FORBIDDEN/main`（现 :2、:6-18、:20-38）；`AGENTS |
| M5b-G-09 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | ARCHIVED 约束文件被 L0 与 README 列为 ACTIVE_NORMATIVE 根权威，字母条款号在宪章中不存在 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:权威表`（现 :93）；`::文件头 权威`（现 :8）；`docs/owner/PIPELINE_O |
| M5b-G-10 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | ACR-DORMANT 门在缺二进制时静默 PASS，源码判据只匹配单一符号且豁免自相矛盾，nm 无 timeout | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:check/bin_path`（现 :19-24、:46-57、:63-76）、`::main ACR |
| M5b-G-11 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | heavy 分类无正向判据、资源门在 CI 内零调用点，且两个静态门的扫描面不含 provider/模块 DLL | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:R7/R9`（现 :105-110、:132-138）；`ci/run.py::requires_mo |
| M5b-G-12 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | SERIAL-HEAVY 门的核心判定为「四关键词全缺才报错」的结构式 fail-open，且声称的覆盖面与实扫四文件不符 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:docstring/scan/ALLOWLIST`（现 :3-13、:27-34、:44-60、:55 |
| M5b-G-13 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | ABI-BOUNDARY 门实际只扫 legacy 单头，PASS 文案虚报覆盖头数并含死分支 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:HEADERS/BOUNDARY_ONLY/check/死分支`（现 :13-19、:24-43、:3 |
| M5b-G-14 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | 导出符号与 ABI 合同一致性（§12.3-6）实际无有效检查：门只判符号表非空，Linux 侧零覆盖 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:（exported_abi_dumpbin 登记 + dumpbin 段）`（现 :586-590、: |
| M5b-G-15 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | CLI_PROTOCOL §6 承诺的 tools/check_cli_protocol.py 不存在；CLI-COMMAND- | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:§6`（现 :47-56）；`tools/check_cli_command_layer.py::ma |
| M5b-G-16 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | CLI 配置模板把产物写到仓库根，AGENTS.md 禁而 CLI_PROTOCOL 认，门只豁免不拦 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:kConfigTemplate`（现 :97-101，`\"output_dir\": \".\"`） |
| M5b-G-17 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | L0 顶层文档自述「当前提交实测绿」，同仓库 CI 基线自证七项以上非豁免门禁仍为红灯 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:1 一句话结论`（现 :20）；`REVIEW.md::6 机器验证入口`（现 :115-118）；` |
| M5b-G-18 | G_GOV_GATE | P1 | M5b | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 注册表合同页对未接入产品链路的模块给出 production 身份与"1/N 等价已验"完成度声明 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:职责/端口/并行/验证`（现 :15、:20-24、:34、:40、:52）；`lib/core/sr |
| M5b-G-19 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | §17-9 的 Agent 图像初审与 Owner 终审在机器上门禁面为零，视觉检查文档指向非产品浏览器 | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:发布 Gate`（现 :90-91）；`docs/owner/CHANGE_REVIEW.md::已知 |
| M5b-G-20 | G_GOV_GATE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | MODULE-READMES 门只覆盖 5 个硬编码目录，与 21 份 module.yaml / 26 份 registry  | 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md｜:docstring/MODULES/main`（现 :6、:12-18、:20-40）；`ci/che |
| M6a-G-002 | G_GOV_GATE | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 三个 Phase1 模块各自私造日志体系：以 CWD 相对路径把日志文件写进源码树 lib/*/logs/、用私有环境变量控级且 | 问题扫描/findings/G_GOV_GATE/p1/M6a_L13_L14.md｜:dpsf_get_log_level / dpsf_ensure_log_file`（复核时点 :18 |
| M6b-G-004 | G_GOV_GATE | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 追溯与文档门的有效性缺陷集合：三条追溯门无一能证伪 VERIFIED，锚门可被 git 异常整体跳过，豁免与不可豁免类别均无执行 | 问题扫描/findings/G_GOV_GATE/p1/M6b_L16_L18.md｜:main return`（现 :159，`broken`/`sym_broken` 收集后仍无条件 ` |
| M6b-G-005 | G_GOV_GATE | P1 | M6b | 已登记待修 | 移交 | OPEN | — | 宪章 §12.3 十二项机器检查在本域的落地核对表：可复算的"假覆盖"清单（追溯链三条门无一能证伪 VERIFIED，§12.3 | 问题扫描/findings/G_GOV_GATE/p1/M6b_L16_L18.md｜:§12.3 第 1/2/3/4/5/6/7/8/9/10/11/12 条`（现 :424-:442）  |
| M6b-G-006 | G_GOV_GATE | P1 | M6b | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 机器门覆盖率被文档高估：索引自述"由机器校验"仅指结构自洽，实测 123 条 ACTIVE_NORMATIVE 中至少 82 条 | 问题扫描/findings/G_GOV_GATE/p1/M6b_L16_L18.md｜:文首校验声明`（现 :18）、`::（active/archived 条目全集）`、`tools/do |
| M7-G-101 | G_GOV_GATE | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 宪章 §18.1 裁决的"四投影"在 SCI 侧仍是"仅 TAN、SIN/CAR 显式拒绝"，ALG 以「两者不互改」结案 =  | 问题扫描/findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1｜§15`（复核时 :360，grep「不互改」命中）；对照 `ASTROCS_PROJECT_CONST |
| M7-G-102 | G_GOV_GATE | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | ALG 自行出具合规结论：「冻结现状实现为合同基线；五项偏差不构成合同违反」+「无差异项（核对通过）」（L20-020） | 问题扫描/findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1｜§13`（复核时 :307）；`docs/algorithms/DRIZZLE_GEOMETRY.md: |
| M7-G-103 | G_GOV_GATE | P1 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 冻结常数缺「值×因次×适用域×推导源」四元表（跨 SCI/ALG ≥8 个常数；含负责人点名的 1.4826/k_corr/2. | 问题扫描/findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1 |
| M7-G-104 | G_GOV_GATE | P1 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 「独立 Oracle 缺位」清单（应父层／M2b 改道归集；级别由各实例持有，本条不另立总述） | 问题扫描/findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1 |
| M7-G-105 | G_GOV_GATE | P1 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | M7-G-001 家族**第 2 类**（M2b 改道并入本家族，不单立总述）：「引用不存在的标准依据」——把工程上界/自造出处 | 问题扫描/findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1 |
| M8-G-001 | G_GOV_GATE | P1 | M8 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 【元结论】注册面在三个方向上是空集：任何现有门都不能回答「这个证据/测试是否真被执行」 | 问题扫描/findings/G_GOV_GATE/p1/M8_L22_L23.md｜`tools/quality/check_ctest_registration.py::collect_ |
| M8-G-002 | G_GOV_GATE | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | 「选中检查全部合同化 SKIP」被 `ci/run.py` 判为整体 PASS；叠加 R4 不验采集数，形成"跑了个空集也算过" | 问题扫描/findings/G_GOV_GATE/p1/M8_L22_L23.md｜1038-1057，`SKIP_EXIT_CODE = 77`；`elif not executed a |
| M8-G-003 | G_GOV_GATE | P1 | M8 | 已登记待修 | 仍成立 | OPEN | — | 已知失败基线的 40 个探针自身含 4 个恒真探针，且唯一针对恒真断言的 F-022 探针只查 1 个文件 | 问题扫描/findings/G_GOV_GATE/p1/M8_L22_L23.md｜F-032 :996 条件尾部 `or True`；F-033 :1002、F-036 :1028、F- |
| M8a-G-003 | G_GOV_GATE | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | 第三方/派生代码的识别与豁免按「目录名字面」切分，登记面与真实闭包互不覆盖 | 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md｜:VENDORED_PARTS`（复核 35）、`::is_third_party`（95）、`::th |
| M8a-G-004 | G_GOV_GATE | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | 「依赖锁的机器校验入口」「preset/actions 复验」等现时态声称在 CI 注册表零接线，且已接的检查是单向 | 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md｜:依赖锁定 (BLD-004)`（复核 58-63）、`::工具链表`（4、26-27）、`::机器路径 |
| M8a-G-005 | G_GOV_GATE | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | BSD 第三方源（nanoflann / astrometry.net healpix）编入交付 DLL，许可与哈希零采集 | 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md｜:astrocs_aio`（复核 267 healpix_core.cpp）、`::astrocs_dr |
| M8a-G-006 | G_GOV_GATE | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | MODULE-READMES 门确数覆盖 5/43、要素核验 0 项且实测放行错锚（裁决：维持 P1，附与三家 P0 的实质差别 | 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md｜:MODULES`（复核 8-18）、`::合同 ID 正则`（29）、`::引用文件存在性`（38-4 |
| M8a-G-007 | G_GOV_GATE | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | 生成器自述「checker 以本生成器输出为源」不实：registry 无 diff 门，手写页与 descriptor 双向漂 | 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md｜:docstring`（复核 1-8）、`::OUT_DIR`（17）、`::git rev-parse |
| M8a-G-008 | G_GOV_GATE | P1 | M8a | 已登记待修 | 仍成立 | OPEN | — | 分层标准把宪章 §12.2 的 L2 定义改写，并把仓库外 Wiki 置于权威链首位 | 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md｜:分层表/权威链`（复核 1-22，关键 :8/:11/:13/:16-20）；`ASTROCS_PRO |
| M9-G-1 | G_GOV_GATE | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | XPSD 魔数/长度校验失败不解除映射，句柄随下一次 memset 永久失联 | 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md｜:load_xpsd_file（校验失败返回点）、::close_xpsd_file（唯一回收处）、:: |
| M9-G-2 | G_GOV_GATE | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | 树节点数组分配失败被 break 吞掉，仍返回成功并少注册若干棵树（部分天区静默为空） | 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md｜:load_xpsd_file（树解析循环的 break 与出口 return 0） |
| M9-G-3 | G_GOV_GATE | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | Gaia 配置数组计数循环可无限自增（strtod 不推进时不消费字符）→ 模块全生命周期可被一次配置挂死 | 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md｜:json_get_f64_array（计数循环）；调用方 ::gaia_cfg_parse；上游入口  |
| M9-G-4 | G_GOV_GATE | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | FITS 原子写 target/tmp 均 512 字节且静默截断：长路径下二者塌缩为同一路径（原子提交被替换为原地覆写） | 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md｜:acs_fio_writer_begin_v1（拷贝与 tmp 生成）、::fio_writer_ma |
| M9-G-5 | G_GOV_GATE | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | IO-001 原子写提交点全程无耐久性屏障：rename 前只 fflush 不 fsync，目录项亦不 sync（掉电/崩溃可 | 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md｜:acs_fio_writer_end_v1（flush→close→rename 链）、::fio_f |
| M9-G-6 | G_GOV_GATE | P1 | M9 | 需负责人裁决 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | HiPS properties 由返回 void 的函数直写正式路径：元数据写失败静默、且无 tmp+rename（同库已有 W | 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md｜:write_properties（定义与两处调用）、::write_chksum_determinis |
| V11-N-02 | G_GOV_GATE | P1 | V11 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `AstroSphereTileView` **五份镜像全部**落后（无一份可作参照物）⇒ 当前无越界路径，但"variance | 问题扫描/findings/G_GOV_GATE/p0/V11.md｜69-81` = **8 字段**（第 8 项 `const void* var_num_sum` 由  |
| V11-N-06 | G_GOV_GATE | P1 | V11-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）同一个 `struct_size` 判据在仓内有**三套互斥语义**，头规定的那套几乎没人用 | 问题扫描/findings/G_GOV_GATE/p1/V11-c.md |
| V11-N-07 | G_GOV_GATE | P1 | V11-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）唯一的 `struct_size` 机器门 `ABI-BOUNDARY`：**语料与被保护面 8/8 零交集**，且"扩 | 问题扫描/findings/G_GOV_GATE/p1/V11-c.md |
| V11-N-08 | G_GOV_GATE | P1 | V11-d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）AIO 存在**两份 ABI 握手结构、前两字段顺序相反**；被门检的那份与交付路径实际用的那份**不是同一个类型** | 问题扫描/findings/G_GOV_GATE/p1/V11-d.md |
| V11-N-09 | G_GOV_GATE | P1 | V11-d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL-SUBITEM-EXPIRED | — | （P1）导出面白名单只覆盖 5 个模块 DLL，**被跨语言消费的 5 个 DLL 零裁剪**；Gaia 的公共 ABI 头住在 | 问题扫描/findings/G_GOV_GATE/p1/V11-d.md |
| V11-N-10 | G_GOV_GATE | P1 | V11-d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | （P1·**本轮最该被记住的一条**）系统性统计：「镜像同步」与「镜像被门采集」在本仓 **100% 对应** ⇒ `N-01. | 问题扫描/findings/G_GOV_GATE/p1/V11-d.md |
| V13-N-01 | G_GOV_GATE | P1 | V13 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | MATRIX 权威侧自铸 3 枚 `EVID`，其 VERIFIED 在结构上不可被证伪 ⇒ 双视图共 5 行假 evidenc | 问题扫描/findings/G_GOV_GATE/p0/V13.md |
| V13-N-02 | G_GOV_GATE | P1 | V13 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 同批把 MATRIX 三行 `test_path` 写成**散文串**（非路径）⇒ 修完 BOM 后仍 DANGLING_REF | 问题扫描/findings/G_GOV_GATE/p0/V13.md |
| V13-N-04 | G_GOV_GATE | P1 | V13-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 追溯主表的 status 列 63/63 全 VERIFIED 却**全仓零门消费** ⇒ 「假 VERIFIED 恒不红」是结 | 问题扫描/findings/G_GOV_GATE/p1/V13-b.md |
| V13-N-06 | G_GOV_GATE | P1 | V13-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | `c3452d48` 删掉的 4 行 VERIFIED 合同行**无任何对应登记动作** ⇒ 合同面与追溯面永久分叉且恒不红 | 问题扫描/findings/G_GOV_GATE/p1/V13-b.md |
| V14-N-01 | G_GOV_GATE | P1 | V14 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | Windows 产品清单把两个 SKELETON unit 登记为 IMPLEMENTED ⇒ 过度声明随安装树进入发布评审，且 | 问题扫描/findings/G_GOV_GATE/p1/V14.md｜`cmake/astrocs.product.windows.json.in`（把 `PLATFORM- |
| V14-N-02 | G_GOV_GATE | P1 | V14 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | `MOD-NOOP` 三面矛盾：两个清单都登记 IMPLEMENTED，而它自己的 README/module.yaml/CMa | 问题扫描/findings/G_GOV_GATE/p1/V14.md｜`README:3/:17`、`module.yaml` 的 `module_status`、`CMak |
| V14-N-03 | G_GOV_GATE | P1 | V14 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 逐列 diff 两份 12 行 kernel 表：编进交付 `.so` 的那份有 **6/12 行 precision 标错** | 问题扫描/findings/G_GOV_GATE/p1/V14.md |
| V14-N-04 | G_GOV_GATE | P1 | V14-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | runtime 出厂的 `kernel_id`/`sci_contract_id` 三类失登记 ⇒ **按 kernel_id  | 问题扫描/findings/G_GOV_GATE/p1/V14-b.md |
| V14-N-05 | G_GOV_GATE | P1 | V14-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `p1_noise` 一接线，安装树就**自动多出交付 `.so`**，而两份白名单零登记；反向按纪律补登记反而**假红** | 问题扫描/findings/G_GOV_GATE/p1/V14-b.md |
| V14-N-06 | G_GOV_GATE | P1 | V14-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 两个纯 JSON 读写节点三面宣称 `cpu_heavy` + `parallel_ok=True` ⇒ 元数据节点占用/阻塞  | 问题扫描/findings/G_GOV_GATE/p1/V14-b.md |
| V14-N-08 | G_GOV_GATE | P1 | V14-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | registry 页的**生成面与修订面同盘互踩**：重跑生成器即蒸发 10 页人工修订，并把占位 ID 钉回 docs 面 | 问题扫描/findings/G_GOV_GATE/p1/V14-c.md |
| V17-N-01 | G_GOV_GATE | P1 | V17 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·①）**selfcheck 家族 10 TU / 16 门的"必败验证"接受"子进程没起来"** ⇒ 核心是 **127 | 问题扫描/findings/G_GOV_GATE/p1/V17.md |
| V17-N-02 | G_GOV_GATE | P1 | V17 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·②）`SERIAL-HARDCODE` 门（三 profile、非豁免、在册）扫描面**无存在性前置** ⇒ 一个自登记 | 问题扫描/findings/G_GOV_GATE/p1/V17.md |
| V17-N-03 | G_GOV_GATE | P1 | V17 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·②③）`test_p1003_drizzle_path.py::test_04` **恒 SKIP** ⇒ 可达性校验在 | 问题扫描/findings/G_GOV_GATE/p1/V17.md |
| V17-N-04 | G_GOV_GATE | P1 | V17-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `test_mon003_synthetic.py`：**编译失败 → `skipTest`**（该红时静默），而它守的是 §1 | 问题扫描/findings/G_GOV_GATE/p1/V17-b.md |
| V17-N-05 | G_GOV_GATE | P1 | V17-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （潜在缺陷）`test_iso_acr_gpu_isolation.py`：**生产源清单逐条 `if not isfile:  | 问题扫描/findings/G_GOV_GATE/p1/V17-b.md |
| V17-N-06 | G_GOV_GATE | P1 | V17-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 陈旧 `.so` 复用家族：种子 fits 站**外的三孪生站**，四站全部无源↔产物新鲜度前置 | 问题扫描/findings/G_GOV_GATE/p1/V17-b.md |
| V18-N-01 | G_GOV_GATE | P1 | V18 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·`C-21` 判据②在全仓现状不成立）12 套 selfcheck harness 并存**三种 `cond` 语义** | 问题扫描/findings/G_GOV_GATE/p1/V18.md |
| V18-N-02 | G_GOV_GATE | P1 | V18 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P0·**补强 `V9-N-07`，不另立机制**）四道 `waivable=false` 门的"自检形态"里，`check_ | 问题扫描/findings/G_GOV_GATE/p1/V18.md |
| V18-N-05 | G_GOV_GATE | P1 | V18-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·判据取自被检物自身）`check_log_contract.py selfcheck():301-306` **用被测生 | 问题扫描/findings/G_GOV_GATE/p1/V18-b.md |
| V18-N-09 | G_GOV_GATE | P1 | V18-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·`C-21①` 连前置清单都拿不到）`p1psf` 整族注入名**全经变量传递** ⇒ 字面量检索计数 **0**，se | 问题扫描/findings/G_GOV_GATE/p1/V18-b.md |
| V18-N-10 | G_GOV_GATE | P1 | V18-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`p1drz` 手抄清单已与注册表漂移 **3 名**：`k_injections[]` 9 名 vs `P1DRZ_C | 问题扫描/findings/G_GOV_GATE/p1/V18-b.md |
| V18-N-11 | G_GOV_GATE | P1 | V18-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·**元门 fail-open，连带扣减全部注入锁**）11 个 selfcheck 的子进程 `child_env[]= | 问题扫描/findings/G_GOV_GATE/p1/V18-b.md |
| V18-N-13 | G_GOV_GATE | P1 | V18-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）额度与豁免同源：`check_full_integration.py:39 HARDCODE_THREAD_DEBT_C | 问题扫描/findings/G_GOV_GATE/p1/V18-b.md |
| V18-N-14 | G_GOV_GATE | P1 | V18-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·两套重入判据并存，只有前一套会被空值污染）selfcheck 的"我在哪一阶段"判据**两种写法分裂** | 问题扫描/findings/G_GOV_GATE/p1/V18-b.md |
| V19-N-02 | G_GOV_GATE | P1 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`fatduck` profile **成员 0 却映射 exit 0** ⇒ 真机复验通道是恒绿空壳 | 问题扫描/findings/G_GOV_GATE/p1/V19.md |
| V19-N-03 | G_GOV_GATE | P1 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`serves_checks` 只声明 **2** 个消费者，实消费其产物的门有 **9** 道；两个 serve 步* | 问题扫描/findings/G_GOV_GATE/p1/V19.md |
| V19-N-04 | G_GOV_GATE | P1 | V19-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）UT-QUALITY 的门禁对象 build/lnx_v5_clean_rel/astrocs 在 CI 与 CMake | 问题扫描/findings/G_GOV_GATE/p1/V19-b.md |
| V19-N-05 | G_GOV_GATE | P1 | V19-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | （P1·即负责人裁决 A-28 的施工面）21 道门真调用 g++/gcc/nm/objdump/tar/bash/taskse | 问题扫描/findings/G_GOV_GATE/p1/V19-b.md |
| V19-N-06 | G_GOV_GATE | P1 | V19-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | （P1）环境守卫让 152/674 用例（22.6%）静默隐身，而 R11 与 runner 两侧都只数采集数 ⇒ 整类被 sk | 问题扫描/findings/G_GOV_GATE/p1/V19-b.md |
| V19-N-07 | G_GOV_GATE | P1 | V19-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）跨门输入依赖写不下也查不到：schema 无 inputs/depends_on 字段，而 run.py:616-623 | 问题扫描/findings/G_GOV_GATE/p1/V19-b.md |
| V19-N-08 | G_GOV_GATE | P1 | V19-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）dirty_ignore_prefixes 被用来遮蔽非豁免门无条件覆写 5 份**受跟踪** ISA 测量 CSV 并 | 问题扫描/findings/G_GOV_GATE/p1/V19-b.md |
| V2-N-02 | G_GOV_GATE | P1 | V2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 已删函数仍以 VERIFIED 留在 API 合同表而新公共函数未登记 ⇒ CON-API-CONTRACTS 在干净检出必红 | 问题扫描/findings/G_GOV_GATE/p0/V2.md｜371`（仍以 **VERIFIED** 登记已删除的 `estimate_mag_lim_by_den |
| V2-N-09 | G_GOV_GATE | P1 | V2-D2 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED | C-09 | `psf_mode` 是无条件字面量 `"fast"` ⇒ 走精确路径也标 fast，锁只查键名不查值 | 问题扫描/findings/G_GOV_GATE/p0/V2-D2.md｜1737`（产物）与 `:1750`（manifest）写死 `"fast"`；而精确路径 `:1778 |
| V2-N-10 | G_GOV_GATE | P1 | V2-D2 | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED | — | 极限星等迭代：不收敛为 fail-open、`capped` 靠 `fmod` 猜、`converged`/`query_fai | 问题扫描/findings/G_GOV_GATE/p0/V2-D2.md |
| V4-N-04 | G_GOV_GATE | P1 | V4 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 我把 M8-F-001 自降为 PARTIAL：新采集 R11 无任何 CI 执行面 | 问题扫描/findings/G_GOV_GATE/p1/V4.md |
| V6-N-01 | G_GOV_GATE | P1 | V6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 「追溯锚订正」实际删掉 TRACEABILITY.csv 四行，直接砍断两条 FROZEN SCI 的追溯端点并使 CON-TR | 问题扫描/findings/G_GOV_GATE/p1/V6.md｜26-67`（门 `CON-TRACEABILITY`，`fast`+`linux-main`+`win |
| V6-N-02 | G_GOV_GATE | P1 | V6 | 判定非缺陷 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/MOVED | — | 给 MATRIX 的 CSV 注入 UTF-8 BOM 令整条同构校验提前 return，且只改视图未改被自己声明为权威的 JS | 问题扫描/findings/G_GOV_GATE/p1/V6.md｜:_check_csv_parity:380-390`（以 `encoding="utf-8"` 打开、 |
| V6-N-04 | G_GOV_GATE | P1 | V6-evid | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 冻结 SCI 文档的推导权威指向 gitignored 目录 ⇒ 干净检出无法复核任何一条新科学锚 | 问题扫描/findings/G_GOV_GATE/p1/V6-evid.md |
| V6-N-05 | G_GOV_GATE | P1 | V6-evid | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED | — | README 自述与**同一提交**的 diff 直接矛盾：声明「未改仓库树 lib/core」，实际该提交给 `module_ | 问题扫描/findings/G_GOV_GATE/p1/V6-evid.md |
| V6-N-10 | G_GOV_GATE | P1 | V4 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 有修无账：账本 20 条的 fix_commit 全集与本轮 9 个真改代码的提交完全不相交 | 问题扫描/findings/G_GOV_GATE/p1/V4.md |
| V9-N-01 | G_GOV_GATE | P1 | V9 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·簇1 机制③+④的合成实例，且**直接推翻我工单 A1 的写法**）`c3452d48` 把三行「冻结科学门」从 `do | 问题扫描/findings/G_GOV_GATE/p1/V9.md |
| V9-N-02 | G_GOV_GATE | P1 | V9 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·机制③的放大器）`check_traceability_matrix.py:387-390` 在表头不等处**直接 re | 问题扫描/findings/G_GOV_GATE/p1/V9.md |
| V9-N-03 | G_GOV_GATE | P1 | V9 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`TRACEABILITY_MATRIX.csv` 的 **BOM 在 HEAD blob 里**（非本机脏），而 ch | 问题扫描/findings/G_GOV_GATE/p1/V9.md |
| V9-N-04 | G_GOV_GATE | P1 | V9 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·新增必红门 A5）`CON-API-CONTRACTS`（非豁免，三 profile）：`API_CONTRACTS.c | 问题扫描/findings/G_GOV_GATE/p1/V9.md |
| V9-N-05 | G_GOV_GATE | P1 | V9 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·新增必红门 A6 + 一条口径地雷）`DOC-LINE-ANCHORS`：41 条 bindings 里 **13 条  | 问题扫描/findings/G_GOV_GATE/p1/V9.md |
| V9-N-06 | G_GOV_GATE | P1 | V9 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·连带放大，非独立缺陷但决定工单量）实体红 4-5 个会被**放大成 6-7 个红门** | 问题扫描/findings/G_GOV_GATE/p1/V9.md |
| V9-N-08 | G_GOV_GATE | P1 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·机制④新根因）`CON-COMMENTS` 当前是**假绿**而非合规：注释提取正则 `re.S` 使 `//.*` * | 问题扫描/findings/G_GOV_GATE/p1/V9-b.md |
| V9-N-09 | G_GOV_GATE | P1 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·机制⑤）39 道 `ctest-target` 门**不带 `--fail-if-no-tests`** ⇒ 目标改名/ | 问题扫描/findings/G_GOV_GATE/p1/V9-b.md |
| V9-N-10 | G_GOV_GATE | P1 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·机制②新通道）`ci/run.py:898` 把 **exit 77 无条件判为 `SKIPPED(waivable)` | 问题扫描/findings/G_GOV_GATE/p1/V9-b.md |
| V9-N-12 | G_GOV_GATE | P1 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·**直接推翻"定向复跑全绿"这条自证手段**）`impact_map.json` 只触达 71/130 门 ⇒ **74 | 问题扫描/findings/G_GOV_GATE/p1/V9-b.md |
| V9-N-14 | G_GOV_GATE | P1 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·机制③在册无载体 + 9% 采集率）`CI-BINDING-TESTS` 的命令只匹配 **21 个文件中的 2 个、3 | 问题扫描/findings/G_GOV_GATE/p1/V9-b.md |
| V9-N-15 | G_GOV_GATE | P1 | V9-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·豁免面被用于**固定化规范违规**）`UT-CLI`（非豁免）把根目录产物写进 `dirty_ignore` ⇒ 门对根 | 问题扫描/findings/G_GOV_GATE/p1/V9-b.md |
| V9-N-18 | G_GOV_GATE | P1 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1·机制④四门清单）`CON-CONFIG-CONTRACTS`、`CON-BUILD-GRAPH`、`CON-SCIENC | 问题扫描/findings/G_GOV_GATE/p1/V9-c.md |
| V9-N-19 | G_GOV_GATE | P1 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·旧证据不可用）6 个 `DEEP-*` 门的最新证据停在 **2026-09-05T20:48:42Z**，而其中 ** | 问题扫描/findings/G_GOV_GATE/p1/V9-c.md |
| V9-N-20 | G_GOV_GATE | P1 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （登记为**负结果**，防邻站误修）两例"疑红实绿"，V9 自行纠正后**不立危害条** | 问题扫描/findings/G_GOV_GATE/p1/V9-c.md |
| W1-N-03 | G_GOV_GATE | P1 | W1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）未跟踪模块迁移面**整块零编译**，且新注释与负责人在案裁决相反（约 1400 行新适配器代码从未进过编译器） | 问题扫描/findings/G_GOV_GATE/p1/W1.md |
| W1-N-05 | G_GOV_GATE | P1 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）**新跨 DLL 边界结构不带自描述**：`hp_drizzle_api.h:104-111` 的 `HpAutoNsi | 问题扫描/findings/G_GOV_GATE/p1/W1-b.md |
| W1-N-06 | G_GOV_GATE | P1 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）「负责人裁定」**零登记**（机制⑫ 新实例）：P17 nside 语义三处引用一条不入登记面的裁决 | 问题扫描/findings/G_GOV_GATE/p1/W1-b.md |
| W5-N-07 | G_GOV_GATE | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`include/astrocs/abi/status_codes.h:210-211` 声明 `acs_status_ | 问题扫描/findings/G_GOV_GATE/p1/W5.md |
| W5-N-08 | G_GOV_GATE | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）`module_adapters.cpp:300-315` 的 `status_str` 是**第 4 份私有名值表** | 问题扫描/findings/G_GOV_GATE/p1/W5.md |
| W5-N-09 | G_GOV_GATE | P1 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P1）错误码登记事实源**文件不存在**：`lib/orchestrator/cpp/include/orchestrator | 问题扫描/findings/G_GOV_GATE/p1/W5.md |
| M1a-H-001 | H_NUMERIC | P1 | M1a | 已登记待修 | 仍成立 | OPEN | — | 节点重采样 worker 内 signal sampler 打开失败 fail-open：整行带停留 NaN/覆盖=0 却照常计 | 问题扫描/findings/H_NUMERIC/p1/M1a_L01_L02.md｜:p3_op_resample（worker lambda 与行带执行段）；对照 lib/phase3_ |
| M2b-H-01 | H_NUMERIC | P1 | M2b | 已登记待修 | 仍成立 | OPEN | — | sky fraction 序列化精度三向不一致（properties 固定 6 位小数 / manifest %.8f / 冻结 | 问题扫描/findings/H_NUMERIC/p1/M2b_L03_L15.md｜:finalize_image_product（properties 侧 std::to_string） |
| M3-H-001 | H_NUMERIC | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | `fill` 的非空间场分支不 apply `variance_floor`，退化模型可产出全帧 `variance=0` /  | 问题扫描/findings/H_NUMERIC/p1/M3_L05_L07.md｜:fill_impl`（复核时点 :371-418；关键 :401-408 空间分支 apply flo |
| M3-H-002 | H_NUMERIC | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | `snr_noise_scale_law` 无条件缩放 `variance`，只在 `α²>0` 且有限时缩放 `ivar` → | 问题扫描/findings/H_NUMERIC/p1/M3_L05_L07.md｜:snr_noise_scale_law`（复核时点 :447-454，关键 :449 `*varian |
| M3b-H-02 | H_NUMERIC | P1 | M3b | 已登记待修 | 部分修复 | OPEN | — | 未收敛+硬钳位参数被编排层判有效；fvec=1e10 巨残差污染 χ²；λ 无界 | 问题扫描/findings/H_NUMERIC/p1/M3b_L06.md｜:moffat4_residual(:78-80,:96-99)/::gauss_solve 奇异重试( |
| M3b-H-03 | H_NUMERIC | P1 | M3b | 已登记待修 | 仍成立 | OPEN | — | FP64 通道按 uint16 满值 65535 判饱和、mag 中间量降级 float32，与合同「全程不降级」字面冲突 | 问题扫描/findings/H_NUMERIC/p1/M3b_L06.md｜:norm/dynrange(:1794-1801)/::is_saturated(:2273)/::l |
| M5a-H-001 | H_NUMERIC | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | 出厂 CPU provider 的 Drizzle/积分/UPM 内核以 float32 累积并用裸 1e-6f 静默归零 | 问题扫描/findings/H_NUMERIC/p1/M5a_L11.md｜:run_kernel 分派`（ACS_CPU_KIDX_DRIZZLE_ACCUMULATE :264 |
| M7-H-101 | H_NUMERIC | P1 | M7 | 已登记待修 | 部分修复 | OPEN | — | 迭代类算法「未收敛」在 6 篇 ALG 中三口径：无返回码 / 有码仍回填最优 / 收敛门量纲与产品精度脱钩（L20-003） | 问题扫描/findings/H_NUMERIC/p1/M7_H_NUMERIC_p1.m｜§13 默认值表 + §6 F3 + §10 rc 表`（复核时 :380 命中 CG 判据）、`UPM |
| M7-H-102 | H_NUMERIC | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | Drizzle 面积函数以 NAN 作半球哨兵，下游拒绝判据是 w≤0 ⇒ NaN 静默进累加器污染整 tile（L20-005 | 问题扫描/findings/H_NUMERIC/p1/M7_H_NUMERIC_p1.m｜§/compute_overlap_area_g`（复核时 :73/:121 命中 NAN） |
| M7-H-103 | H_NUMERIC | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 绝对量纲阈与哨兵跨 6 篇未随量纲/电平定标；UPM 求值接口双哨兵（NaN vs 0.0）构成 fail-open（L20-0 | 问题扫描/findings/H_NUMERIC/p1/M7_H_NUMERIC_p1.m｜§6 F4/§13`（复核时 :173/:174/:176/:237 命中）、`PHASE3_RSMP_ |
| M7-H-104 | H_NUMERIC | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 噪声模型把负预测方差夹到 1e-12 地板 ⇒ 模型失效像素获得全链最大 ivar，且该地板被逐位冻结（L20-027） | 问题扫描/findings/H_NUMERIC/p1/M7_H_NUMERIC_p1.m｜§/floor 块`（复核时 :11/:21/:26/:39） |
| M9-H-3 | H_NUMERIC | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | read_leaf_block 用文件自报偏移与长度直接寻址映射区，无映射长度约束（源侧越界读） | 问题扫描/findings/H_NUMERIC/p1/M9_L24_L26.md｜:read_leaf_block（映射区寻址与 memcpy 两处）、::load_xpsd_file（ |
| M9-H-4 | H_NUMERIC | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | HiPS SNR 目录读出的容量入参为 int 且未判负，(size_t) 强转使负值变巨值 → 六个调用方缓冲越界写 | 问题扫描/findings/H_NUMERIC/p1/M9_L24_L26.md｜:aio_hips_read_snr_catalog；合同面 lib/astro_image_io/in |
| M9-H-5 | H_NUMERIC | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | AHPX 权重/SNR 块按字节数 memcpy 进按 float 个数 resize 的缓冲（堆越界写 1~3 字节 + 几何 | 问题扫描/findings/H_NUMERIC/p1/M9_L24_L26.md｜:readSnr、::readWeight；正对照同文件 ::readPixels；次级面 ::getI |
| M9-H-6 | H_NUMERIC | P1 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | AHPX 块描述符把 JSON 浮点强转 uint64 作偏移/长度、不与文件长度对照，读取用 (long) 窄化（LLP64  | 问题扫描/findings/H_NUMERIC/p1/M9_L24_L26.md｜:AhpxReader::parseHeader（重锚后；offset/size 强转）、::readR |
| V5-N-03 | H_NUMERIC | P1 | V5 | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | FIXED/FIXED | — | 畸形 `magnitudeRange` 经裸 `atof` 进入新剪枝谓词 ⇒ 整文件 100% 静默漏星；该字段在剪枝引入前完 | 问题扫描/findings/H_NUMERIC/p1/V5.md｜::1201-1203`（`magnitude_low/high` 由裸 `atof` 解析，仅校验"X |
| FD-I-001 | I_DOC_HYGIENE | P1 | FD | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `providers/` 一名两指：不编译的源目录 与 登记为 IMPLEMENTED 的交付路径共用同一目录名 | 问题扫描/findings/I_DOC_HYGIENE/p1/FD_providers_｜17`（`unit_id=PROV-CPU-BASELINE`、`kind=provider`、`rel |
| M3-I-001 | I_DOC_HYGIENE | P1 | M3 | 已登记待修 | 仍成立 | OPEN | — | P1-CAL/P1-COS 文档族的「现状断言」整体过期：已入库并被消费的代码仍被写成未编译、无符号、无调用方 | 问题扫描/findings/I_DOC_HYGIENE/p1/M3_L05_L07.md｜:§0`（:8-9「尚未存在生产符号」）；`lib/cosmetic/README.md::§目录现状` |
| M3-I-002 | I_DOC_HYGIENE | P1 | M3 | 无法复现 | 无逐条处置行（合并层新增/未列） | OPEN/CLOSED-ANCHOR-DEAD | — | `docs/science/PHASE1_API.md:8` 称 `ac_set_num_threads` 属「遗留（不在本契约 | 问题扫描/findings/I_DOC_HYGIENE/p1/M3_L05_L07.md｜:§范围`（复核时点 :8）；对照 `lib/calibration/include/astro_cal |
| M5a-I-001 | I_DOC_HYGIENE | P1 | M5a | 已登记待修 | 仍成立 | OPEN | — | 两份同名 ACR_EQUIVALENCE.md 给出互斥等价语义（逐位等价 vs 1e-6 容差） | 问题扫描/findings/I_DOC_HYGIENE/p1/M5a_L11.md｜3 状态 FROZEN、:84-88 容差、:99-103 验证 Oracle） ；`docs/algo |
| M5b-I-01 | I_DOC_HYGIENE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | 活动文档以历史轮次（V19R8/预发布 v5）结论冒充现状；同名词表在两份"发布状态"文件中含义互斥 | 问题扫描/findings/I_DOC_HYGIENE/p1/M5b_L12_L17.m｜:标题/概述/Gate 字面量`（现 :1-8）；`docs/DOCUMENT_INDEX.yaml:: |
| M5b-I-02 | I_DOC_HYGIENE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | README「当前状态」段陈述的是历史里程碑，与同仓库现状相反；顶层 README 不在版本门扫描面 | 问题扫描/findings/I_DOC_HYGIENE/p1/M5b_L12_L17.m｜:当前状态`（现 :9-37）；`::权威与当前状态`（现 :26、:31）；对照 `docs/owne |
| M5b-I-03 | I_DOC_HYGIENE | P1 | M5b | 已登记待修 | 仍成立 | OPEN | — | 发布状态结论与既有实现不一致的三处直接冲突（Windows 就绪 / 科学合同状态 / 投影 registry 范围） | 问题扫描/findings/I_DOC_HYGIENE/p1/M5b_L12_L17.m｜:（Windows 侧复验）`（现 :9）；`docs/owner/SCIENCE_OVERVIEW.m |
| M6a-I-001 | I_DOC_HYGIENE | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 合同与模块三件套把「实测 N 行」写成现状断言却整体滞后（7 个文件全部不符），phase3_proj 三件套的两条「现状」陈述 | 问题扫描/findings/I_DOC_HYGIENE/p1/M6a_L13_L14.m｜10-11「coverage.cpp（239 行实测）与唯一权威签名头 coverage.h（59 行） |
| M6a-I-002 | I_DOC_HYGIENE | P1 | M6a | 已登记待修 | 部分修复 | OPEN | — | 部分修复残余：合同字段 result.coverage_ok 仍被写路径无条件置 1（真值只在 verify 路径计算），头注释 | 问题扫描/findings/I_DOC_HYGIENE/p1/M6a_L13_L14.m｜:p3_output_write_atomic_ex 结果段`（复核时点 :360-379：:374 ` |
| M6a-I-003 | I_DOC_HYGIENE | P1 | M6a | 已登记待修 | 仍成立 | OPEN | — | 活动 README/模块状态面的「N/N PASS」与「全 PASS」宣称无仓内可核锚；本次复核另发现被指的测试脚本在 CI 面 | 问题扫描/findings/I_DOC_HYGIENE/p1/M6a_L13_L14.m｜:文件表/运行测试`（复核时点 :12「tests/abi/test_secure_loader.py  |
| M6b-I-001 | I_DOC_HYGIENE | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 现行入口叙述失实：多份 ACTIVE 文档把 ARCH-002 已废止的 orchestrator.exe / astrocs- | 问题扫描/findings/I_DOC_HYGIENE/p1/M6b_L16_L18.m｜:标题 + Symptom 表`（现 :1、:7、:21、:23、:31、:33） ；`docs/ARC |
| M6b-I-002 | I_DOC_HYGIENE | P1 | M6b | 已登记待修 | 仍成立 | OPEN | — | 冻结与基线页以现在时给出不具备定位性的验收结论，并复活 owner §0 已废止的 PASS 口径 | 问题扫描/findings/I_DOC_HYGIENE/p1/M6b_L16_L18.m｜:文首状态机 + PERFORMANCE_BASELINE/FINALIZATION_SELF_REVI |
| M7-I-101 | I_DOC_HYGIENE | P1 | M7 | 已登记待修 | 仍成立 | OPEN | — | 复杂度/内存声明与本文自身伪代码、数据结构不相容（跨 6 篇 ALG 的 8 处实例，L20-004） | 问题扫描/findings/I_DOC_HYGIENE/p1/M7_I_DOC_HYGI｜节` 符号锚；行号注「复核时」） |
| W4-R2-01 | J_FS_PUBLISH | P1 | W4 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `aio_publish.cpp` 删除/校验遍历的 `snprintf` 拼接一律不查截断 ⇒ 长路径下按前缀路径操作，`rm | 问题扫描/findings/J_FS_PUBLISH/p1/W4.md |
| W4-R2-02 | J_FS_PUBLISH | P1 | W4 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 同文件符号链接/并发删除面：`publish_rmrf` 以 `stat`（随链接）判目录 ⇒ 指向树外目录的 symlink  | 问题扫描/findings/J_FS_PUBLISH/p1/W4.md |
| M3-A-007 | A_SCI_DEF | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | 「幂等归一」不变量与 I3 测试隐含未声明前提，多数像素被 floor 时存在数学反例 | 问题扫描/findings/A_SCI_DEF/p2/M3_L05_L07.md｜:§7 独立不变量（幂等归一不变量）`（复核时点 :65）、`§11/§15`（:108、:134） ； |
| M3-A-008 | A_SCI_DEF | P2 | M3 | 需负责人裁决 | 仍成立 | OPEN | A-12 | SCI-NOISE 的 MAD→σ 精度断言算术为假，同一常数三套字面值且无单一权威定义点 | 问题扫描/findings/A_SCI_DEF/p2/M3_L05_L07.md｜:§9 精度策略`（复核时点 :91） ；`docs/algorithms/NOISE_ESTIMATI |
| M3-A-009 | A_SCI_DEF | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | SCI-CAL §8/§9a 用「FP32 饱和语义」描述浮点溢出，术语与 IEEE-754 行为相反且实现实为 inf/NaN | 问题扫描/findings/A_SCI_DEF/p2/M3_L05_L07.md｜:§8 极端/退化条件表 t_light/t_dark 行`（复核时点 :77）与 `§9a satur |
| M4-A-06 | A_SCI_DEF | P2 | M4 | 已登记待修 | 仍成立 | OPEN | — | SCI §4 profile 合法集与同文档 §5 及实现三方互斥；「非法⇒INVALID_CONFIGURATION」通道失实 | 问题扫描/findings/A_SCI_DEF/p2/M4_L08_L09.md｜21,35,49-50；lib/phase2/src/rejection.cpp:1039-1046；d |
| M6a-A-001 | A_SCI_DEF | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | kMaxOrder=20 的注释出处与算式皆不实（ARCH-P3 §3 无 order 上界；HEALPix 叶数是 12·4ⁿ | 问题扫描/findings/A_SCI_DEF/p2/M6a_L13_L14.md｜:kMaxOrder`（复核时点 :23 `static constexpr int kMaxOrder |
| M7-A-201 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-DRZ §6 用「外接圆半径 1.25」去「覆盖」一条全对角 1.532，比较对象不同阶，句子按字面为假（L19-004 | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§6 假设`（复核时 :75，grep「覆盖赤道对角线」「1.532」「1.044」同处命中） |
| M7-A-202 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-DRZ 9003 例「全枚举」与所列因子奇偶/整除互斥（L19-005） | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§7 零漏选不变量`（:85）、`::§11`（:115）；同句式复制进注册表 `docs/standa |
| M7-A-203 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-NOISE 的全局兜底式 `max(vmed_or_sig², floor)` 量纲两读（ADU² vs ADU⁴）（L | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§5 定义块`（:51） |
| M7-A-204 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-PHOT 的 S=0 分支「跳过 IRLS 取 median(r)」与同页 `scale=10^(−location)` | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§4 S 定义`、`::§5 S=0 分支与 scale 式`（:39/:59）、`::§10` |
| M7-A-205 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-UPM 对 k_corr 值域自相矛盾：§4 接受 0<k<1、§6 要求 k≥1，k<1 使 N_eff>N_reta | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§4`（:35）、`::§6`（:71）、`::§7`（:79） |
| M7-A-206 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-PSF 的通量/宽度链在 §2 与 §3 之间 px 因子不闭合（flux 三标签已由 M7-A-119 定档，本条只登 | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§5/§11`（:63/:118） |
| M7-A-207 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-CAL 母版生成对全 NaN 像素取 median 是空集运算，行为未定义且 §8 缺行（L19-021） | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§4 数值条`（:37）、`::§8` |
| M7-A-208 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-WCS §5a 实测证据表两行隐含像元尺度互斥（1.63e-4 vs 1.375e-4 deg/px，差 18.5%）且 | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§5a 实测证据表`（复核时 :91 命中；:97 同族） |
| M7-A-209 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-P3 的 `order_needed` 用 `s_out_rad`，§2 声明 s_out 单位 deg/px，deg→ | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§2 s_out 行`（:29）、`::§5 公式`（:55） |
| M7-A-210 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-ACR 的 weight_mode 数值枚举与名称集合两表不闭合，护栏覆盖域不可判定（L19-027） | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§2 wmode 行`（:19）、`::§4`（:33） |
| M7-A-211 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | NOISE_MODEL 自证 MAD 双截断常数「差 <1e-12」失实，实差 5.602e-12（L21-013） | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§2/§4/§9/§135`（复核时 :17/:46/:91/:135 四处命中，grep `1.482 |
| M7-A-212 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | COSMETIC 三处次级正确性错误：中值对重复计数「天然免疫」为假、label 0 的 size 说明逻辑反、out_hot/ | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§/相应块`（复核时 :141/:144/:153/:199） |
| M7-A-213 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | 误差预算以「≪」串接不同类量，被优化项小于同篇声明的主项；切平面分支等价性无可证伪判据（L20-029） | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§/`（:215）、`docs/algorithms/UPM_SOLVER.md::§/`（:101）、 |
| M7-A-214 | A_SCI_DEF | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-INT 常量场「位精确」门（max_abs==0）在 IEEE-754 下对一般常数 C 不可满足，且同节并存 rtol | 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.m｜§7 常量场不变量`、`::§11 常量场门与 Python 参考`（复核时 :102 命中 `max_ |
| M8-A-001 | A_SCI_DEF | P2 | M8 | 已登记待修 | 部分修复 | OPEN | — | provider oracle 判定器把 2e-4 相对容差与截断到 1.4826 的 MAD 常数写成默认口径，与被判定文档（ | 问题扫描/findings/A_SCI_DEF/p2/M8_L22_L23.md｜7/:78 引用 docs/science 口径、:111 `1.4826`、:186 `tol = 2 |
| M9-A-3 | A_SCI_DEF | P2 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | query_disc / queryDisc 三名二制：同名查询函数半径参数 arcsec 与 deg 并存，公共版与浏览器版签 | 问题扫描/findings/A_SCI_DEF/p2/M9_L24_L26.md｜:astrocs::healpix::query_disc（radius_arcsec）、lib/hea |
| V12-N-02 | A_SCI_DEF | P2 | V12 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P2·**反向钉死的机器门**）`snr_constants` 把**截断串在场**固化为过门必要条件 ⇒ 按 `S-1` 统 | 问题扫描/findings/A_SCI_DEF/p2/V12.md |
| V12-N-04 | A_SCI_DEF | P2 | V12 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·伪精度）`kTrimMeanToSigma` 的 16 位值**不等于它自称的解析值**：第 7 位起就错（rel −4 | 问题扫描/findings/A_SCI_DEF/p2/V12.md |
| V12-N-07 | A_SCI_DEF | P2 | V12-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL-SUBITEM-EXPIRED | — | （P2）割线迭代 5 个兜底字面量与结构默认值**各写一遍**；`±6.0` 全仓无承载 | 问题扫描/findings/A_SCI_DEF/p2/V12-b.md |
| V12-N-09 | A_SCI_DEF | P2 | V12-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）同一个「32」**三义**、比较号不同、可覆盖通道只作用于其中一条 ⇒ **边界点两侧结论相反** | 问题扫描/findings/A_SCI_DEF/p2/V12-b.md |
| V12-N-10 | A_SCI_DEF | P2 | V12-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）1.2 bbox 裕量：三处文本保证强度互斥 + **赤道分支缺"锥触极"回退守卫**；V12 造出可复算漏星反例 | 问题扫描/findings/A_SCI_DEF/p2/V12-b.md |
| V12-N-12 | A_SCI_DEF | P2 | V12-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）波长网格 `step`：测试面钉 **1**、真库是 **2** ⇒ 凡经 fixture 验证的 λ 轴只覆盖真库一半 | 问题扫描/findings/A_SCI_DEF/p2/V12-b.md |
| V12-N-13 | A_SCI_DEF | P2 | V12-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P3）`n_target` 被夹进 `[50,60]`，注释写"统一为 60"，文档写"20→40→60" ⇒ 三套口径互斥， | 问题扫描/findings/A_SCI_DEF/p2/V12-b.md |
| V12-N-14 | A_SCI_DEF | P2 | V12-c | 判定非缺陷 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/REJECT-NEVER-EXISTED | — | （P2）Moffat-β 先验**三套数值**：`2.5` / `2.5532` / `2.5531628` | 问题扫描/findings/A_SCI_DEF/p2/V12-c.md |
| V12-N-15 | A_SCI_DEF | P2 | V12-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/CANNOT-REPRODUCE | — | （P2）门常量 `511` 与 `512` **互为字面量、无派生、无 `static_assert`** | 问题扫描/findings/A_SCI_DEF/p2/V12-c.md |
| V12-N-16 | A_SCI_DEF | P2 | V12-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | （P2）`sqrt(10)` 与 `ln10` **各两处独立硬编码**，而 `MAG_PER_LN10` 是正例 ⇒ 证明本仓 | 问题扫描/findings/A_SCI_DEF/p2/V12-c.md |
| V12-N-17 | A_SCI_DEF | P2 | V12-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）光度学**零点常数四套 + 两处兜底**（`22.5` / `20.6` / `20.48` / `14.0` / `0 | 问题扫描/findings/A_SCI_DEF/p2/V12-c.md |
| W2-N-11 | A_SCI_DEF | P2 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）交付帧头**同时写 CD 与由 CD 伪造的 `CDELT1/2` + `CROTA2=0`**（不可逆且不等价） | 问题扫描/findings/A_SCI_DEF/p2/W2.md |
| W2-N-12 | A_SCI_DEF | P2 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`WcsTan` 交付 RA 值域 **[−180,180)** 与合同 **[0,360)** 互斥，而交叉门用同一  | 问题扫描/findings/A_SCI_DEF/p2/W2.md |
| W2-N-13 | A_SCI_DEF | P2 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）Phase3 投影退化的「**四角同半球**」守卫对跨极恒真 ⇒ 冻结文本这一判据本身不能表达其意图 | 问题扫描/findings/A_SCI_DEF/p2/W2.md |
| W2-N-15 | A_SCI_DEF | P2 | W2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）「独立参考实现」测试**硬写本仓明令禁止的魔数 210960**，且私有上限与生产不同制 | 问题扫描/findings/A_SCI_DEF/p2/W2.md |
| M1a-B-006 | B_STD_MISMATCH | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | 求解后头块不清理输入自带 CDELT1/2（CD 与 CDELT 混写），且 SIP 回写循环含 i+j<2 非法项 | 问题扫描/findings/B_STD_MISMATCH/p2/M1a_L01_L02.｜:run_stage_platesolve（WCS 头写回块） |
| M8a-B-001 | B_STD_MISMATCH | P2 | M8a | 已登记待修 | 部分修复 | OPEN | — | SCI 把 RCR 方法的语义权威锚定为「官方 rcr 2.4.7 软件参考」，引用未落实到推导位 | 问题扫描/findings/B_STD_MISMATCH/p2/M8a_L25_L27.｜`docs/science/REJECTION.md::§9a 统计假设行`（130）与 `::§14  |
| V10-N-07 | C_ALG_IMPL | P2 | V10-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）三个 module_entry 的**自定义 JSON 解析器**：数组元素数无上限（先巨额分配再拒）、`double→ | 问题扫描/findings/C_ALG_IMPL/p2/V10-c.md |
| V10-N-08 | C_ALG_IMPL | P2 | V10-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2，**现产不可达**）`hips_order` 第四站点无界 ⇒ `1u << leaf_order_` 移位 UB；or | 问题扫描/findings/C_ALG_IMPL/p2/V10-c.md |
| V10-N-09 | C_ALG_IMPL | P2 | V10-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P3）CLI/环境变量面裸解析：同 TU 已有带 `endptr` 的写法未推广 | 问题扫描/findings/C_ALG_IMPL/p2/V10-c.md |
| V20-N-02 | C_ALG_IMPL | P2 | V20 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2｜面1）`max_workers` 六模块解析、四个生效、**两个零消费**，其中 hips 连「仅作回显」的承诺都不成立 | 问题扫描/findings/C_ALG_IMPL/p2/V20.md |
| V20-N-03 | C_ALG_IMPL | P2 | V20-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·面1+面3）snr 的 `noise_cfg` 判别位与冗余副本**只写不读**，而**同结构体其余字段全都读** ⇒  | 问题扫描/findings/C_ALG_IMPL/p2/V20-c.md |
| V20-N-04 | C_ALG_IMPL | P2 | V20 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2｜面2）`cli/commands.cpp`（sha `b85aec30ec7c`）`--resource-detail` | 问题扫描/findings/C_ALG_IMPL/p2/V20.md |
| V20-N-05 | C_ALG_IMPL | P2 | V20-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·面1）phase3 平铺「特征键」`output_fits_path`/`sampler_used`/`mode` 的* | 问题扫描/findings/C_ALG_IMPL/p2/V20-b.md |
| V20-N-07 | C_ALG_IMPL | P2 | V20-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·面3）导出面形参三态：`(void)` 刻意未用 41 处 对 **真·零引用未标注 4 处** | 问题扫描/findings/C_ALG_IMPL/p2/V20-b.md |
| V20-N-08 | C_ALG_IMPL | P2 | V20-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·面3+面4）`P3nGeom.frame` 与 `coverage_output`：**唯一合法值就是缺省值**，且写入 | 问题扫描/findings/C_ALG_IMPL/p2/V20-b.md |
| V20-N-09 | C_ALG_IMPL | P2 | V20-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·面3）**只写不读字段已成簇**：6 个结构体 / 18 个字段 | 问题扫描/findings/C_ALG_IMPL/p2/V20-b.md |
| V20-N-11 | C_ALG_IMPL | P2 | V20-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （轴主定 **P3**，账本按现存三档落 P2 并注记）非法值折叠到缺省：`snr_max_sources` 的 **写 -1／ | 问题扫描/findings/C_ALG_IMPL/p2/V20-c.md |
| V20-N-12 | C_ALG_IMPL | P2 | V20-d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·面1）`p3_verify.json` 读 `module_build_id` 而生产者**从不写该键** ⇒ 交付产物 | 问题扫描/findings/C_ALG_IMPL/p2/V20-d.md |
| V21-N-03 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`cli/monitor.h:144,157,179-180` + 唯一调用点 `:230`：Linux 分支自算的 ` | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V21-N-04 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`modules/conformance/echo/src/echo_module.c:350,362,371`：出参  | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V21-N-08 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`cli/resource_recorder.h:43` 的 `ResRecord::system_cpu_pct`：* | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V21-N-09 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`cli/monitor.h:76-79`（填点 `:124-126,:138-140,:166-167,:175-17 | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V21-N-10 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`include/astrocs/core/context.h:31-39,170-200` + `lib/core/s | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V21-N-11 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`lib/astro_image_io/include/hiss_format.h:352` 的 `uint32_t n | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V21-N-14 | C_ALG_IMPL | P2 | V21 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）安装树合同与产品清单的版本/归因字段：`packaging/install-tree.contract.json:5`  | 问题扫描/findings/C_ALG_IMPL/p2/V21.md |
| V7-N-01 | C_ALG_IMPL | P2 | V7 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）cosmetic 参数面**两通道值域几乎不相交**：同一 config 在 IR 生产通道与 DLL 通道交付**不同 | 问题扫描/findings/C_ALG_IMPL/p2/V7.md |
| V7-N-02 | C_ALG_IMPL | P2 | V7 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P2）SIP 缺键→36 个系数全零但 `present=true` ⇒ 交付**声明"畸变已修"而实际未修**；判别位记声明 | 问题扫描/findings/C_ALG_IMPL/p2/V7.md |
| V7-N-03 | C_ALG_IMPL | P2 | V7 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | （P2，**机制级**，本卷最值一条）typed-artifact 消费面「缺键 ⇒ 就地默认」全仓规模：兜底把**上游显式拒绝 | 问题扫描/findings/C_ALG_IMPL/p2/V7.md |
| W3-R2-004 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）p1_session_run 仍全局改写 OpenMP ICV 且不恢复；登记锚已漂移（:162-165 → 现 :25 | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜259-263；lib/calibration/src/ac_api.cpp:128-134；登记面 d |
| W3-R2-007 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）astrocs_process.h 承诺「超时杀进程树」，实现=单进程 SIGKILL；无进程组/Job Object、 | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜21、:29；cli/process.cpp:113-115（Win）、:188-194（POSIX）； |
| W3-R2-008 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）executor.cpp 头注宣称「全仓唯一 executor 池、scheduler 不再自建 std::thread | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜19-20、:39；lib/core/src/scheduler.cpp:339-341；另 lib/p |
| W3-R2-009 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）p1_atomic_publish 兜底「删目标再 rename」且注释自证不违反原子性；同仓另一处注释明令禁止同型—— | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜1183-1205；矛盾对照 lib/astro_image_io/src/hiss_stream_wr |
| W3-R2-010 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）aio_frame_save_cache：tmp 以 _wfopen(UTF-16) 打开、改名却用 MoveFileE | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜78-95（open_utf8_file: CP_UTF8→_wfopen）、:877-878、:911 |
| W3-R2-011 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）CON-008 BoundedAsyncQueue 缺 §10.6 五判据中的「超时」；全仓零生产消费者 | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜44-67（push/pop 均无限 wait，无 wait_for/deadline）；宪章 §10. |
| W3-R2-012 | C_ALG_IMPL | P2 | W3-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）节点 plan 自报 max_workers 只进重节点闸门、不进租约请求：writer 自报 1「不占整份预算」实际按 | 问题扫描/findings/C_ALG_IMPL/p2/W3-b.md｜54-57（语义声明：供 NodeSpec min/max 使用）；lib/core/src/sched |
| W6-N-06 | C_ALG_IMPL | P2 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）preset 契约声明指向**不存在文件**、其校验器零注册，`build.sh` 与 preset 面不同源 | 问题扫描/findings/C_ALG_IMPL/p2/W6.md |
| W6-N-07 | C_ALG_IMPL | P2 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）头独立编译：**唯一悬空引号 include + 8 处 stddef 闭包缺**，且仓内无此类门 | 问题扫描/findings/C_ALG_IMPL/p2/W6.md |
| W6-N-08 | C_ALG_IMPL | P2 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`ACR-DORMANT`（fast+双 main、**非豁免**）的 nm 判据锚 `build/root-cmake | 问题扫描/findings/C_ALG_IMPL/p2/W6.md |
| W6-N-09 | C_ALG_IMPL | P2 | W6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`p1_noise_adapter` 的「自动复活」承诺**在受跟踪面缺落点** ⇒ 永久零注册却双在册 | 问题扫描/findings/C_ALG_IMPL/p2/W6.md |
| M2a-C-13 | C_DOC_CODE_GAP | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | gaia_client_get_spectrum_params 缺句柄空指针防护，是同族查询符号里唯一不设防者，且合同未声明该差 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M2a_L04_L10.｜:gaia_client_get_spectrum_params；对照 ::gaia_client_ge |
| M2a-C-14 | C_DOC_CODE_GAP | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | exchange/manifest schema 与执行校验器在三处口径分叉，"与 schema 一一对应"声明不成立 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M2a_L04_L10.｜:type_id/storage_uri；contracts/data/artifact_manifes |
| M2b-C-05 | C_DOC_CODE_GAP | P2 | M2b | 已登记待修 | 仍成立 | OPEN | — | lib/hips/types.h 的 ALG-HIPS-002..005 语义映射与 ALG-HIPS-001 §0 分节索引整 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M2b_L03_L15.｜:ASTROCS_HIPS_*_ID 宏段（「公式语义源」注释）；docs/algorithms/HIP |
| M2b-C-06 | C_DOC_CODE_GAP | P2 | M2b | 已登记待修 | 仍成立 | OPEN | — | manifest 的 nrej_tiles/nused_tiles 恒取叶级 tile 总数，与同文件 products 清单的 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M2b_L03_L15.｜:aio_hips_finalize 的 manifest 段（prods[] 表与 fprintf 参 |
| M2b-C-07 | C_DOC_CODE_GAP | P2 | M2b | 已登记待修 | 仍成立 | OPEN | — | 注册表与 ALG 内的事实性数量/规模断言与当前代码不符（ACS_FIO_ERR 17 码 vs 实际 13；p3_wcs.cp | 问题扫描/findings/C_DOC_CODE_GAP/p2/M2b_L03_L15.｜:D.fits 清单行 5（错误语义行的偏差列）；docs/algorithms/PHASE3_PROJ |
| M3-C-013 | C_DOC_CODE_GAP | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | `pc_calibrate_simple` 的返回码合同与实际返回值不符（`-3` 永不可达、同一条件两处不同码） | 问题扫描/findings/C_DOC_CODE_GAP/p2/M3_L05_L07.m｜:返回码合同`（复核时点 :101-104）；`lib/photometric_calib/cpp/sr |
| M3-C-014 | C_DOC_CODE_GAP | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | ALG 把两处串行循环写成「OpenMP 并行」，与代码 `#pragma` 分布不符 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M3_L05_L07.m｜:F2.1 步骤1 行`（复核时点 :81「步骤1（每帧 n，OpenMP parallel for）[ |
| M4-C-08 | C_DOC_CODE_GAP | P2 | M4 | 已登记待修 | 仍成立 | OPEN | — | UPM 接口面：normalized_weights 把 rc=2 塌缩为 rc=1；calibrate_block 未检 le | 问题扫描/findings/C_DOC_CODE_GAP/p2/M4_L08_L09.m｜:p2_upm_normalized_weights（复核时点 :1333）；::p2_upm_cali |
| M4-C-09 | C_DOC_CODE_GAP | P2 | M4 | 已登记待修 | 仍成立 | OPEN | — | ERR-P2-UPM-001 双文档锚互异且均偏离现实；NO_DATA 字面量无实现；AIO/phase2 format 接受面 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M4_L08_L09.m｜85,:89；docs/architecture/ERROR_MODEL.md:53；lib/phase |
| M6a-C-004 | C_DOC_CODE_GAP | P2 | M6a | 已登记待修 | 部分修复 | OPEN | — | 部分修复残余：drizzle 合同词表仍把 nested 写成「0=RING 1=NESTED」并列、DLL 面缺省仍取 0，与 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M6a_L13_L14.｜:DRZ_CFG_KEY_NESTED`（复核时点 :55 词表注释 `/* 0=RING 1=NEST |
| M7-C-201 | C_DOC_CODE_GAP | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | ALG 称 `bitpix=−64` 输出 FP64 精度，而该通道上游源恒 float32 ⇒ 声明的精度增益不存在（L20- | 问题扫描/findings/C_DOC_CODE_GAP/p2/M7_C_DOC_COD｜§/bitpix 块`（复核时 :38/:76/:86/:121 命中 bitpix） |
| M8-C-003 | C_DOC_CODE_GAP | P2 | M8 | 已登记待修 | 仍成立 | OPEN | — | `lib/plate_solve/README.md` 称投票/选星为「整数归并 + 静态调度，输出 bitwise 与线程数无 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M8_L22_L23.m｜:算法要点`（复核时点 :156「投票/选星（整数归并+静态调度，输出 bitwise 与线程数无关…」 |
| M8a-C-007 | C_DOC_CODE_GAP | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | dependency-lock 声称「无 ACS_ZLIB_ROOT 则 cfitsio 降级不编译 zcompress」，与根 | 问题扫描/findings/C_DOC_CODE_GAP/p2/M8a_L25_L27.｜:system_dependencies[zlib].policy`（复核 48）；`cmake/cfi |
| M8a-C-008 | C_DOC_CODE_GAP | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | providers/cpu/common README 公共符号表缺一个真实导出函数，状态词用词表外的 FROZEN | 问题扫描/findings/C_DOC_CODE_GAP/p2/M8a_L25_L27.｜:题头/状态行`（复核 3-4）、`::§5 符号表`（复核 61-72）、`::尾注`（104-111 |
| M8a-C-009 | C_DOC_CODE_GAP | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | test-only oracle 依赖群未入锁：astropy-healpix / scipy / rcr 三例，且 oracl | 问题扫描/findings/C_DOC_CODE_GAP/p2/M8a_L25_L27.｜:test_only_oracles`（复核 85-133）；`DEPENDENCIES.md::tes |
| V1-N-03 | C_DOC_CODE_GAP | P2 | V1-brief | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | snr_estimator.h 接口文本自斥：同名字段两值 | 问题扫描/findings/C_DOC_CODE_GAP/p2/V1-brief.md |
| V1-N-05 | C_DOC_CODE_GAP | P2 | V1-brief | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | DATA-P1-FLUX 单位三口径未收敛 | 问题扫描/findings/C_DOC_CODE_GAP/p2/V1-brief.md |
| V1-N-06 | C_DOC_CODE_GAP | P2 | V1-brief | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | v1 导出未初始化新字段且退化哨兵三套不一（含 N-06b 计数缺口） | 问题扫描/findings/C_DOC_CODE_GAP/p2/V1-brief.md |
| V1-N-07 | C_DOC_CODE_GAP | P2 | V1-brief | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 唯一钉退休口径的测试仍存在但因不在根图而永不运行 ⇒ 两条被改路径树内无活测试 | 问题扫描/findings/C_DOC_CODE_GAP/p2/V1-brief.md |
| V1-N-11 | C_DOC_CODE_GAP | P2 | V1-brief | 已修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/FIXED | — | 逐源重建轮廓的串行 CPU 重算风险（§17.6/§10.5 候选，数值需实测） | 问题扫描/findings/C_DOC_CODE_GAP/p2/V1-brief.md |
| V3-N-01 | C_DOC_CODE_GAP | P2 | V3 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | 新增的失败码 -14 未登记进公共 API 返回码合同（本批修复引入的同源缺口） | 问题扫描/findings/C_DOC_CODE_GAP/p2/V3.md｜:233-239`（帧通道返回码清单，逐码带锚 -1..-8 / -9 / -12 / -13）；生产者 |
| V3-N-02 | C_DOC_CODE_GAP | P2 | V3 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | plugin 面把 -14 折叠进默认域，DATA 语义在第二出口丢失 | 问题扫描/findings/C_DOC_CODE_GAP/p2/V3.md｜862-866`；语义源 `hp_drizzle_api.cpp:992-997` |
| V3-N-03 | C_DOC_CODE_GAP | P2 | V3 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | upm-fit 读旧版采样 artifact 缺 `control_grid_per_tile` 键时静默按 8 ⇒ 跨版本面的 | 问题扫描/findings/C_DOC_CODE_GAP/p2/V3.md｜245`（`grid!=8 → rc=3`）与 `:1061-1065`（open 校验） |
| V5-N-01 | C_DOC_CODE_GAP | P2 | V5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 长路径修复把「静默塌缩」换成「执行期拒绝」，但 validate/plan/create 仍放行 ⇒ 校验与执行判据不一致 | 问题扫描/findings/G_GOV_GATE/p2/V5.md｜:execute:786-788`（`head[512]`、`head_end=head+511`，触边 |
| L28b-D-004 | D_COMMENT | P2 | L28b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 细分阈 1.25 的溯源括号被清空为 []，同值在 ALG 文档里是另一个常量（HP_CIRCUMRADIUS_FACTOR）： | 问题扫描/findings/D_COMMENT/p2/L28b.md｜369（常量与空括号）、:590（唯一读取点 local_max / local_min <= ADAP |
| L28c-D-002 | D_COMMENT | P2 | L28c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | bench_report.h 自称「预冻结规则唯一出处」的三个门限常数（3 warmup / 7 measure / MAD÷m | 问题扫描/findings/D_COMMENT/p2/L28c.md｜36-42（并 :85 与实现 lib/backend_host/bench_report.cpp:85 |
| L28d-D-002 | D_COMMENT | P2 | L28d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 边细分阈值块注释仍写整改前口径（×1e-12 / 1.6e-14 rad / 每边 ~256 段），与同函数 20 行后的「新阈 | 问题扫描/findings/D_COMMENT/p2/L28d.md｜550-555（函数上方主说明块）；被否证的实现 :576-577；同文件已订正口径 :565、:572 |
| L28d-D-003 | D_COMMENT | P2 | L28d | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | CD 阻尼过渡带的上限注释「1e-4 → ~22」与实现的对数插值上限 1e6 相差约 4.5 个数量级，同函数两处注释互斥 | 问题扫描/findings/D_COMMENT/p2/L28d.md｜613-617（函数上方说明块）；被否证实现 :618-629（log10 插值）；同函数内已订正口径  |
| L28e-D-002 | D_COMMENT | P2 | L28e | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | COMMENT_STANDARD 的推荐写法示例自带两个不可解析 ID（示例即伪锚） | 问题扫描/findings/D_COMMENT/p2/L28e.md｜35-40（「科学代码推荐写法」代码块，:38 为示例注释行）；对照条文同文件 :10（必须注释第 1  |
| M1a-D-002 | D_COMMENT | P2 | M1a | 已登记待修 | 仍成立 | OPEN/STILL-WAITING-UNTRACKED-LOCK | — | 科学链路单位注释自矛盾群（重锚定稿）：StarPoint/U 注"角秒"而实际为像素、SIP order 值域注释过期、x00  | 问题扫描/findings/D_COMMENT/p2/M1a_L01_L02.md｜:StarPoint / ::IpvWcsResult；docs/algorithms/PLATESOL |
| M1a-D-003 | D_COMMENT | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | 注释卫生：头注与被其废止的非目标并存、审计流水堆积、合同式注释与代码矛盾（多文件） | 问题扫描/findings/D_COMMENT/p2/M1a_L01_L02.md｜:文件头注释；lib/phase3_session/p3_wcs.h:::25-27/:43-44；li |
| M2a-D-1 | D_COMMENT | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | 极区尺度常数写成 211034.6（真值 ≈211076.3），配套注释给出与代码不同的裕量系数 | 问题扫描/findings/D_COMMENT/p2/M2a_L04_L10.md｜:HEALPIX_SCALE_PER_NSIDE_ARCSEC 注释簇；docs/algorithms/ |
| M2a-D-2 | D_COMMENT | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | 累加器字段数与位宽注释三处互斥（3 字段/20B→12B vs 4 字段实现 vs 迁移面「6 个 f64」） | 问题扫描/findings/D_COMMENT/p2/M2a_L04_L10.md｜:TileAccumulatorT 注释与结构体；::processPixel 形参注释；lib/dri |
| M2a-D-3 | D_COMMENT | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | 生产代码注释堆积任务号/审计流水，违 §12.2 的注释白名单 | 问题扫描/findings/D_COMMENT/p2/M2a_L04_L10.md｜:drizzleTiled（P1-DRZ-NONFINITE 段）；::spherical_overla |
| M2a-D-4 | D_COMMENT | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | Gaia 侧注释/断言簇：算法名与实现不符、扩展函数声明无对应实现、已修缺陷仍留过期弱断言与恒真断言 | 问题扫描/findings/D_COMMENT/p2/M2a_L04_L10.md｜:文件头能力注释；lib/gaia_xpsd_client/CMakeLists.txt::libm 扩 |
| M3-D-004 | D_COMMENT | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | `master_generator.cpp` 注释引用不存在的合同小节，并以任务号流水充当契约依据 | 问题扫描/findings/D_COMMENT/p2/M3_L05_L07.md｜:generate_master_flat 步骤3 前注释`（复核时点 :171-175）；`::gen |
| M3-D-005 | D_COMMENT | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | cosmetic 注释与 ALG 描述一个不存在的 label-0 防误清机制，并把 method=1 写成 8 邻居双线性 | 问题扫描/findings/D_COMMENT/p2/M3_L05_L07.md｜:文件头与 detect_hot/cold 注释`（复核时点 :14、:59-60；实际守卫 `labe |
| M3-D-006 | D_COMMENT | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | `star_matcher.cpp`/`pc_api.cpp` 以任务号流水作注释主体，且三处科学表述与实现相反 | 问题扫描/findings/D_COMMENT/p2/M3_L05_L07.md｜:常量与阶段注释`（复核时点 :21-27、:173、:197/:209/:222/:240、:300/ |
| M3b-D-01 | D_COMMENT | P2 | M3b | 已登记待修 | 仍成立 | OPEN | — | 科学量注释卫生系统性欠账：断句残句、修改流水、思考式推导、公共头零单位契约、错误系数注释 | 问题扫描/findings/D_COMMENT/p2/M3b_L06.md｜171,:185-188,:207,:285-290,:313-326,:330-331,:339/:3 |
| M4-D-03 | D_COMMENT | P2 | M4 | 已登记待修 | 仍成立 | OPEN | — | §12.2 卫生根因条目：任务编号/审计流水/残断 token/失实注释簇（L08+L09 全实例合并） | 问题扫描/findings/D_COMMENT/p2/M4_L08_L09.md｜:符号/行，复核时点；实例列举） |
| M5a-D-002 | D_COMMENT | P2 | M5a | 已登记待修 | 仍成立 | OPEN | — | 实现与检查器注释堆积任务号/控制包章节，活动 ARCH 文档写入提交哈希与历史 PASS 计数 | 问题扫描/findings/D_COMMENT/p2/M5a_L11.md｜:MON-002 阈值注释`（:90-99）、`::GateConfig::active_window_ |
| M6a-D-007 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 同一公共头内 PSF 拟合状态被两套互斥口径注释（质量位「status==0 或 3 有效」vs 逐星剔除「status!=0  | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:SnrQualityFlagBits`（复核时点 :337 `SNR_QF_PSF_OK = 1u < |
| M6a-D-008 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 同一科学合同（SCI-NOISE-001）在两个数值不同的实现上各挂「唯一真实入口 / 现状唯一生产实现」宣称，注释层互斥；本条 | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:op: estimate_snr 前言`（复核时点 :2142 区段「唯一真实入口 NoiseMode |
| M6a-D-009 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN/STILL-SUBITEM-EXPIRED | — | §12.2 禁止形态（任务号/审计轮次/commit 哈希/修复流水）成为注释主干：Phase2/3 侧六目录实测 257 处命 | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜`lib/plate_solve/cpp/ipv/src/ipv_sip.cpp`（:29/:171/: |
| M6a-D-011 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | cosmetic 修复核注释三说并存：函数头写「8 邻居双线性」、体内写「4 方向距离反比」、权威合同为 4 正交方向 IDW（ | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:interpolate_pixels 前言`（复核时点 :157-159「method=0 (medi |
| M6a-D-012 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | registry 声明的 per-projection 守卫字段无任何强制点：注释称「中心 /dec/ 守卫/合法 FOV 声明 | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:P3ProjectionSpec`（复核时点 :44-47 六要素声明「max_abs_crval_d |
| M6a-D-013 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 热路径死代码被 ALG 当「实测」固化：pix2ang 结果只被 (void) 压制、(void)y1 压制的是实际被使用的变量 | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:p3_sample_bilinear_ex`（复核时点 :174-176 计算 `ipix` 后 `p |
| M6a-D-014 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 同一文件内的并发注释互斥：一处要求「所有 aio open/read 经单一全局 mutex 串行化」并在实现里这么写，另一处写 | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:CON-010 复归注释`（复核时点 :157-160「cfitsio / aio_hips 并发文件 |
| M6a-D-015 | D_COMMENT | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 浏览器后端头注释把已废弃的私有 tile 线性约定 [y*512+x] 写成现行契约，与唯一权威映射及自身实现相反；同头另把二进 | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md｜:文件头`（复核时点 :2/:4-5「正式 Browser 数据源…仅通过 astro_image_io |
| M9-D-1 | D_COMMENT | P2 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | drizzle 局部尺度推导注释把"量纲为一的切平面坐标"表述为"仍是弧度量级小值"，注释表述失准而实现结果正确 | 问题扫描/findings/D_COMMENT/p2/M9_L24_L26.md｜:local_scale_3d_tangent（注释块与换算步）、::局部尺度调用点（:568 邻域） |
| M9-D-2 | D_COMMENT | P2 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | 公共查询接口 radius_deg 与 match_radius_arcsec 双口径并存、GaiaStar 裸字段无单位、sc | 问题扫描/findings/D_COMMENT/p2/M9_L24_L26.md｜:GaiaStar（裸字段）与查询族声明（radius_deg ×4 / match_radius_ar |
| V6-N-03 | D_COMMENT | P2 | V6 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 新写注释两处不实自述：把符号宿主指向根本没有该符号的文件，并把两行宏值比较说成 ABI 形参证明 | 问题扫描/findings/D_COMMENT/p2/V6.md |
| V8-N-03 | D_COMMENT | P2 | V8 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·判据①自报计数）README「4 处计算」与它自己括号里列的 5 个名字互相否定 | 问题扫描/findings/D_COMMENT/p2/V8.md |
| V8-N-06 | D_COMMENT | P2 | V8-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·判据②）`gaia_client.h` 公共头契约锚指向**三个不存在的节名** | 问题扫描/findings/D_COMMENT/p2/V8-b.md |
| V8-N-08 | D_COMMENT | P2 | V8-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P3·判据②）角度归一化契约**区间端点写错**：头注释 `(-90, 90]` 左开，而 `-90` 是可达返回值 | 问题扫描/findings/D_COMMENT/p2/V8-b.md |
| W1-N-08 | D_COMMENT | P2 | W1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`gaia_client.c:31-40` 新注释的实测包络**口径错报**：称 high 属于 [16.59, 25. | 问题扫描/findings/D_COMMENT/p2/W1.md |
| W1-N-09 | D_COMMENT | P2 | W1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`gaia_client.c` 的 `parse_attr_after` 新 docstring（`:853-856`） | 问题扫描/findings/D_COMMENT/p2/W1.md |
| W1-N-11 | D_COMMENT | P2 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`module_adapters.cpp:3170-3178` 欠采样告警在 **auto 分支**也触发，且文案硬编码 | 问题扫描/findings/D_COMMENT/p2/W1-b.md |
| W1-N-13 | D_COMMENT | P2 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`drizzle_engine.cpp` 的 p22 剖面行 `out_sort=%.3f` **恒传字面量 0.0** | 问题扫描/findings/D_COMMENT/p2/W1-b.md |
| L28e-E-004 | E_TRACE_BREAK | P2 | L28e | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | ALG-ANCHOR-001 标 ACTIVE_NORMATIVE 却是自指孤锚：全仓唯一宿主是它自己的声明行 | 问题扫描/findings/E_TRACE_BREAK/p2/L28e.md｜3；同头并列的任务号 SCI-ANCHOR-001 的宿主 tests/quality/test_doc |
| L28e-E-005 | E_TRACE_BREAK | P2 | L28e | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | SCI-ASTROMETRY-001 与 SCI-PHOTOMETRY-001 是 STAR_DETECTION.md 自造的全 | 问题扫描/findings/E_TRACE_BREAK/p2/L28e.md｜32、:35、:61；正主 SCI-AST-001（docs/science/ASTROMETRY.md |
| M1a-E-004 | E_TRACE_BREAK | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | 同一测试 ID 在 SCI 与 ALG 指向不同语义（TST-WCS-001 / TST-WCS-INV） | 问题扫描/findings/E_TRACE_BREAK/p2/M1a_L01_L02.m｜:§8 测试族表；docs/science/ASTROMETRY.md::§15 关联 ID 列 |
| M2a-E-5 | E_TRACE_BREAK | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | DATA-002 合同的「负测映射」列出三个树内不存在的测试函数名 | 问题扫描/findings/E_TRACE_BREAK/p2/M2a_L04_L10.m｜:负测映射表；tests/artifact/test_phase_product_exchange.py |
| M5b-E-06 | E_TRACE_BREAK | P2 | M5b | 已登记待修 | 仍成立 | OPEN | — | ERROR_MODEL 规定的阶段 ID 词表在实现中无任何产出、缺 Phase3，并引用不存在的标准文件 | 问题扫描/findings/E_TRACE_BREAK/p2/M5b_L12_L17.m｜:阶段 ID 词表`（现 :10-13，`P1.READ/P1.STARS/P1.WCS/P1.PHOT |
| M6a-E-002 | E_TRACE_BREAK | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 权威星点检测实现把上游 C 源文件行锚当公式出处：sdet_api.cpp 内 39 处 `star_finder.c:NNN` | 问题扫描/findings/E_TRACE_BREAK/p2/M6a_L13_L14.m｜195 `// 圆度, 1.0]（star_finder.c:105-108)`、:2283 `// m |
| M6b-E-005 | E_TRACE_BREAK | P2 | M6b | 已登记待修 | 仍成立 | OPEN | — | 追溯合同自身与实现不符：8 层 vs 9 行、唯一性声明互斥、schema 路径不存在、PASS 输出契约缺 rows=、CI  | 问题扫描/findings/E_TRACE_BREAK/p2/M6b_L16_L18.m｜:§1/§2/§3/§7`（现 :20、:30、:33-43、:86、:129-139、:159）、`d |
| M6b-E-006 | E_TRACE_BREAK | P2 | M6b | 已登记待修 | 仍成立 | OPEN | — | 控制面合同把权威层级锚在被废止控制包文档与已 supersede 的约束文件上，并引用不存在的 `$F.2` | 问题扫描/findings/E_TRACE_BREAK/p2/M6b_L16_L18.m｜:文首权威声明 + §5`（现 :6-8、:135-136）、`AstroCS_ENGINEERING_ |
| M6b-E-007 | E_TRACE_BREAK | P2 | M6b | 已登记待修 | 仍成立 | OPEN/STILL | — | 合同文件清单与树内实存不符：IO-001 冻结合同宣称的 `modules/services/io/` 骨架 4 项中 3 项不 | 问题扫描/findings/E_TRACE_BREAK/p2/M6b_L16_L18.m｜:§3 内容/路径表`（现 :24-32）、`docs/contracts/INDEX.yaml::DA |
| M7-E-201 | E_TRACE_BREAK | P2 | M7 | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | SCI/合同/注释三处「与实现一致」声明所引代码行锚整段漂移（本代理读码亲证） | 问题扫描/findings/E_TRACE_BREAK/p2/M7_E_TRACE_BR |
| M7-E-202 | E_TRACE_BREAK | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | 文档以绝对行号自引，插入即漂移，使同文「见 :N」指向错误行（L19-029） | 问题扫描/findings/E_TRACE_BREAK/p2/M7_E_TRACE_BR｜§2 尾`（:32 自引 `REJECTION.md:16`；复核时 :16 实为 §1 内容），同篇  |
| M8-E-001 | E_TRACE_BREAK | P2 | M8 | 已登记待修 | 仍成立 | OPEN | — | `lib/hips_p2/` 只有文档三件套（无源文件），追溯锚实指 `lib/phase2/tools/stage2.cpp` | 问题扫描/findings/E_TRACE_BREAK/p2/M8_L22_L23.md｜:构建`（:6-9 指 `lib/phase2/tools/stage2.cpp`（自称 1762 行实 |
| M8a-E-003 | E_TRACE_BREAK | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | DISP-PSF-007 被 README/manifest/测试/HANDOVER 四处当偏差 ID 使用，偏差登记表只到 0 | 问题扫描/findings/E_TRACE_BREAK/p2/M8a_L25_L27.m｜:文档版本行`（4）、`lib/dynamic_psf/module.yaml::偏差注`（136）、` |
| M8a-E-004 | E_TRACE_BREAK | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | README 路径引用簇：跨目录引用无解析前缀、签名头指错目录、行锚落在邻近 target | 问题扫描/findings/E_TRACE_BREAK/p2/M8a_L25_L27.m｜:现状段`（复核 7-8、23）、`lib/hips/README.md::测试引用`（复核 172）、 |
| SA-N-01 | E_TRACE_BREAK | P2 | SA | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）悬空锚主口径：**3135 出现 / 1008 种**，其中裸文件名 570 种 / 2278 出现（72.7%）⇒ 全 | 问题扫描/findings/E_TRACE_BREAK/p2/SA.md |
| SA-N-02 | E_TRACE_BREAK | P2 | SA | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）凭空符号 **2 处**（我写下的）：`ipv_wcs.cpp::build_fits_wcs_from_solutio | 问题扫描/findings/E_TRACE_BREAK/p2/SA.md |
| SA-N-03 | E_TRACE_BREAK | P2 | SA | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）三类结构性悬空：自指档案 94 种 / 272 出现、未入库影子树 13 种 / 34 出现、gitignored 运行 | 问题扫描/findings/E_TRACE_BREAK/p2/SA.md |
| SA-N-04 | E_TRACE_BREAK | P2 | SA-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）门禁与接口裁决的权威依据文本在 HEAD 不可解析：控制包标准 8 种 / 21 出现 | 问题扫描/findings/E_TRACE_BREAK/p2/SA-b.md |
| SA-N-05 | E_TRACE_BREAK | P2 | SA-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）硬底线：硬悬空 48 种 / 55 出现 + 产物名 34 种 / 75 出现 + 外链未外部化 7 种 / 18 出现 | 问题扫描/findings/E_TRACE_BREAK/p2/SA-b.md |
| M1a-F-006 | F_TEST_GAP | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | p3_interp/p3_coverage 单元级"自测"以测试自带参考实现为对照，却被合同当作覆盖语义证据 | 问题扫描/findings/F_TEST_GAP/p2/M1a_L01_L02.md｜:bilinear（测试内私有参考实现）；tests/unit/p3_coverage_test.cpp |
| M2a-F-5 | F_TEST_GAP | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | 名为 test_drizzle_oracle / test_drizzle_parallel 的合成 Oracle 测试实为平面 | 问题扫描/findings/F_TEST_GAP/p2/M2a_L04_L10.md｜:模块 docstring 与 kernel 表；lib/backend_host/baseline_k |
| M2a-F-6 | F_TEST_GAP | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | drizzle 输出像素间噪声相关性的实测数值仅存文档，代码侧只做恒真式打印、无断言且取样口径未披露 | 问题扫描/findings/F_TEST_GAP/p2/M2a_L04_L10.md｜:相关性统计段（n_pairs / corr_mean / corr_max）；docs/science |
| M5b-F-02 | F_TEST_GAP | P2 | M5b | 已登记待修 | 仍成立 | OPEN | — | tests/cli 的 TestManifestVerify 类定义在 __main__ 守卫之后，直跑时静默不执行 | 问题扫描/findings/F_TEST_GAP/p2/M5b_L12_L17.md｜:（if __name__ 与后续类定义）`（现 :230-233 及其后） |
| M6a-F-001 | F_TEST_GAP | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 唯一权威 HEALPix 核心的 CONFORMANT 证据行指向一个未挂 ctest/CI、且需仓外输入文件的测试：头注释的「 | 问题扫描/findings/F_TEST_GAP/p2/M6a_L13_L14.md｜:文件头 来源声明`（复核时点 :5-9「…由 astropy-healpix (BSD-3-Claus |
| M7-F-201 | F_TEST_GAP | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | 冻结容差以文字或占位充当，且「构造保证」在 FP64 下并非逐位命题（L20-026） | 问题扫描/findings/F_TEST_GAP/p2/M7_F_TEST_GAP_p2｜§5c/§8`（复核时 :86/:105「构造保证」）、`docs/algorithms/PHASE3_ |
| M8-F-014 | F_TEST_GAP | P2 | M8 | 已登记待修 | 仍成立 | OPEN | — | ISA-002「AVX 永不优于已发布 AVX2」用例只断言 assertTrue(True)，且判据 csv 由测试自己写出（ | 问题扫描/findings/F_TEST_GAP/p2/M8_L22_L23.md｜:TestIsaAvx::test_05_avx_never_beats_shipped_avx2`（* |
| M8-F-015 | F_TEST_GAP | P2 | M8 | 已登记待修 | 仍成立 | OPEN | — | 恒真断言面未被任何机器门覆盖：现存 5 处 `CHECK(true)`（含冒名不变量门）；`CHECK(0 == 0)` 写法已 | 问题扫描/findings/F_TEST_GAP/p2/M8_L22_L23.md｜125）、`tests/unit/p2_output_semantics_test.cpp`（:136） |
| M8a-F-001 | F_TEST_GAP | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | 本轮两处并发修复（RCR 版本口径统一、toolchain.lock 刷新）均无回归断言，同类漂移必然复发 | 问题扫描/findings/F_TEST_GAP/p2/M8a_L25_L27.md｜:docstring`（6-21）；`lib/phase2/tools/rcr_oracle_compa |
| M9-F-3 | F_TEST_GAP | P2 | M9 | 已登记待修 | 无逐条处置表（M9 §1 定稿清单） | OPEN | — | provenance 双键的 pixfrac 半有界、scale 半无界；在册断言把亚弧秒取值钉成合法基线，生产真实值域（0.1 | 问题扫描/findings/F_TEST_GAP/p2/M9_L24_L26.md｜:aio_hips_set_drizzle_provenance；声明面 lib/astro_image |
| V15-N-10 | F_TEST_GAP | P2 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·③析取式 reason）CPU-005 路由回退原因只断"串里含 X 或 Y"，而 **`invalid` 几乎任何错误 | 问题扫描/findings/F_TEST_GAP/p2/V15.md |
| V15-N-11 | F_TEST_GAP | P2 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·②自建自证第二实例）`test_isa_variants.py`：`.inc` 文本锁（同一不变量的 Python 侧第 | 问题扫描/findings/F_TEST_GAP/p2/V15.md |
| V15-N-12 | F_TEST_GAP | P2 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·④正向在册冒充"已实现"）`test_p3006:133-139` docstring 自称验「状态 IMPLEMENT | 问题扫描/findings/F_TEST_GAP/p2/V15.md |
| V15-N-13 | F_TEST_GAP | P2 | V15 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·④枚举白名单 + 键存在 + 产物存在）`test_p2007:234-250`/`:281-284`：`verdict | 问题扫描/findings/F_TEST_GAP/p2/V15.md |
| W1-N-07 | F_TEST_GAP | P2 | W1 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）本区间被改的 `test_last_inlier_reset.cpp`（`lib/plate_solve/cpp/ipv | 问题扫描/findings/F_TEST_GAP/p2/W1.md |
| W1-N-10 | F_TEST_GAP | P2 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）新增只写不读面：`p1drz_thread_probe.cpp` 写 `<prefix>.order`，但**两把新锁都 | 问题扫描/findings/F_TEST_GAP/p2/W1-b.md |
| W1-N-12 | F_TEST_GAP | P2 | W1-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`HEALPIX_SCALE_PER_NSIDE_ARCSEC` 双实现，无单一承载层 | 问题扫描/findings/F_TEST_GAP/p2/W1-b.md |
| FD-G-003 | G_GOV_GATE | P2 | FD | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 基线文件自陈「当前 linux-main 有约 10 项红灯且不得登记进基线」——其中含本审计定为「结构性空壳」的 CON-CO | 问题扫描/findings/G_GOV_GATE/p2/FD_shadow_agents｜:contract.excluded_by_policy` |
| M1a-G-003 | G_GOV_GATE | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | 同一校验条件在请求层/会话层/描述符层内联重复，未接单一机器源 | 问题扫描/findings/G_GOV_GATE/p2/M1a_L01_L02.md｜:（`\/dec\/ > 85` 内联）；lib/phase3_session/p3_wcs.cpp:: |
| M2a-G-2 | G_GOV_GATE | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | DATA 合同 ID 闭包检查器的正则只覆盖带 -NNN 后缀的形态，约半数在册 DATA-* 落在门外，而合同自称「全部登记、 | 问题扫描/findings/G_GOV_GATE/p2/M2a_L04_L10.md｜:DATA_RE 与扫描/比对段；docs/contracts/DATA_ARTIFACTS.md::§ |
| M2a-G-3 | G_GOV_GATE | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | 模块版本 / ABI 版本 / 产品 VERSION 三处共用同一字面量 0.11.0-alpha.2，违 §16.1 版本命名 | 问题扫描/findings/G_GOV_GATE/p2/M2a_L04_L10.md｜:module_version；lib/gaia_xpsd_client/include/astrocs |
| M4-G-02 | G_GOV_GATE | P2 | M4 | 已登记待修 | 仍成立 | OPEN | — | 整阶段 Session 以 astrocs.phase2.resample 假身份驻留模块注册表，占位 ID 无权威页 | 问题扫描/findings/G_GOV_GATE/p2/M4_L08_L09.md｜:phase2_descriptor（复核时点 :484-501）与注册段 register_phase |
| M6b-G-007 | G_GOV_GATE | P2 | M6b | 已登记待修 | 仍成立 | OPEN | — | 宪章点名的 L0 文档与分层元描述互相矛盾：`docs/owner/PHASE_OVERVIEW.md` 不存在，两套"当前分层 | 问题扫描/findings/G_GOV_GATE/p2/M6b_L16_L18.md｜:§12.1`（现 :414-418）、`docs/README-DOCS.md::L0-L5 表 +  |
| M6b-G-008 | G_GOV_GATE | P2 | M6b | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 冻结面表述与实际状态并存冲突：Phase3 四投影集合新旧两版同时流通 | 问题扫描/findings/G_GOV_GATE/p2/M6b_L16_L18.md｜:§0 非目标 / §11 / §15 冻结四投影表`（现 :20、:190、:348-:370）、`d |
| M8a-G-009 | G_GOV_GATE | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | vendored json-schema-validator 无许可全文、无登记，版本只活在 Makefile 注释（裁决 P1 | 问题扫描/findings/G_GOV_GATE/p2/M8a_L25_L27.md｜:json-validator.cpp`（4-6）、`::smtp-address-validator. |
| M8a-G-010 | G_GOV_GATE | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | cfitsio 完整性证明只覆盖 60 源 + 1 头，且上游 4.7.0 安全修复无跟进评估记录 | 问题扫描/findings/G_GOV_GATE/p2/M8a_L25_L27.md｜:cfitsio.provenance_note/file_sha256/aggregate_sha25 |
| V11-N-03 | G_GOV_GATE | P2 | V11 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `GaiaSpectrumStar` 旧镜像 ⇒ **不越界但静默错位**：重投影验证图除第 1 颗星外坐标皆错 | 问题扫描/findings/G_GOV_GATE/p0/V11.md｜57-66` = **5 字段**（`+flux_min/+flux_mul`，由 `70edb842` |
| V13-N-05 | G_GOV_GATE | P2 | V13-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/STILL | — | 登记面零宿主新口径计数：**28 枚非登记面零宿主**（SCI 侧 14 为本层新案；此口径不与八级阶梯引用度相加） | 问题扫描/findings/G_GOV_GATE/p1/V13-b.md |
| V13-N-07 | G_GOV_GATE | P2 | V13-b | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 制度缺口：登记面对新编号**零承载要求** ⇒ 「新造未登记」在制度上无可违之规（附一条 SUPERSEDED 自毁） | 问题扫描/findings/G_GOV_GATE/p1/V13-b.md |
| V13-N-08 | G_GOV_GATE | P2 | V13-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 工作树根目录遗留 p8..p11-files.patch（归批提醒，非本层所为） | 问题扫描/findings/G_GOV_GATE/p1/V13-b.md |
| V14-N-07 | G_GOV_GATE | P2 | V14-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 四个新 `SNR_API` 导出在**第三登记面**（`module.yaml` 的 exports/source_symbol | 问题扫描/findings/G_GOV_GATE/p1/V14-c.md |
| V16-N-01 | G_GOV_GATE | P2 | V5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `ci/known_failures.json` 的两条 waiver 依据指向 gitignored 工作区 ⇒ "红被制度化 | 问题扫描/findings/G_GOV_GATE/p2/V5.md｜66`（`"run/ci/ci-baseline-001/ctest_full_pre.log（当前门卫 |
| V19-N-09 | G_GOV_GATE | P2 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）7 道门 changed_paths 与 5 条 impact rule 是零 tracked 命中的死模式（3 条正落 | 问题扫描/findings/G_GOV_GATE/p2/V19.md |
| V19-N-10 | G_GOV_GATE | P2 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）outputs 与 dirty_ignore 与 .gitignore 三面零联动：两条半登记产物既不能提交也不被忽略 | 问题扫描/findings/G_GOV_GATE/p2/V19.md |
| V19-N-11 | G_GOV_GATE | P2 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）glob 型输入零校验：CI-BINDING-TESTS 的 -p 模式若被改到匹配 0 文件，门永久绿且两个守卫都不触 | 问题扫描/findings/G_GOV_GATE/p2/V19.md |
| V19-N-12 | G_GOV_GATE | P2 | V19 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （负结果 · 不立条，供邻站定性与防误报） | 问题扫描/findings/G_GOV_GATE/p2/V19.md |
| V5-N-02 | G_GOV_GATE | P2 | V5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 「全部拼接点带容量」的修复声明覆盖面不实：gaia_plan 仍是同型无界推进（当前有界、无豁免注释） | 问题扫描/findings/G_GOV_GATE/p2/V5.md｜:gaia_plan:554`（`w += snprintf(w, 剩余容量)` 同模式）；修复声明见账 |
| V5-N-04 | G_GOV_GATE | P2 | V5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 新剪枝的科学口径五处同源全缺：文档 0 行、0.25 裕量的权威文件不版本化、测试 0 触达 | 问题扫描/findings/G_GOV_GATE/p2/V5.md｜fixture 星等窗恒 `10.0,20.0` ⇒ `10 > 20+0.25` 恒假，永不剪，`ga |
| V9-N-11 | G_GOV_GATE | P2 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·设计即静默）`API-DOCS`、`UNIT-CLOSURE` 在 `ci/run.py:103 EMPTY_OUTPU | 问题扫描/findings/G_GOV_GATE/p2/V9-c.md |
| V9-N-13 | G_GOV_GATE | P2 | V9-c | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2·`--focus`/`--changed-from` 类 CI 口径漏门）`changed_paths` **99/13 | 问题扫描/findings/G_GOV_GATE/p2/V9-c.md |
| W5-N-10 | G_GOV_GATE | P2 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）退出码面三缺：`commands.cpp:306 rc=124`（超时）与 `:309 rc=127`（spawn 失败 | 问题扫描/findings/G_GOV_GATE/p2/W5.md |
| W5-N-11 | G_GOV_GATE | P2 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）诊断可定位性：`LOGGING_DIAGNOSTICS_STANDARD.md:9` 要求的「source」一环**无任 | 问题扫描/findings/G_GOV_GATE/p2/W5.md |
| W5-N-12 | G_GOV_GATE | P2 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）门面 Python 吞错的危险子集在**门自身**：`tools/quality/check_complexity.py | 问题扫描/findings/G_GOV_GATE/p2/W5.md |
| W5-N-13 | G_GOV_GATE | P2 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）解压重试**无字节封顶**：`aio_ahpx_reader.cpp:476`(ZSTD)/`:494`(LZ4) 以  | 问题扫描/findings/G_GOV_GATE/p2/W5.md |
| W5-N-14 | G_GOV_GATE | P2 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`drizzle_engine.cpp:1149`（直写）与 `:2275`（分块）同构 `try { hmeta.ga | 问题扫描/findings/G_GOV_GATE/p2/W5.md |
| W5-N-15 | G_GOV_GATE | P2 | W5 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）`aio_fits.cpp:389` 的通用数值关键字助手 `try{return stod(v);}catch{ret | 问题扫描/findings/G_GOV_GATE/p2/W5.md |
| M2a-H-3 | H_NUMERIC | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | HiPS 写侧把 support 静默钳到 1.0，层级归约用钳后的 support 反乘通量，覆盖>1 时破坏 §12.2 声 | 问题扫描/findings/H_NUMERIC/p2/M2a_L04_L10.md｜:write_tile_core（sup 归一与钳制）、::层级归约累加段；docs/contracts |
| M2a-H-4 | H_NUMERIC | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | variance 面 FLOAT64 输入被静默窄化为 FLOAT32 后送入引擎，与 signal 侧「f32/f64 二选一 | 问题扫描/findings/H_NUMERIC/p2/M2a_L04_L10.md｜:帧块读取与 variance 装配段；lib/healpix_db/healpix_drizzle/d |
| M2b-H-02 | H_NUMERIC | P2 | M2b | 已登记待修 | 仍成立 | OPEN | — | pix2ang_nest 对越界 ipix 静默返回 (0,0)（合法天球点），neighbors/xy 换算另有两处 shif | 问题扫描/findings/H_NUMERIC/p2/M2b_L03_L15.md｜:pix2ang_nest 声明注释；lib/common/healpix/healpix_core.c |
| M4-H-01 | H_NUMERIC | P2 | M4 | 已登记待修 | 仍成立 | OPEN | — | build_geo 的 obs.control_id ∉ nodes 时无裁决面：std::map operator[] 默认插 | 问题扫描/findings/H_NUMERIC/p2/M4_L08_L09.md｜:control_by_id/frame_index 预串行段与消费点（复核时点 :423-424,:5 |
| M8-H-001 | H_NUMERIC | P2 | M8 | 判定非缺陷 | 部分修复 | OPEN/NOT_A_DEFECT | — | `gaia_client.c` 的 GaiaTraceCtx 注释声称「只原子更新本查询上下文」，实为多线程共享同一栈对象上的裸 | 问题扫描/findings/H_NUMERIC/p2/M8_L22_L23.md｜:GaiaTraceCtx`（注释块 :42-45「…只**原子**更新本查询上下文…」；结构成员为裸  |
| V2-N-05 | H_NUMERIC | P2 | V2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | MAD 型门在对称/量化残差下可为 0 ⇒ 门恒不触发（fail-open），而 RMSE 版无此退化 | 问题扫描/findings/H_NUMERIC/p2/V2.md｜226`（判据 `1.4826·MAD/A>0.2`）与 `:441-445`（`result->mad |
| V2-N-06 | H_NUMERIC | P2 | V2 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 两处回归锁的自述失实：一条宣称的断言根本不存在，另一条是循环 oracle | 问题扫描/findings/H_NUMERIC/p2/V2.md |
| V2-N-07 | H_NUMERIC | P2 | V2 | 需负责人裁决 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | C-09 | 三处小残余（同批登记以免散落） | 问题扫描/findings/H_NUMERIC/p2/V2.md |
| M1a-I-001 | I_DOC_HYGIENE | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | 模块源目录滞留 V4 时代历史文档以现在时态呈现，并有运行残留物混入源码树 | 问题扫描/findings/I_DOC_HYGIENE/p2/M1a_L01_L02.m｜:头部结论段；lib/plate_solve/cpp/ipv/::（目录现状） |
| M1a-I-002 | I_DOC_HYGIENE | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | kMaxSide 以编译期宏可覆盖，为 SCI 冻结的 [1,20000] 上限留下放大口子（现状未使用） | 问题扫描/findings/I_DOC_HYGIENE/p2/M1a_L01_L02.m｜:（上限常量定义）；docs/contracts/DATA_SEMANTICS.md::§28（"编译期 |
| M1a-I-003 | I_DOC_HYGIENE | P2 | M1a | 已登记待修 | 仍成立 | OPEN | — | FITS 实现文档内部撕裂：fdatasum 三口径并存、签名滞后、旧状态注记未随整改订正 | 问题扫描/findings/I_DOC_HYGIENE/p2/M1a_L01_L02.m｜:§3/§6/§12/§14；docs/modules/…/p3 侧 README::状态段 |
| M2a-I-1 | I_DOC_HYGIENE | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | SCI-DRZ-001 的符号表与验证段落进了 Gaia 极区 prune 的专用常数（C=π/2、C45=π/(2√2)），d | 问题扫描/findings/I_DOC_HYGIENE/p2/M2a_L04_L10.m｜:§2 符号表、::§3a、::§10 表述行；对照 lib/healpix_db/healpix_dr |
| M2a-I-2 | I_DOC_HYGIENE | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | module.yaml 声明 determinism=fixed_reduction_order，同一模块合同另处声明输出行序依 | 问题扫描/findings/I_DOC_HYGIENE/p2/M2a_L04_L10.m｜:determinism；docs/algorithms/GAIA_QUERY.md::§2.9 第 5 |
| M2a-I-3 | I_DOC_HYGIENE | P2 | M2a | 已登记待修 | 仍成立 | OPEN | — | 数据接口合同把 ARCHIVED_NON_NORMATIVE 的归档约束文件列为现行上游权威 | 问题扫描/findings/I_DOC_HYGIENE/p2/M2a_L04_L10.m｜:约束来源行；docs/interfaces/data/DATA-002_*、DATA-004_*（同族 |
| M3-I-003 | I_DOC_HYGIENE | P2 | M3 | 已修 | 仍成立 | OPEN/FIXED-RESIDUAL | — | `lib/photometric_calib/docs/algorithm.md` 以「状态：已实现」呈现，且其两处核心事实被  | 问题扫描/findings/I_DOC_HYGIENE/p2/M3_L05_L07.md｜:文档头`（复核时点 :4、:6）、`::§1.1 目标`（:14）、`::§3 网格`（:107）、` |
| M3-I-004 | I_DOC_HYGIENE | P2 | M3 | 已登记待修 | 仍成立 | OPEN | — | p1phot 测试套件存在两份同名异体副本，未注册的一份自称「共址于 lib/phase1/tests/p1phot」 | 问题扫描/findings/I_DOC_HYGIENE/p2/M3_L05_L07.md｜:头注`（复核时点 :12-13）；对照 `lib/photometric_calib/tests/p1 |
| M3b-I-01 | I_DOC_HYGIENE | P2 | M3b | 已登记待修 | 部分修复 | OPEN/STILL | — | 数值卫生欠账：不可达守卫、同式两种除法、常数位数/取值笔误、积分式与字段数文字错 | 问题扫描/findings/I_DOC_HYGIENE/p2/M3b_L06.md｜:theta 归一与守卫(:392-403)、整数除法(:1943-1944) vs :667；s_fa |
| M5a-I-002 | I_DOC_HYGIENE | P2 | M5a | 已登记待修 | 部分修复 | OPEN | — | ISA_VARIANTS 同一文档内对 ISA-004 的评估平台口径自相矛盾（Windows 域 vs vm-bj 完整评估） | 问题扫描/findings/I_DOC_HYGIENE/p2/M5a_L11.md｜:§1.5 尾句`（:51）与 `::§1.6`（:53-67） |
| M5b-I-04 | I_DOC_HYGIENE | P2 | M5b | 已登记待修 | 仍成立 | OPEN | — | CHANGELOG 把上一轮次标为 Current Alpha | 问题扫描/findings/I_DOC_HYGIENE/p2/M5b_L12_L17.m｜:[0.11.0-alpha.1] Current Alpha`（现 :3-6）；`VERSION`；` |
| M5b-I-05 | I_DOC_HYGIENE | P2 | M5b | 已登记待修 | 仍成立 | OPEN | — | CI 元文档的规模计数与多项路径/条目已与注册表脱节 | 问题扫描/findings/I_DOC_HYGIENE/p2/M5b_L12_L17.m｜:（计数与映射）`（现 :6、:18、:31）；`ci/WORKFLOW_BINDING_AUDIT.m |
| M5b-I-06 | I_DOC_HYGIENE | P2 | M5b | 已登记待修 | 仍成立 | OPEN | — | docs/audit 的三份 V 期表仍是 DOCUMENT_INDEX 登记的 ACTIVE_INFORMATIVE，且内容与 | 问题扫描/findings/I_DOC_HYGIENE/p2/M5b_L12_L17.m｜:（orchestrator 行）`、`docs/audit/doc_classification.cs |
| M5b-I-07 | I_DOC_HYGIENE | P2 | M5b | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 活动文档中的版本命名空间自述与占位说明缺失（governance/VERSION_NAMESPACES 及若干合同页） | 问题扫描/findings/I_DOC_HYGIENE/p2/M5b_L12_L17.m｜:（history 豁免说明与 known_limits）`（现 :53-57）；`docs/contr |
| M5b-I-08 | I_DOC_HYGIENE | P2 | M5b | 已登记待修 | 仍成立 | OPEN | — | 版本命名空间文档自述版本与正文互斥，且把无消费者的"扫描器"写成权威依据 | 问题扫描/findings/I_DOC_HYGIENE/p2/M5b_L12_L17.m｜:（文首版本行与"本文档 = 1"）`（现 :8、:22）；`::（检查器依据）`；`tools/doc |
| M6a-I-004 | I_DOC_HYGIENE | P2 | M6a | 已登记待修 | 仍成立 | OPEN | — | 遗留 cc_* 通道的同名分叉实现不带 legacy 声明（其文件头文案与生产版雷同、默认导出符号、含无条件 stderr 进度 | 问题扫描/findings/I_DOC_HYGIENE/p2/M6a_L13_L14.m｜:文件头`（复核时点 :1-7 无 legacy/禁接入/公式差异声明）与 `::修复循环`（:139- |
| M6a-I-005 | I_DOC_HYGIENE | P2 | M6a | 已登记待修 | 无逐条处置行（合并层新增/未列） | OPEN | — | 横向新增：三件套与头注释把「唯一/内建/已接线/实测」类现状断言写成无锚散文，本次 38 条实核 20 条与事实源不符（本条为该 | 问题扫描/findings/I_DOC_HYGIENE/p2/M6a_L13_L14.m｜相符 15 条、与事实源不符 20 条、无法在仓内判定 3 条（判定依赖 `run/**` 免报区证据） |
| M6b-I-003 | I_DOC_HYGIENE | P2 | M6b | 已登记待修 | 仍成立 | OPEN | — | 文档内统计与清单陈述与实测不符（可复算的口径失真清单） | 问题扫描/findings/I_DOC_HYGIENE/p2/M6b_L16_L18.m｜:§8 Traceability`（现 :129、:130） ；> - 科学 → 算法 → 工程映射见  |
| M7-I-201 | I_DOC_HYGIENE | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | SCI-SCOPE「默认 FP64 科学计算；FP32 仅显式等价路径」被 SCI-CAL §9 在文档层直接反驳（L19-02 | 问题扫描/findings/I_DOC_HYGIENE/p2/M7_I_DOC_HYGI｜数值精度`（:53）对照 `docs/science/CALIBRATION.md::§9`（:82） |
| M7-I-202 | I_DOC_HYGIENE | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | hips_frame 字面值在合同与写侧措辞不一致（L21-009 **按前台裁决降级定稿**） | 问题扫描/findings/I_DOC_HYGIENE/p2/M7_I_DOC_HYGI |
| M7-I-203 | I_DOC_HYGIENE | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | DATA_SEMANTICS §24.4 给全整数的 coverage 结果标「UNIT=ADU/tile 口径透传」（L21- | 问题扫描/findings/I_DOC_HYGIENE/p2/M7_I_DOC_HYGI｜§24.4`（复核时 :1592 命中 ADU/tile） |
| M7-I-204 | I_DOC_HYGIENE | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | 同文档内「reason u8」两套互斥值域（rejection 0..3 vs sampler 0..5）（L21-012） | 问题扫描/findings/I_DOC_HYGIENE/p2/M7_I_DOC_HYGI｜§/reason 行`（复核时 :471/:992/:1246/:1266 四处） |
| M7-I-205 | I_DOC_HYGIENE | P2 | M7 | 已登记待修 | 仍成立 | OPEN | — | 符号 K 在唯一词典内三义并存；electron 许可域标注与四文档现状互斥（L21-014） | 问题扫描/findings/I_DOC_HYGIENE/p2/M7_I_DOC_HYGI｜K 行`（复核时 :21/:25）、`docs/science/CALIBRATION.md::K=t_ |
| M8-I-001 | I_DOC_HYGIENE | P2 | M8 | 已登记待修 | 仍成立 | OPEN | — | `lib/snr_estimator/README.md` 以现状口吻声明「bitwise 与线程数无关」及自测清单，但该模块  | 问题扫描/findings/I_DOC_HYGIENE/p2/M8_L22_L23.md｜:实现要点`（复核时点 :147「bitwise 与线程数无关。内存：掩膜 O(h·w) uint8 … |
| M8a-I-001 | I_DOC_HYGIENE | P2 | M8a | 需负责人裁决 | 仍成立 | OPEN | A-33 | 「每子库都要有 README」这一负责人直接要求在本仓无任何条文支撑，35/92 目录单元因此落在规范空白里 | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜5-12）；`docs/README-DOCS.md:8/:11`；`AGENTS.md::必读条目 4 |
| M8a-I-002 | I_DOC_HYGIENE | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | 门的全覆盖样本恰是最薄样本：5 份 README 缺负责人 8 要素中的 4 项且合同锚写法错误 | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜`lib/phase1/wcs/README.md`（10 行）、`lib/phase1/noise/R |
| M8a-I-003 | I_DOC_HYGIENE | P2 | M8a | 已登记待修 | 部分修复 | OPEN | — | tools/README 与 tools/ 实体名实不符：唯一文档只述 astro_toolkit（git 批操作器），检查器资 | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜:全文 83 行`（复核 1-13、19-40、56-71、75-79、81-83）；对照 `tools |
| M8a-I-004 | I_DOC_HYGIENE | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | healpix_core 上游出处 URL 格式损坏且 NOTICE 未保留上游版权原文（BSD-3 字面义务） | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜:来源头注`（复核 4-9）、`::neighbors 段注`（337-341、418）；`lib/co |
| M8a-I-005 | I_DOC_HYGIENE | P2 | M8a | 已登记待修 | 仍成立 | OPEN | — | L2 层把只存在于控制包解压区的「V7 编号标准」当现行权威：23 份 manifest 与多份 README 的条款原文仓内不 | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜3`、`modules/conformance/echo/module.yaml:3`、`noop/RE |
| M8a-I-006 | I_DOC_HYGIENE | P2 | M8a | 需负责人裁决 | 仍成立 | OPEN | A-32 | 「entrypoint」同词至少两义，仓内无权威定义 — 登记为术语缺口，不按猜测定档 | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜274 §8.2 节点唯一实际 entrypoint、:288 §8.4 单一模块 entrypoint |
| M8a-I-007 | I_DOC_HYGIENE | P2 | M8a | 已登记待修 | 部分修复 | OPEN | — | toolchain.lock 已刷新，INVENTORY_REPORT 仍以现势口吻归因锁「九类工具全部缺失」— 修复未回写型失 | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.m｜:§1 盘点方法第 4 条`（复核 15）、`::头部`（1-8）；`ci/toolchain.lock |
| W4-R2-03 | J_FS_PUBLISH | P2 | W4 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | `p1sess` 测试套件在 TMPDIR/TEMP/TMP 三缺时回退当前目录 ⇒ 产物散落仓库根，且被 `.gitignor | 问题扫描/findings/J_FS_PUBLISH/p2/W4.md |
| W4-R2-04 | J_FS_PUBLISH | P2 | W4-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | CLI「完成标记」与完整性锚的吞错三处：manifest 末块 flush 不判即 rename、sha256 失败向冻结 sc | 问题扫描/findings/J_FS_PUBLISH/p2/W4-b.md |
| W4-R2-05 | J_FS_PUBLISH | P2 | W4-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 「把旧错误口径当权威」专项命中：发布包对账工具验的是仓库内不存在的幽灵布局，且四套清单命名互斥 | 问题扫描/findings/J_FS_PUBLISH/p2/W4-b.md |
| W4-R2-06 | J_FS_PUBLISH | P2 | W4-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 发布包 build provenance 时间戳是硬编码常数，且 VERSION 缺失时以废止版本字面量兜底 ⇒ provena | 问题扫描/findings/J_FS_PUBLISH/p2/W4-b.md |
| W4-R2-07 | J_FS_PUBLISH | P2 | W4-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | 审核包生成器 `tools/gen_audit_pack.py`：清单与实际内容脱钩且自宣 PASS、含全量 git 历史 ⇒  | 问题扫描/findings/J_FS_PUBLISH/p2/W4-b.md |
| W4-R2-08 | J_FS_PUBLISH | P2 | W4-b | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | IO 与 CLI 临时文件命名可预测 + 全仓零独占创建 + overwrite 只在 begin 时点检查 ⇒ TOCTOU  | 问题扫描/findings/J_FS_PUBLISH/p2/W4-b.md |
| V7-N-08 | C_ALG_IMPL | P? | V7 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P3）亲和性探测失败 ⇒ 静默按 1 核，**无「探测失败」判别位** ⇒ run manifest 的 `budget=1` | 问题扫描/findings/C_ALG_IMPL/p3/V7.md｜83-95`（`sched_getaffinity != 0` → 1；计数为 0 → 1）；`hard |
| V7-N-09 | C_ALG_IMPL | P? | V7 | 判定非缺陷 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN/NOT_A_DEFECT | — | **判否负清单**（V7 逐行核为 fail-closed / 非缺陷，各轴与后续扫描**不得重报**） | 问题扫描/findings/C_ALG_IMPL/p3/V7.md |
| V7-N-10 | C_ALG_IMPL | P? | V7 | 已登记待修 | 无 _merge 处置表（V/W/SA/补轴 验证波 | OPEN | — | （P2）杜撰锚残留：`module_entry.cpp:355` 仍以「`API-DRZ-001` 默认 1.0」为 pixfr | 问题扫描/findings/C_ALG_IMPL/p3/V7.md |

## 五、无 `_merge` 逐条处置表的条目（291 条 · V/W/SA 验证波）

| 产出方 | 条数 | id 清单 |
|---|---:|---|
| V21 | 17 | V21-N-01, V21-N-02, V21-N-05, V21-N-06, V21-N-07, V21-N-12, V21-N-13, V21-N-15, V21-N-16, V21-N-17, V21-N-03, V21-N-04, V21-N-08, V21-N-09, V21-N-10, V21-N-11, V21-N-14 |
| W5 | 15 | W5-N-01, W5-N-02, W5-N-03, W5-N-04, W5-N-05, W5-N-06, W5-N-07, W5-N-08, W5-N-09, W5-N-10, W5-N-11, W5-N-12, W5-N-13, W5-N-14, W5-N-15 |
| V7 | 10 | V7-N-04, V7-N-05, V7-N-06, V7-N-07, V7-N-01, V7-N-02, V7-N-03, V7-N-08, V7-N-09, V7-N-10 |
| W2 | 10 | W2-N-01, W2-N-02, W2-N-03, W2-N-04, W2-N-05, W2-N-06, W2-N-11, W2-N-12, W2-N-13, W2-N-15 |
| W3-b | 9 | W3-R2-005, W3-R2-006, W3-R2-004, W3-R2-007, W3-R2-008, W3-R2-009, W3-R2-010, W3-R2-011, W3-R2-012 |
| FD | 8 | FD-F-003, FD-F-001, FD-F-002, FD-G-001, FD-G-002, FD-G-004, FD-I-001, FD-G-003 |
| V15 | 8 | V15-N-05, V15-N-06, V15-N-08, V15-N-09, V15-N-10, V15-N-11, V15-N-12, V15-N-13 |
| L28e | 7 | L28e-D-001, L28e-E-001, L28e-E-002, L28e-E-003, L28e-D-002, L28e-E-004, L28e-E-005 |
| V12-b | 7 | V12-N-08, V12-N-11, V12-N-07, V12-N-09, V12-N-10, V12-N-12, V12-N-13 |
| V19 | 7 | V19-N-01, V19-N-02, V19-N-03, V19-N-09, V19-N-10, V19-N-11, V19-N-12 |
| V2 | 7 | V2-N-01, V2-N-03, V2-N-04, V2-N-02, V2-N-05, V2-N-06, V2-N-07 |
| V9-b | 7 | V9-N-07, V9-N-08, V9-N-09, V9-N-10, V9-N-12, V9-N-14, V9-N-15 |
| V9-c | 7 | V9-N-16, V9-N-17, V9-N-18, V9-N-19, V9-N-20, V9-N-11, V9-N-13 |
| W1-b | 7 | W1-N-04, W1-N-05, W1-N-06, W1-N-11, W1-N-13, W1-N-10, W1-N-12 |
| W6 | 7 | W6-N-01, W6-N-02, W6-N-03, W6-N-06, W6-N-07, W6-N-08, W6-N-09 |
| V12 | 6 | V12-N-01, V12-N-03, V12-N-05, V12-N-06, V12-N-02, V12-N-04 |
| V18-b | 6 | V18-N-05, V18-N-09, V18-N-10, V18-N-11, V18-N-13, V18-N-14 |
| V9 | 6 | V9-N-01, V9-N-02, V9-N-03, V9-N-04, V9-N-05, V9-N-06 |
| W1 | 6 | W1-N-01, W1-N-02, W1-N-03, W1-N-08, W1-N-09, W1-N-07 |
| L28b | 5 | L28b-D-001, L28b-D-002, L28b-D-003, L28b-D-005, L28b-D-004 |
| V1-brief | 5 | V1-N-03, V1-N-05, V1-N-06, V1-N-07, V1-N-11 |
| V13-b | 5 | V13-N-04, V13-N-06, V13-N-05, V13-N-07, V13-N-08 |
| V19-b | 5 | V19-N-04, V19-N-05, V19-N-06, V19-N-07, V19-N-08 |
| V5 | 5 | V5-N-03, V5-N-01, V16-N-01, V5-N-02, V5-N-04 |
| W4-b | 5 | W4-R2-04, W4-R2-05, W4-R2-06, W4-R2-07, W4-R2-08 |
| L28c | 4 | L28c-D-001, L28c-E-001, L28c-E-002, L28c-D-002 |
| V12-c | 4 | V12-N-14, V12-N-15, V12-N-16, V12-N-17 |
| V20-b | 4 | V20-N-05, V20-N-07, V20-N-08, V20-N-09 |
| V8-b | 4 | V8-N-04, V8-N-07, V8-N-06, V8-N-08 |
| W2-b | 4 | W2-N-07, W2-N-08, W2-N-09, W2-N-10 |
| L28d | 3 | L28d-D-001, L28d-D-002, L28d-D-003 |
| SA | 3 | SA-N-01, SA-N-02, SA-N-03 |
| V10 | 3 | V10-N-01, V10-N-02, V10-N-03 |
| V10-b | 3 | V10-N-04, V10-N-05, V10-N-06 |
| V10-c | 3 | V10-N-07, V10-N-08, V10-N-09 |
| V11 | 3 | V11-N-01, V11-N-02, V11-N-03 |
| V11-d | 3 | V11-N-08, V11-N-09, V11-N-10 |
| V13 | 3 | V13-N-03, V13-N-01, V13-N-02 |
| V14 | 3 | V14-N-01, V14-N-02, V14-N-03 |
| V14-b | 3 | V14-N-04, V14-N-05, V14-N-06 |
| V17 | 3 | V17-N-01, V17-N-02, V17-N-03 |
| V17-b | 3 | V17-N-04, V17-N-05, V17-N-06 |
| V18 | 3 | V18-N-12, V18-N-01, V18-N-02 |
| V2-D2 | 3 | V2-N-08, V2-N-09, V2-N-10 |
| V20 | 3 | V20-N-01, V20-N-02, V20-N-04 |
| V3 | 3 | V3-N-01, V3-N-02, V3-N-03 |
| V6 | 3 | V6-N-01, V6-N-02, V6-N-03 |
| V8 | 3 | V8-N-01, V8-N-02, V8-N-03 |
| W3 | 3 | W3-R2-001, W3-R2-002, W3-R2-003 |
| W4 | 3 | W4-R2-01, W4-R2-02, W4-R2-03 |
| SA-b | 2 | SA-N-04, SA-N-05 |
| V1 | 2 | V1-N-02, V1-N-04 |
| V11-b | 2 | V11-N-04, V11-N-05 |
| V11-c | 2 | V11-N-06, V11-N-07 |
| V14-c | 2 | V14-N-08, V14-N-07 |
| V15-b | 2 | V15-N-14, V15-N-15 |
| V20-c | 2 | V20-N-03, V20-N-11 |
| V4 | 2 | V4-N-04, V6-N-10 |
| V6-evid | 2 | V6-N-04, V6-N-05 |
| W6-b | 2 | W6-N-04, W6-N-05 |
| V1-N09 | 1 | V1-N-09 |
| V1-N12 | 1 | V1-N-12 |
| V15-c | 1 | V15-N-16 |
| V20-d | 1 | V20-N-12 |

> 该波条目在 `findings/**` 中有正文与 `ID/优先级`，但没有 `_merge/M*.md` 的逐条四态行（合并层早于该波或按 producer 归档）。其处置态仅取 `FIX_LEDGER` 已填列；未填者一律记“已登记待修”，不猜判。
