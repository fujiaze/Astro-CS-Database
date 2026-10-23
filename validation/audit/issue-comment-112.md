## ACSD v2 重建交付（阶段汇报 + 独立验证 + 独立审核结果）

分支 `rebuild/acsd-v2`；冻结设计提交 `9073a656b` 是本分支祖先，分支未重置；`main` 未改；无 PR；未强推。
本分支相对冻结提交共 **121 个提交**（feat/fix/test 为主）。

### 1. 实现范围

22 个模块全部从零实现并接上同一套公开 API（`include/acsd/contracts.hpp`）：

底座（model/numerics/operators/uncertainty/resources/storage）、坐标与几何
（coordinates/geometry）、观测（observation）、目录与盲解（catalogue/sources）、
PSF 与测光（sources/response/photometry）、统计（information/sky/coadd）、
投影与工作流（projection/workflows）、入口（apps/cli、apps/contract）、独立 Qt6 GUI（gui/）。

公开声明只做了 **2 处最小修订**，各有反例与独立复核：
`make_footprint_provider` 公开工厂（此前 `FootprintProvider` 无构造入口 ⇒ 几何链对外部不可达）；
`Box`/`Basis` 的默认构造值（此前默认构造出未定义值）与 `Assessment.omitted_position_bound_rad`
（省略的位置误差界此前无处承载）。其余字段与签名未变。

### 2. 独立验证（`python3 validation/run_all.py`，11 个必需 case）

**pass = 7，fail = 0，pending = 4**：
U01（14/14，内存与合同进程双路由）、C01（69/69）、G01（171/171）、F01（1 pass + 1 pending）、
S01（1 pass + 1 pending）、V01（12/12）、E01（11/11）。
pending = W01 / P01（6/7 负例已检出）/ R01（6/7，缺配置 schema 的缓存开关）/ X01（端到端未发布产品），
每一项都在 `validation/<case>/comparison.json` 写明具体缺口。**pending 不是通过。**

关键实跑数字：
- U01：`quadratic_form` = **4.328125**（= 277/64，精确）、`numeric_enclosure` [4.328125, 4.328125]；
  协方差列 [3.75, 2] 与 [2, 10.3125]；判别负值 289/64 = 4.515625（差 0.1875，用例有判别力）。
- C01：forward vs astropy/wcslib ≤ **6.44e−16 rad**；inverse **0 pixel**；导数相对差 ≤ 2.7e−7；
  SIP 2..9 阶全覆盖；4 个具名失败（背半球逆、非 ICRS、PV 畸变、强 SIP）。
- G01：逐叶与总和含独立真值；`work=1` → 具名 work 终止；`max_bits=16` → 具名 precision 终止；
  极区 `chart_apex_singular` 具名边界；预算耗尽时未决质量保留不归零。
- E01：产品闭包 9 项（块哈希**从磁盘字节重算**）+ FITS 头/像元经 astropy 往返差 **0**。
- V01：仿射真值被重心插值精确复现到 **4.44e−16**；无曲率界时只给条件预测、**不发布区间**。

### 3. 产品与三命令

- `acsd export` 端到端 exit 0：manifest（`acsd.product/2`、complete、算法集带 source_hash）、
  objects/、blocks/、`signal.fits`；独立解析器读回 CRPIX=(8.5,8.5)=((w+1)/2,(h+1)/2)。
- 产品往返：publish → **新进程** open → 独立读回；坏闭包（缺块/改块/错清单哈希/截断）全部被拒，
  缺块报 `missing_dependency`、磁盘失败仍 `io_error`。
- 合同驱动 30 个 operation，每条都有「请求+响应+退出码」三件套证据。
- CLI：取消 → **exit 5**（恰一个 run_finished、不发布半成品）；配置缺键 → **exit 2**（不再 SIGABRT）；
  预算耗尽 → **exit 4 + resource_exhausted** 且 products 为空；doctor 与 --version 输出纯 JSONL。

### 4. 独立审核（两份报告，均由**未参与实现**的 Agent 只读审核）

`validation/audit/cat_review.md`（审核域：除目录/盲解/响应/光度外的全部）与
`validation/audit/phot_review.md`（审核域：目录/盲解/响应/光度；其中响应与光度部分已由审核者
标注为自审）。共 **19 条发现（P0 1 / P1 6 / P2 12）**，处置：

