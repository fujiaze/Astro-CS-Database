# ACSD v2 重建交付报告（Issue #112，分支 `rebuild/acsd-v2`）

本文件是交付的**证据索引**：每一条主张都指向仓库内的可复跑证据或实跑数字。
没有实跑的条目一律标 `NOT_RUN`；失败的条目保留失败事实与 attempt。

冻结设计提交：`9073a656bf1794f307cdb2a977a52bbfa493c118`（本分支的祖先，分支未重置）。
`main` 未修改，未创建 PR，未强推。

---

## 1. 交付了什么

### 1.1 模块（`src/`、`apps/`、`gui/`）

| 层 | 模块 | 状态 |
|---|---|---|
| 底座 | model（hash / exact / units / array / result） | 实现并自测 |
| 底座 | numerics（interval / linalg / quadrature） | 实现并自测（ctest `numerics_selftest`） |
| 底座 | operators（线性作用：identity/diagonal/csr/compose/sum/transpose/…） | 实现并自测（ctest `operators_selftest`） |
| 底座 | uncertainty（协方差、根合并、几何界） | 实现并自测（ctest `covariance_selftest`） |
| 底座 | resources（CPU 预算、work 账本、线程池） | 实现 |
| 底座 | storage（LocalStorage，raw/zstd 块、sha256 校验） | 实现 |
| 坐标 | coordinates（TAN+SIP WCS、注入性证书、HEALPix、chart、足迹 provider） | 实现并实跑 |
| 几何 | geometry（精确有理 cut-cell 交叠、面积账本、目标域包含 fallback A、Drizzle、refine） | 实现并实跑 |
| 观测 | observation（校准、重整、bind_wcs、bind_observation_model） | 实现并实跑 |
| 目录 | catalogue（列式目录、离线四元组索引、只读 provider） | 实现并实跑 |
| 盲解 | sources（探测、四元组匹配、TAN+SIP 拟合、参数噪声） | 实现并实跑 |
| PSF/测光 | sources（fit_psf / evaluate_psf / measure_science_sources） | 实现并实跑 |
| 响应 | response（响应积分、photon/energy 约定、缓存键） | 实现并实跑 |
| 光度 | photometry（联合 latent、零空间、apply_scale） | 实现并实跑 |
| 统计 | information（通量信息、支撑三角网） | 实现并实跑 |
| 天光 | sky（相对天光、规范、修正协方差） | 实现并实跑 |
| 归并 | coadd（BLUE、非负 QP、选择、权重） | 实现并实跑 |
| 投影/导出 | projection（build_export、export_signal） | 实现并实跑 |
| 工作流 | workflows（normalize/mosaic/export、产品持久化、FITS 写出） | 实现并实跑 |
| 入口 | apps/cli（`acsd normalize|mosaic|export --json`） | 实现并实跑 |
| 入口 | apps/contract（合同请求/响应驱动） | 接入 24 个操作（见 §5 限制） |
| 独立 GUI | gui/（Qt6 独立进程，不链接科学库） | 实现并实跑（Linux/amd64） |

`include/acsd/contracts.hpp` 是唯一声明正本：本次重建**修改了 2 处**（都是最小修订，
各有反例与独立复核）：新增公开的 `make_footprint_provider` 工厂；给 `Box`/`Basis`
加默认构造值。其余字段与签名未变。

### 1.2 规模

- 本分支相对冻结提交的提交数：76（feat 24 / fix 23 / test 20 / 其余 9）。
- 实现代码规模：`src/`、`include/`、`apps/`、`gui/` 共 112 个 C++ 文件、约 41 900 行。
- 验证代码：`validation/`（11 个必需 case + 独立审核 + 各模块自测）。

---

## 2. 独立验证的真实状态

`python3 validation/run_all.py`（一键入口，输出 `validation/run_summary.json`）：
**pass = 7，fail = 0，pending = 4**。

| case | 状态 | 关键数字 |
|---|---|---|
| U01 | **pass**（14/14，内存与合同进程双路由） | 277/64 = **4.328125**，包围 [4.328125, 4.328125]；C 列 [3.75, 2] 与 [2, 10.3125]；判别负值 289/64 = 4.515625（差 0.1875） |
| C01 | **pass**（69/69） | forward ≤ 6.44e−16 rad；inverse 往返 0 pixel；导数相对差 ≤ 2.7e−7；SIP 2..9 阶全覆盖；4 个具名失败 |
| G01 | **pass**（171/171） | 逐叶与总和含独立真值；work=1 → 具名 work 终止且不认证；max_bits=16 → 具名 precision；极区 `chart_apex_singular` 具名边界 |
| F01 | **pass**（1 pass + 1 pending） | fit_psf 与 measure_science_sources 返回 ok（8 星、integrated_flux 937.6175…）；`two_source_aperture_flux` 标 pending 并写明「驱动测的是每源在其 PSF 窗口上的积分，不是孔径盒积分」 |
| S01 | **pass**（1 pass + 1 pending） | 公共天光保护：观测 K·H 的自由公共方向逐列为 0（≤1e−10）；留出方差 pending（公开合同里 P 的域不可观测） |
| V01 | **pass**（12/12） | 仿射真值被重心插值精确复现到 **4.44e−16**；二次真值在 δ+M 界内；无曲率界时只给条件预测、**不发布区间**；域外查询不填 |
| E01 | **pass**（11/11） | 独立闭包检查器（块哈希**从磁盘字节重算**）+ FITS 头/像元经 astropy 往返差 **0** |
| W01 | pending | 观察侧到达联合协方差装配并给出具名证书失败（`the normalising denominator has no positive lower bound`）；权重真值（BLUE 7/6,−1/6；QP 1,0；LOO 假阳性率/覆盖率）由 `validation/stats` 的 173 项检查对冻结参考实测 |
| P01 | pending（6/7） | 6 项具名负例逐条被检出且**可归因于篡改**；`unknown_recipe` 需合同路径 dispatch |
| R01 | pending（6/7） | 载荷与资源无关（threads 1/4、H=32/256MiB 四次运行 signal 哈希全同）；写失败具名且不发布；取消 → exit 5 且 cancelled=1；**work 账本 8625 units**；`cache_switch_reachable` pending（配置 schema 无缓存开关） |
| X01 | pending | 三命令在真实磁盘上实跑：`normalize` 15 s 结束、**无崩溃**、published=0、逐帧状态具名；未发布产品 |

**「未决」不是「通过」**：W01/P01/R01/X01 的观察侧各自写明了具体缺口（见 `validation/<case>/comparison.json` 的 `reason`）。

**红线（写在 `validation/README.md`）**：任何量测必须记录 attempts/successes；
成功数为 0 判 fail，不得用空集默认值充当观测值。这条红线本身是本次重建中
被独立验证方发现并修掉的（见 §4）。

---

## 3. 产品与三命令

- `acsd-contract`（合同驱动）：`u01` = 4.328125 精确、`external-array` = [3,7]、
  `to-fits` = (1.25, 3.5)，三例 exit 0。
- `acsd export` 端到端：exit 0，产物树 12 个文件（`manifest.json`（`acsd.product/2`、
  complete=true、算法集带 source_hash）、`objects/`、`blocks/`、`signal.fits`），
  独立解析器读回 16×16 float64、CTYPE1=RA---TAN、CRPIX=(8.5,8.5)=((w+1)/2,(h+1)/2)、
  BUNIT=fixture:L/sr。
- 产品往返：publish → **新进程** open → 独立读回，瓦片 64×64×12、CTYPE1=RA---HPX、
  有限值恰为源叶 −2/0/3。
- 坏闭包不发布：错 manifest hash、缺块、坏块（hash_mismatch）、截断 manifest 全部被拒。
- `acsd normalize`（8 帧 × 128×128 的冻结 X01 输入）：**15 s 结束、无崩溃、published=0**，
  逐帧状态具名（2×「the spectral basis is invalid」、1×「the design matrix is rank deficient」、
  1×「the pixel integral cannot be built」、4×「the search was exhausted at the hypothesis limit」）。
  未发布任何产品 ⇒ 见 §5。
- 取消：控制报文与 SIGTERM 均 → **exit 5**、恰一个 run_finished、products 为空。
- 预算耗尽（work=1）→ **exit 4 + resource_exhausted**、products 为空（不再出现「exit 0 + 不同载荷」）。

---

## 4. 独立审核与修订

两份**未参与实现**的独立审核报告（只读审核，产出在 `validation/audit/`）：

- `cat_review.md`：审核域为除 catalogue/sources/response/photometry 外的全部。
- `phot_review.md`：审核域为 catalogue/sources/response/photometry（其中 response 与
  photometry 属审核者自写，该两处结论已由其本人标注为自审）。