- 底座已修：同 shared_id 的共享根未合并（交叉项丢失）、`CovarianceAction.influence` 被忽略、
  `ActionImpl` 未初始化公开 `spaces`、`Box`/`Basis` 未定义默认值、位置误差界无处承载。
- 模块已修：PSF 退化模型（|ρ|→1 静默返 0）、目录历元语义不一致（20 年偏 12.93″…129.29″）、
  两份 `propagate_star`、stdout 被 DEBUG 打印污染、CLI 配置崩溃面、缺块错误码、doctor 非 JSONL、
  `--version` 计数、mosaic/export 无取消点、Gram 上的秩分类（阈值被平方成 1e−6）。
- 口径收口：伪逆装配四份副本 → 底座唯一实现 `numerics::pseudo_inverse`；
  历元传播两份实现 → 目录模块为正本。

**独立验证方（val，未参与实现）在本轮发现并驱动修复的底座级缺陷**包括：
数组编码器字节数翻倍、协方差作用按输入长度读输出、Kronrod 中心节点重复计数、SVD 输出因子未赋值、
`BigInt` 除法死循环、HEALPix 查表不精确、最小二乘的反射向量覆盖 R 对角元与列缩放还原方向、
`inverse` 的像元原点错位（CRPIX≠1 时误差恒 2·(CRPIX−1)）、`operators::compose` 恒失败、
探测器足迹缺像元位置、work/precision 预算只被测量未被执行、Drizzle 权重列号硬编码为 0。

其中最有价值的一类发现是**「空集通过」**：验证方最初用「成功项的最大误差」作判据，
在**全部失败**时最大值恰为 0.0 而判 pass。改成「必须记录 attempts/successes」后，
立刻暴露了 `inverse` 与几何的多条真实缺陷。同样的模式在实现方的自测里也存在，已一并修好。

### 5. 剩余限制（不声称通过）

1. **Windows 未实跑**（本环境只有 Linux amd64；Windows 只写了构建配置）。
2. **真实 HST/M42 数据未验**（全部为合成输入 + 外部标准 astropy/wcslib/astropy-healpix/healpy/scipy）。
3. **Huber bootstrap 未验**。
4. **`acsd normalize` 端到端未完成**：冻结 X01 输入（8 帧 × 128×128）上 CLI 15 s 结束、
   无崩溃、published=0，逐帧为具名失败（光谱基无效 2、PSF 秩亏 1、像元积分 1、盲解假设耗尽 4）。
   **P-small/P-large 性能读数因此无法给出**；已实测的是几何单帧代价（块级 0.28 s、
   逐像元 75.3 s）与端到端 wall/峰值 RSS（22.4 MB）。
5. **11 个必需 case 里 7 pass / 0 fail / 4 pending**；pending 的都写明了具体缺口。
6. **合同驱动 30/78 个 operation**，其余在 schema 里声明但在驱动里具名 `unsupported`。
7. **极区仍是具名能力边界**（TAN 在天极无定义）；A 路线的顶区版在实现中。
8. **Drizzle 的默认口径已改为逐像元源行**（列数 = 帧像元数，权重是精确交叠面积占比）；
   16×16 块级路径保留为备选，但**块级不声称权重正确**（中心归属与精确交叠的最大差 1.0、RMS 0.1055）。
9. **参数盒是剖面区间**，不是完整 interval-Newton 根证书。
10. **大规模星数走稠密分解**（稀疏 QR/LSQR 未实现）；**非矩形收集域不支持**。
11. **本机 CFITSIO 4.6.2 的像元写路径不可用**（`ffppx` 报成功但数据单元全零），
    故 FITS 写出用项目内显式实现、读路径用 CFITSIO；外部 astropy 往返差 0。

### 6. 复现

```bash
python3 tools/check_design.py && python3 tools/generate_contract_schema.py --check
cmake -S . -B build/science -G Ninja -DACSD_BUILD_SCIENCE=ON && cmake --build build/science
ctest --test-dir build/science --output-on-failure
python3 validation/run_all.py
bash tools/acceptance.sh
cat validation/audit/cat_review.md validation/audit/phot_review.md
```

完整交付报告见 `validation/audit/delivery-112.md`。