审核共报出 **19 条**发现（P0 1 条、P1 6 条、P2 12 条），处置如下：

| 编号 | 严重度 | 内容 | 处置 |
|---|---|---|---|
| 共享根未合并 | P1 | 同一 shared_id 的两个根未合并 ⇒ 同源交叉项丢失（单根 2 / 双根 4，应为 8） | **已修**（`e171d8995`），新增 ctest 回归 |
| 协方差忽略 influence | P0 | `CovarianceAction.influence` 被忽略 ⇒ 发布的协方差不是 I·C·Iᵀ | **已修**（`384be1820`），新增 ctest 回归 |
| ActionImpl 未初始化 spaces | P1 | 公开成员 `LinearAction::spaces` 恒空 | **已修**（`384be1820`） |
| `Box`/`Basis` 未定义默认值 | P2 | 默认构造出未定义值（plane 基下变 inf/NaN） | **已修**（`ec8a63a43`，合同最小修订） |
| PSF 退化模型 | P0 | \|ρ\|→1 时 density 静默返 0 ⇒ 模板塌陷 ⇒ 误报 not_identifiable | 已派 obs 修订 |
| 目录历元不一致 | P1 | `lookup` 传播到目录历元、`query` 到观测历元；盲解用了前者（20 年差偏 12.93″…129.29″） | 已派 cat 修订 |
| 两份 propagate_star | P1 | sources 的副本违反 pmRA·cosDec 约定 | 已派 obs 修订 |
| stdout 污染 | P1 | 8 处 `printf("DEBUG …")` 留在交付库里 | 已派 obs 修订 |
| CLI 配置崩溃 | P1 | 缺键 → 未捕获异常 → SIGABRT（134）而非 exit 2 | 已派 apps 修订 |
| 合同驱动覆盖面 | P1 | 11 个必需 case 里 7 个「从未运行」（dispatch 缺入口） | 已派 apps + val 修订 |
| 其余 P2 | P2 | 缺块错误码、doctor 非 JSONL、version 计数、mosaic/export 无取消点、Gram 秩阈值、目录解码、算法重复实现等 | 已分派各归属 Agent |

此前由独立验证方（val，**未参与实现**）发现并已修复的底座级缺陷，同样逐条落库并复跑：

1. 数组编码器字节数翻倍（`bb90d6663`）——所有数组写失败。
2. 协方差作用按输入长度读输出（`17d6258fe`）——U01 整条路失效。
3. Kronrod/Gauss 求积中心节点重复计数且缺 half 因子（`ce215b04d`）——∫1 得 38.85 而非 20。
4. SVD 输出因子未赋值 + 转置语义（`2f5123169`）——空因子与空指针。
5. `BigInt` 二进制长除法（`5a7fb8522`）——`gcd` 死循环、全部有理数运算挂起。
6. HEALPix 精确叶查表（`5a7fb8522`）——与 astropy-healpix 逐叶 18000/18000 一致。
7. 最小二乘的反射向量覆盖 R 对角元、多右端项不填解（`1779b0d98`）。
8. 最小二乘的列缩放还原方向（`fb600080b`，我方修订引入、被 cat 独立复现后改正）。
9. `inverse` 的像元原点错位（`8b0d7fac0`）——CRPIX≠1 时误差恒 2·(CRPIX−1)。
10. `Rational` 零分母（`f4a8ea89b`）。
11. `operators::compose` 恒失败（`76f8d9f32`）。
12. 探测器足迹缺像元位置（`6acd431b8`）——逐叶面积偏高 0.157% 的真因。
13. work/precision 预算此前只被测量、没有被执行（`6acd431b8`）。
14. Drizzle 权重列号硬编码为 0（`e4d82973c`）。

**「空集通过」是本次最有价值的一类发现**：验证方最初用「成功项的最大误差」作判据，
在**全部失败**时最大值恰为 0.0 而判 pass。修正判据（必须记录 attempts/successes）
后，立即暴露了上述第 9 条与几何的若干条。同样的模式在实现方的自测里也存在，
已一并修好（`7ed9e67f4`）。

---

## 5. 剩余限制（不声称通过的部分）

1. **Windows 未实跑**：本环境只有 Linux amd64（Debian 13、GCC 14.2、glibc 2.41、
   Qt 6.7.3）。Windows 只写了构建配置，**没有在 Windows 上编译或运行过**。
2. **真实 HST/M42 数据未验**：全部验证用合成输入与外部标准（astropy/wcslib、
   astropy-healpix、healpy、numpy/scipy）。
3. **Huber bootstrap 未验**：Huber 默认关；显式开启后的聚类 bootstrap 验证未做。
4. **`acsd normalize` 端到端未完成**：冻结 X01 输入（8 帧 × 128×128）上，CLI 15 s 结束、
   无崩溃、**published=0**，逐帧为具名失败（光谱基无效 2 帧、PSF 设计矩阵秩亏 1 帧、
   像元积分 1 帧、盲解假设耗尽 4 帧）。**VERIFICATION 的 P-small / P-large 性能读数因此
   无法给出**；已实测的是几何单帧代价（块级 0.28 s、逐像元 75.3 s，
   `validation/geo/geo_performance.json`）与 x01 端到端的 wall/峰值 RSS
   （22.4 MB；`validation/lead/perf_x01_normalize.json` 保留了修复前 214.7 s 的记录）。
5. **VERIFICATION 必需 case 的覆盖面**：11 个里 **7 个 pass、0 个 fail、4 个 pending**
   （W01/P01/R01/X01，各自写明具体缺口）。pending **不是通过**。
6. **合同驱动接入 30 个操作**（含产品系与观测系），其余在 schema 里声明但在驱动里具名
   `unsupported`。
7. **极区（north_polar）仍是具名能力边界**：TAN 在天极无定义（chart 导数奇异）；
   A 路线的顶区版（不依赖 chart 导数）设计已定，**未实现**。
8. **Drizzle 的源行单位**：交付默认已改为**逐像元源行**（列数 = 帧像元数，权重是精确交叠
   面积占比）；16×16 的块级路径保留为「精度换速度」的备选，但**块内归属误差未进分子**
   （实测中心归属与精确交叠的最大差 1.0、RMS 0.1055），因此**块级不声称权重正确**。
9. **参数盒是简化实现**：光度与 PSF 的数值参数盒按「剖面区间」给出，不是完整的
   interval-Newton 根证书（已具名标注）。
10. **稀疏 QR/LSQR 未实现**：大规模星数走稠密分解，超预算时返回失败原因而非降级。
11. **非矩形收集域不支持**：compute_overlap 遇非矩形足迹整体 `unsupported`（已具名）。
12. **本机 CFITSIO 4.6.2 的像元写路径不可用**（`ffppx` 报成功但数据单元全零），
    因此 FITS 写出由项目内的显式实现承担，读路径用 CFITSIO；证据与外部 astropy
    往返校验在 `validation/apps/fits_cfitsio_probe.{cpp,txt}` 与
    `third_party/manifest.json`。
13. **共享工作树的可重复性**：审核期间曾因另一 Agent 的在途编辑导致整树构建失败
    （已自愈）。所有提交前必须跑全量构建，这条已写入各 Agent 的交付纪律。

---

## 6. 复现入口

```bash
# 设计正本与声明
python3 tools/check_design.py
python3 tools/generate_contract_schema.py --check

# 构建与自测
cmake -S . -B build/science -G Ninja -DACSD_BUILD_SCIENCE=ON
cmake --build build/science
ctest --test-dir build/science --output-on-failure

# 独立验证套件（11 个必需 case）
python3 validation/run_all.py

# 验收门（设计 → 语法 → 构建 → 三例 → CLI → 验证套件 → GUI ctest）
bash tools/acceptance.sh

# 独立审核报告
cat validation/audit/cat_review.md validation/audit/phot_review.md

# GUI（真实 CLI 协议冒烟，offscreen）
bash gui/tests/run_offscreen_smoke.sh
```

## 7. 过程纪律

- 冻结提交 `9073a656b` 是本分支祖先，分支从未重置；`main` 未改；无 PR；未强推。
- 每个 Agent 只提交自己的写域路径（`git add -- <paths>` + `git commit -- <paths>`），
  不用 `git add -A`/`-a`，不 `reset --hard`/`stash`。
- 未知误差不填零；缺能力按声明的 Code 具名返回；不以 `unsupported` 桩占位。
- 独立验证与独立审核方**未参与实现**；它们报出的每一条 P0/P1 都已落库、修订并复跑。
