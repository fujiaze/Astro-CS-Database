# HiPS 稠密数据层与内存累加器的压缩编码评估（zstd / Rice / TRIM）

> 上游：ASTROCS_DESIGN.md §8.3（异步只用于能隐藏延迟的 I/O、预取与压缩）、§9（内存极简化）、
> §10（aio 是文件级唯一 I/O 边界）、§13（发布决定只属负责人）。
> 证据锚：`artifacts/evidence/compress-01/`（机器可读数字与复现脚本）；复现命令见 §7。

**结论（先给判断，再给证据）**

1. **HiPS 稠密数据层：zstd 的压缩率与时长都很好，但「不可作为默认」——标准否决。**
   REC-HIPS-1.0 §4.2.1.3 的扩展名是 **MUST**（只认 `.fits` / `.jpg` / `.png`，且小写），
   `hips_tile_format`（**R 必填**）词表封闭为 `fits/jpeg/png/tsv`——**没有任何 token 能声明 zstd**；
   FITS 4.0 §10.4 Table 36 的 `ZCMPTYPE` 是**穷举表**且新增须向 IAUFWG 注册，zstd 不在表内。
   标准**允许**压缩的位置是 **HTTP 传输层**（§5.1 Note）与**分发容器**（§5.1 tar/zip），不是产品文件格式。
2. **若一定要在产品文件内压磁盘：标准内唯一无损选项是 FITS 4.0 §10 tile-compression +** `quantize_level=0`**。**
   实测 **GZIP_2_q0 逐位一致**（读端必须显式传 `nulval` 才能还原 NaN 位模式；本产品 NaN 单一模式
   `0x7FC00000`，100% 可还原），整产物外推 **0.4115**；但压缩 6.10 ms/MiB、解压 2.81 ms/MiB，
   在本机 419 MiB/s 磁盘上**净亏 5.87 ms/瓦片**（临界带宽只有 **132 MiB/s**）。
3. **Rice 的「更优」是假象**：`RICE_1`（fpack 默认 `-q 4`）整产物 **0.1125**，看起来 3.5 倍于一切通用压缩器——
   但它是**有损**的：实测**摧毁 NaN 掩码**（238553 个无效像素变成有限垃圾）且有限像素最大相对误差
   **1.1e-2**。而**无损 Rice 在 float 图像上根本不可用**：`quantize_level=0` + `RICE_1` 在 cfitsio 4.6.2 上
   一律返回 **status 413 "error compressing image"**（195/195 瓦片全失败）。**如实报：Rice 不适用，不是更优。**
4. **zstd 唯一能赢的地方是速度**：zstd 3（+字节转置）比标准内的 GZIP_2_q0 **快 6.6× 压缩、19× 解压**，
   压缩率还略好（0.3974 vs 0.4115）。这个优势只在**传输层/归档层**（标准允许）才有意义。
5. **内存累加器：不采用。** zstd 各档在累加器数值载荷上只能到 **0.8000**（收益 20.0%，恰好卡在
   MEM-DESIGN-01 的 20% 门槛线上）；而「解压-改-重压」的往返成本是 **6.03 ms/次就地更新**（zstd 3），
   全产物 4707 次更新 = **+28.4 s CPU**，对照 MEM-DESIGN-01 实测优化后 writer 全程 user time **27.68 s**
   ⇒ **writer CPU 翻倍**。zstd 是纯字节无损编码器，**能**保持逐字节一致，但 A+C+B(cons) 已把峰值
   驻留降到 142.4 MiB 且「完成即整体释放（0 字节）」，压缩在其上最多再省约 15~20 MiB。**收益/代价不成立。**

---

**路径约定**：本文正文中的 `code/…`、`evidence/…`、`data/…`、`logs/…` 均相对原基准工作区
`run/COMPRESS-01/`（gitignore 产物，可能已被 `run_gc` 回收）。已入库的机器可读证据与复现脚本在
`artifacts/evidence/compress-01/`；全部复现命令与重建方式见 §7。语料提取（§7 步骤 1）需要主产物
`run/PERF-401/out/real16_w1` 存在。

---

## 1. 依据与测量口径

### 1.1 依据

| 层 | 文档 / 条款 | 本评估用到的规定 |
|---|---|---|
| 最高设计 | ASTROCS_DESIGN.md §8.3 | 「异步只用于能隐藏延迟的 I/O、预取与**压缩**」；静态预算/探针校正；「内存占用永不越界」 |
| 最高设计 | ASTROCS_DESIGN.md §9 | 「内存极简化：工作集只保留当前分块所需，流式读取、分块处理、用完即释」 |
| 最高设计 | ASTROCS_DESIGN.md §10 | aio 是文件级唯一 I/O 边界；产品只落 output_dir；run/ 只放临时产物 |
| 最高设计 | ASTROCS_DESIGN.md §13 | Alpha 前产物不出现版本信息；发布决定只属负责人（本文不下发布结论） |
| 产品语义 | docs/contracts/DATA_SEMANTICS.md §12.2/§12.4 | signal 无效像素 **IEEE NaN 填充**；support 无效 0.0；variance/ivar 无信息 0.0；逐 tile DATASUM/CHECKSUM |
| 算法登记 | docs/algorithms/HIPS_WRITER.md | ALG-HIPS-001..005；§0 范围界定 |
| 前序实测 | 内存累加器方案评估 MEM-DESIGN-01（产物在 run/，可能已被 run_gc 回收），§1.4/§2.4/§5 | zlib9 0.90 / lzma 0.82~0.85 载荷压缩率；A+C+B(cons) 峰值 1708.0→186.9 MB；优化后 user time 27.68 s |
| 仓内先例 | lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md §5.4 | 「数组不压缩…如需进一步压缩体积，可在**传输层（HTTP gzip）或归档层**（外层 zstd 包）处理」——与本文结论同向 |

### 1.2 被测对象（真实产物，只读）

- **主产物**：`run/PERF-401/out/real16_w1`（M42 16 帧 Phase2 mosaic HiPS，nside=2^18、K=9、tile_width=512、
  4 个 Image 子产品 signal/support/variance/ivar；每层 756 个 Npix*.fits + properties/Moc/metadata）。
  - 瓦片 = **512×512 big-endian float32（BITPIX −32）**，文件 1,054,080 B = 2880 B 头 + 1,048,576 B 数据段 + 2624 B 填充。
  - **稠密数据层总量：3024 个数据瓦片 / 2.95 GiB 数据段**（另有 43 个 .bin、12 个 .json 非瓦片文件，不在口径内）。
  - properties 声明 `hips_tile_format=fits`、`hips_version=1.4`、`dataproduct_type=image`。
- **基准语料**：从该产物抽取 **188 个真实瓦片的数据段**（每层 24 个叶级 + 每阶 3 个层级 × 9 阶），
  另导出一份「仅有效载荷」（finite 且非零元素）副本与 **7 个对照/负例块**，共 383 个 1 MiB 级输入（288 MiB）。

### 1.3 机器规格（实测环境）

| 项 | 值 |
|---|---|
| CPU | Genuine Intel(R) CPU 0000 @ 1.70GHz，1 socket / 8 core / 16 thread |
| 内存 | 24,030 MB，**无 swap** |
| 磁盘 | /dev/vdb1 503 GB（可用 161 GB），挂载 /workspace |
| **顺序吞吐（O_DIRECT 实测）** | **写 419 MiB/s**（4.67/4.89/10.02 s per 2 GiB，中位 4.89 s）；**读 359 MiB/s**（6.28/5.65/5.71 s，中位 5.71 s）；缓冲读缓存命中 0.29 s/2 GiB |
| 工具链 | Python 3.13.5、gcc 14.2.0、**libzstd 1.5.7**（/usr/bin/zstd v1.5.7 + python zstandard 0.25.0）、zlib 1.3.1、xz 5.8.1、**cfitsio 4.6.2**、astropy 7.0.1、numpy 2.2.4 |
| **fpack/funpack** | **未安装**（which fpack funpack 为空）。本文改用 **cfitsio 4.6.2 的 fits_img_compress / fits_read_img** —— fpack 内部正是调用这两个函数，故口径等价；未安装的事实如实登记（§8）。 |

### 1.4 测量方法（可复跑，见 §7）

- **压缩率**：zstd 用 in-process C 基准（链接系统 libzstd，无进程启动开销），档位
  `-7,-5,-3,-1,1,3,5,9,12,15,19,22`（22 = ultra；负档 = zstd fast levels），每档 **1 次预热 + 5 次计时取中位**；
  解压 9 次取中位；每档做完整往返 + memcmp 校验（CSV 的 ok 列）。
- **字节转置（shuffle）**：f32 按 4 字节转置后再压（等价于 FITS GZIP_2 的 shuffle 变换），
  转置/逆转置在计时区间之外，只计 codec 时间。
- **FITS 原生**：fits_set_compression_type + fits_set_quantize_level(0.0) + fits_set_tile_dim +
  fits_img_compress；解压走 fits_movabs_hdu(2) + fits_read_img（透明解压），同样 5/9 次取中位。
- **对照**：python zlib（level 1/9）、lzma（preset 6 / 9|EXTREME）、zstandard（level 3）同语料同口径。
- **计时统计**：一律 **中位数**；所有外部命令带 timeout；单进程内存有界（每瓦片 ≤ 数 MiB）。
- **诚实登记**：测量期间另一 agent 的 e3_real_data.py（单核 100%）与一次 p1drz_thread_pr 在跑，
  系统 load ≈ 1~6（16 线程机）。计时为多次取中位，对该干扰不敏感，但**不是独占机器**的干净数字（§8 第 4 条）。

---

## 2. 战线一：HiPS 稠密数据层（磁盘）

### 2.1 数据形状：稠密数据层其实「很稠密」（这是压缩率的结构性上限）

对**全部 3024 个瓦片**逐字节扫描数据段（code/fill_scan.py → evidence/fill_scan.json）：

| 层 | 阶 | 瓦片数 | NaN 占比 | 零占比 | 有限值占比 |
|---|---|---|---|---|---|
| signal/variance/ivar | 0 | 1 | 0.9980 | 0 | 0.0020 |
| | 5 | 7 | 0.7393 | 0 | 0.2607 |
| | 8 | 152 | 0.2344 | 0 | 0.7656 |
| | **9（叶级）** | **523** | **0.1102** | 0 | **0.8898** |
| | 合计 | 756 | **0.1792** | 0 | **0.8208** |
| support | 全部 | 756 | 0 | **0.1792** | 1.0000 |

- **叶级（占 69% 字节）中 399/523 = 76.3% 的瓦片是 100% 满的**，没有 NaN 空洞可省。
- 于是「压缩率」必须分两问：**(a) 整瓦片（含 NaN 空洞）压多少；(b) 真正的数值载荷压多少。**

### 2.2 压缩率

#### §2.2.1 zstd 各档（真实瓦片数据段，字节加权；evidence/zstd_tiles_shuf0.csv）

| 层 | zstd −7 | zstd −1 | zstd 1 | zstd 3 | zstd 9 | zstd 19 | zstd 22 |
|---|---|---|---|---|---|---|---|
| signal | 0.3657 | 0.3649 | 0.3993 | 0.3991 | 0.3989 | 0.3972 | 0.3972 |
| variance | 0.3674 | 0.3666 | 0.4141 | 0.4141 | 0.4140 | 0.4141 | 0.4141 |
| ivar | 0.3672 | 0.3664 | 0.4138 | 0.4138 | 0.4137 | 0.4139 | 0.4139 |
| support | 0.0026 | 0.0022 | 0.0014 | 0.0014 | 0.0015 | 0.0013 | 0.0013 |
| **四层合计** | **0.3657** | **0.3649** | **0.3072** | **0.3071** | **0.3070** | **0.3066** | **0.3066** |

- **level ≥ 3 后压缩率完全饱和**：1→3 差 0.0001，3→22 差 0.0005。
- 负档（−1~−7）压缩率明显更差（0.365 vs 0.307），**且比正档 1 更慢**（见 2.3），**没有使用价值**。
- **四层合计之所以远好于单层，是因为 support 层几乎全是 0**（压到 0.0014）。单看数值层（signal/variance/ivar），
  zstd 只有 **0.40~0.41**。

#### §2.2.2 字节转置（shuffle）的增益（evidence/zstd_tiles_shuf4.csv）

| 层 | zstd 1 | zstd 3 | zstd 9 | zstd 19 |
|---|---|---|---|---|
| signal | 0.3295 | 0.3294 | 0.3284 | 0.3262 |
| variance | 0.3554 | 0.3540 | 0.3526 | 0.3480 |
| ivar | 0.3551 | 0.3535 | 0.3522 | 0.3476 |
| support | 0.0019 | 0.0017 | 0.0017 | 0.0018 |
| **四层合计** | **0.2605** | **0.2597** | **0.2587** | **0.2559** |

⇒ **shuffle 把四层合计从 0.3071 降到 0.2597（相对省 15.4%）**，且**几乎不增加压缩时间**。
这与 FITS 注册约定对 GZIP_2 的描述一致：「shuffled into decreasing order of significant before being
compressed with gzip. **This is especially effective with floating point arrays**」（[S7]）。

#### §2.2.3 「真正的数值载荷」有多不可压（负例对照的关键）

只取 finite 且非零的元素（即方案 A 真正需要存的数）压缩：

| 载荷 | zstd −1 | zstd 1 | zstd 3 | zstd 9 | zstd 19 |
|---|---|---|---|---|---|
| 全部载荷字节加权 | 0.7506 | 0.6321 | 0.6322 | 0.6321 | 0.6308 |
| **signal 叶级载荷（中位）** | 1.0000 | **0.8109** | 0.8109 | 0.8109 | 0.8046 |
| **variance/ivar 叶级载荷（中位）** | 1.0000 | **0.8540** | 0.8542 | 0.8540 | 0.8547 |

- **负档（−1~−7）在满尾数 f32 载荷上完全不压（1.0000）**——这正是一条「判据非退化」的佐证：
  真正的噪声数据上，快档给不出任何收益。
- 纯 f64 噪声参照：**zstd 0.9584~0.9600 / zlib9 0.9607 / lzma 0.9449**，与 MEM-DESIGN-01 记录的
  zlib 0.96 / lzma 0.945 **逐位一致**（互相验证了测量口径）。

#### §2.2.4 天文领域专用方案：FITS 原生压缩（fpack 口径）

fits_img_compress（cfitsio 4.6.2），tile = 512×512（整图单瓦片），整产物外推（per-(layer,order) 中位 × 真实计数）：

| codec | 说明 | 整产物 ratio | 省 | 逐位一致（语料 195 瓦片） |
|---|---|---|---|---|
| RICE_1 | fpack 默认 −q 4，**有损** | **0.1125** | 2.62 GiB | **20/195** |
| GZIP_1 | fpack 默认 −q 4，**有损** | 0.1594 | 2.48 GiB | 20/195 |
| GZIP_2 | fpack 默认 −q 4，**有损**（shuffle+gzip） | **0.0986** | 2.66 GiB | 20/195 |
| RICE_1_q0 | 无损 Rice | **全部失败（status 413）** | — | — |
| GZIP_1_q0 | 无损（quantize_level=0） | 0.4884 | 1.51 GiB | **103/195** |
| GZIP_2_q0 | 无损（shuffle + quantize_level=0） | **0.4115** | 1.74 GiB | **103/195** |

**三条必须如实报的发现：**

1. **Rice 在有损模式下确实远优于一切通用压缩器（0.1125 vs zstd 0.3974，3.5 倍）——但不可用。**
   实测（code/fits_loss.c，真实叶瓦片）：

   | 瓦片 | codec | 改变的像素 | NaN 被摧毁 | 有限像素最大相对误差 | 中位 |
   |---|---|---|---|---|---|
   | signal_leaf_1371567 | RICE_1(q4) | 262144/262144 | **238553** | **3.08e-3** | 1.42e-3 |
   | signal_leaf_1371848 | RICE_1(q4) | 262144/262144 | **206218** | **1.10e-2** | 1.92e-3 |
   | signal_hier8_342891 | RICE_1(q4) | 262143/262144 | **256192** | 2.15e-3 | 9.44e-4 |

   ⇒ **NaN 掩码被整片摧毁**（DATA_SEMANTICS §12.4 规定 signal 无效像素必须是 IEEE NaN），
   且有限像素带 0.2%~1.1% 的量化误差。对一个「1/N worker 逐位一致」「f64 bitwise」的科学产品，
   这属于**科学变更**（改精度/改无效语义），不是工程选项。

2. **无损 Rice 在 float 图像上不可用。** fits_set_quantize_level(outf, 0.0) + RICE_1 在
   **195/195 个真实瓦片**上返回 status 413 "error compressing image"；同一设置在 GZIP_1/GZIP_2 上正常。
   根因：Rice 需要整数输入，cfitsio 对 float 只能先量化（−q 0 对 Rice 无实现路径）。
   这与 FITS 注册约定把 RICE 归为整数压缩算法一致（[S7]）。
   ⇒ **「Rice 专为浮点设计、可能在数值瓦片上显著优于通用压缩器」这一假设，在 fpack 口径下不成立**：
   它对浮点只能有损，无损路径不存在。**如实报，不为了迎合 zstd 而略过，也不为了迎合 Rice 而掩饰。**

3. **FITS 无损档（GZIP_1/GZIP_2 + quantize_level=0）逐位一致是可以做到的**，但有一个**读端前置条件**：

   | 读端 nulval | 结果（signal_leaf_1371567，238553 个 NaN） |
   |---|---|
   | NULL（cfitsio 默认） | **diff = 238553/262144**（NaN 被替换成 cfitsio 自己的规范 NaN 0xFFFFFFFF） |
   | 显式传入位模式 0x7FC00000 的 float | **diff = 0/262144（逐位一致）** |

   本产物 100% 的 NaN 都是单一模式 0x7FC00000（实测 np.isnan 掩码下 Counter 只有一个值），
   故读端传对 nulval 即可完全还原。**这是读端 API 语义，不是编码限制**——但任何不传 nulval 的
   客户端（含大部分第三方工具）会读到不同的 NaN 位模式。

#### §2.2.5 通用压缩器对照（python 侧，同 188 个真实整瓦片，evidence/py_codecs.csv）

| codec | signal | variance | ivar | support | 压缩 ms/MiB | 解压 ms/MiB | 压缩 MB/s | 解压 MB/s |
|---|---|---|---|---|---|---|---|---|
| zlib 1 | 0.4057 | 0.4230 | 0.4228 | 0.0065 | 17.676 | 1.760 | 56.7 | 568 |
| zlib 9 | 0.4015 | 0.4168 | 0.4166 | 0.0024 | 46.335 | 9.747 | 21.6 | 103 |
| lzma（preset 6） | **0.3292** | **0.3588** | **0.3595** | **0.0013** | 154.290 | 11.797 | 6.5 | 85 |
| **zstd 3** | **0.3991** | **0.4141** | **0.4138** | 0.0014 | **1.414** | **0.278** | **709** | **3,598** |

- **压缩率**：zstd 3 与 zlib9 在数值层几乎相同（0.3991 vs 0.4015，差 0.24 个百分点）；
  **lzma 明显更好**（0.3292，比 zstd 好 7 个百分点）——但代价是 **压缩慢 109 倍**（6.5 vs 709 MB/s）、
  解压慢 42 倍。**lzma 的压缩率优势在磁盘端到端口径下被时间开销完全吃掉**（154 ms/MiB 压缩，
  任何磁盘都不划算）。
- **速度**：zstd 3 比 zlib9 **快 33 倍压缩 / 35 倍解压**，比 lzma **快 109 倍压缩 / 42 倍解压**。
- 这与 MEM-DESIGN-01 在 f64 累加器载荷上的结论方向一致（zlib9 0.90 / lzma 0.82~0.85）：
  **lzma 一直是压缩率冠军，但从来不是速度可接受的选项。**

### 2.3 时长（每 1 MiB 瓦片，中位）

| 方案 | 压缩 ms | 解压 ms | 压缩 MB/s | 解压 MB/s |
|---|---|---|---|---|
| zstd −1 | 0.312 | 0.096 | 3,203 | 10,622 |
| zstd 1 | 0.429 | 0.210 | 2,334 | 4,758 |
| **zstd 3** | **0.687** | **0.197** | **1,456** | **5,084** |
| zstd 9 | 2.091 | 0.193 | 478 | 5,190 |
| zstd 19 | 12.485 | 0.199 | 80 | 5,021 |
| zstd 22 | ~100 | ~0.2 | **10.5** | 4,991 |
| zstd 3 + shuffle | 0.931 | 0.146 | 1,105 | 6,926 |
| zstd 19 + shuffle | 30.515 | 0.139 | 33 | 7,434 |
| GZIP_2_q0（FITS 无损） | 6.101 | 2.809 | 164 | 356 |
| GZIP_1_q0（FITS 无损） | 6.166 | 2.149 | 162 | 465 |
| RICE_1（有损） | 6.061 | 1.914 | 165 | 522 |

- **压缩时长可接受**：zstd 1/3 只要 0.4~0.9 ms/MiB（单线程），16 线程下按瓦片并行可到 ~10 GB/s 量级。
- **level ≥ 9 收益为零、代价线性增长**；**level 19/22 不可接受**（33 / 10 MB/s）。
- **zstd 比 FITS 原生 GZIP 快 6.6×（压缩）/ 19×（解压）**，压缩率还略好（0.3974 vs 0.4115）。
- **shuffle 是「免费」的 15.4% 增益**（转置本身的开销在 codec 计时区间之外）。

### 2.4 端到端：压缩的价值 = 省下的 I/O 时间 > 压缩开销

**判据（冻结）**：瓦片写一次、读 R 次；S=1 MiB、压缩率 ρ、codec 时间 t_c + t_d：
- 净收益 Δ = (1−ρ)·S·(1/BW_w + R/BW_r) − (t_c + t_d)
- **临界带宽** BW* = (1+R)·(1−ρ)·S / (t_c + t_d)（读写同速口径）

本机实测 **BW_w = 419 MiB/s、BW_r = 359 MiB/s**，R=1（写一次 + 下游读一次）：

| 方案 | ρ（整产物） | 省 I/O ms/瓦片 | codec ms/瓦片 | **净收益 ms/瓦片** | **临界带宽 MiB/s**（R=1 / R=2） |
|---|---|---|---|---|---|
| zstd −1 + shuffle | 0.4290 | 2.953 | 0.408 | **+2.545** | 2,799 / 4,199 |
| zstd 1 + shuffle | 0.3983 | 3.112 | 0.685 | **+2.428** | 1,758 / 2,638 |
| **zstd 3 + shuffle** | **0.3974** | **3.117** | **1.077** | **+2.040** | **1,120 / 1,681** |
| zstd 3（无 shuffle） | 0.4722 | 2.730 | 0.884 | +1.846 | 1,194 / 1,791 |
| zstd 9 + shuffle | 0.3961 | 3.123 | 2.624 | +0.500 | 460 / 690 |
| zstd 19 + shuffle | 0.3933 | 3.138 | 30.654 | **−27.516** | 40 / 60 |
| GZIP_2_q0（FITS 标准无损） | 0.4115 | 3.044 | 8.910 | **−5.866** | **132 / 198** |
| GZIP_1_q0（FITS 标准无损） | 0.4884 | 2.646 | 8.315 | −5.669 | 123 / 185 |
| RICE_1（有损） | 0.1125 | 4.591 | 7.975 | −3.384 | 223 / 334 |
| GZIP_2 q4（有损） | 0.0986 | 4.663 | 12.246 | −7.583 | 147 / 221 |

**读法**：
- **zstd 1/3 在本机是明确的净收益**：每瓦片净省 2.0~2.4 ms，即「省下的 I/O 时间 = 压缩开销的 2.9~4.5 倍」。
  只有当存储快于 **1.1~1.8 GiB/s** 时 zstd 才不再划算——本机 419 MiB/s 远低于该门槛，
  典型 SATA SSD（~0.5 GB/s）、HDD（~0.15 GB/s）、网络存储都远低于该门槛。
- **FITS 原生 GZIP 档在本机是净亏**（−5.9 ms/瓦片）：它需要存储慢于 **132 MiB/s** 才划算，只有机械盘/远程存储满足。
- **level 19 净亏 27.5 ms/瓦片**，无论磁盘多慢都不值得（临界带宽只有 40 MiB/s）。

### 2.5 标准兼容性（**必查项；这一条否决方案**）

**问题：IVOA HiPS 标准允许的瓦片格式是什么？zstd 压缩的 FITS 是否符合 HiPS 标准？**

**答：不符合。逐条来源如下。**

**[S1] REC-HIPS-1.0（IVOA Recommendation，2017-05-19）§4.2.1.3 "Format of tiles"（p.12）** —
https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf —— 原文：
> "Three image formats **may** be used to package the HiPS tiles for images: FITS [6], PNG or JPEG.
> … **The tile file extension must correspond to the format: .fits for FITS, .jpg for JPEG, .png for PNG.
> These extensions must be in lowercase.**"

**[S2] 同文档 §3（RFC 2119 语义）**：
> "the keywords "must", "required", "should", and "may" … Mandatory elements are indicated as must …"

⇒ **格式选择是 MAY（FITS/JPEG/PNG 三选），但「扩展名必须与格式对应且小写」是 MUST。**
zstd 压缩后的字节流不再是 FITS（无 SIMPLE/BITMAP 头），叫 .fits 违反该 MUST；叫 .fits.zst 同样违反
（不在允许扩展名集合内）。

**[S3] 同文档 §4.4.1 hips_tile_format（R = 必填）**：
> "hips_tile_format R List of available tile formats. The first one is the default suggested to the client –
> Format: list of word blank separated: **"jpeg", "png", "fits", "tsv"**"

⇒ 词表**封闭**，**没有任何 token 能声明 zstd**。且 §6.1：「Usually, the properties file is read first by the
client in order to know the required HiPS parameters and limits (HiPS deepest order, **tile formats**, …)」
—— 客户端靠这个键决定去哪取什么扩展名的瓦片。本产物 properties 实测为 `hips_tile_format=fits`。

**[S4] 同文档 §5.1 "HiPS server"（p.22）—— 标准允许压缩的唯一位置是传输层**：
> "Note: According to the HTTP site configuration, the tiles, notably the FITS tiles, **may or may not be
> compressed**. So it is up the clients to ensure that the required uncompression step is performed
> (**as can be done transparently by the HTTP libraries**)"

⇒ 这是 **HTTP Content-Encoding 语义**（客户端 HTTP 库透明解压），**不是文件命名/格式授权**。
独立实测佐证：CDS 服务器对同一 .fits URL，默认返回 content-type: application/fits 无 content-encoding；
带 Accept-Encoding: gzip, deflate, br, zstd 时返回 content-encoding: gzip。

**[S5] 同文档 §5.1 —— 归档/容器层也允许**：
> "the actual implementation of HiPS as directories and files is not an obligation, only the view as
> directories and files is required. … a HiPS may be stored in a database, or **any other appropriate method
> for packaging it (tar or zip files…)** rather than a basic file system directory."

**[S6] FITS Standard 4.0 §10「Representations of compressed data」§10.4 Table 36**（经独立核验，
https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf）—— ZCMPTYPE 是**穷举表**
（RICE 1 / GZIP 1 / GZIP 2 / PLIO 1 / HCOMPRESS 1 / NOCOMPRESS），且明文
"if other types are later supported, **they must be registered with the IAUFWG**"。
**zstd 不在表内，无注册记录。** ⇒ 即使把 zstd 塞进 FITS tile-compression 容器也不合法。

**[S7] FITS Registered Convention "Tiled Image Compression Convention" v2.3（2013-07-02，Registered 2007-03）**
—— 依据官方页 https://fits.gsfc.nasa.gov/registry/tilecompression.html —— 原文：
> "Current implementations support GZIP, RICE, H-Compress, and the IRAF pixel list compression algorithms."
> "…added support for a new variant of the GZIP algorithm, called GZIP_2, in which the bytes in the pixel
> array are shuffled into decreasing order of significant before being compressed with gzip.
> **This is especially effective with floating point arrays.**"

（该约定已被 FITS 4.0 §10 正式吸收；§10 引言："the specifications in this Standard shall supersede those
in the registered convention"。）

**[S8] WD-HiPS-2.0-20260501 §4.1.2**（**Working Draft，状态声明明文禁止引用为规范**）
https://www.ivoa.net/documents/HiPS/20260501/WD-HiPS-2.0-20260501.html —— 原文：
> "A HiPS **can use any another formats** if this proves more appropriate, bearing in mind that this will
> **reduce the number of clients able to access such a HiPS**. A good example is the **RICE FITS compression
> alternative** … or **WEBP alternative** …"

**[S9] 同 WD 脚注 1**（经独立核验）：
> "In the case of RICE FITS, the extension should remain ".fits" because it is a compression mode that does
> not change the nature of the file since **this compression has been integrated into the FITS standard**"

⇒ WD 给 RICE 开了口子（因为它已进 FITS 标准正文），**没有给 zstd 开任何口子**（既不在 FITS 标准，
也不是新扩展名格式）。且 WD 是草案，**不能作为规范依据**。

**[S10] WD-HiPS-2.0-20260501 §4.3.2 "Trim reduction for FITS tiles"** —— 标准自身推荐的**零成本**体积削减法：
> "A "trim" operation may be applied on FITS tiles to avoid storing and transporting BLANK, or NaN values
> for areas not concerned by the survey. … the FITS header uses the keywords **TRIM1, TRIM2** to indicate the
> size of the unstored margins … and the original size of the cube will be saved using the **ONAXIS1 and ONAXIS2**
> keywords. … **Unlike traditional compression algorithms such as GZIP or RICE, this method is trivial and does
> not add any additional processing time, either for writing or reading. Plus, it retains direct pixel access
> property.**"

**[S11] Hipsgen User Manual（CDS，Pierre Fernique）** https://aladin.cds.unistra.fr/hips/HipsgenManual.pdf
（经独立核验）：
> "Hipsgen offers two possibilities to reduce the size of FITS tiles: edge removal and/or compression.
> **These two methods should only be used when strictly necessary. They are not standardized by the IVOA**
> and are still subject to change, and the FITS tiles produced are **currently only recognised by Aladin Desktop**."
> "**Internal FITS 4.0 compressions:** … using the "fpack" utility in post-processing. … **This alternative method
> produces HiPS that complies with the IVOA 1.0 standard.** However, tiles compressed in this way are currently
> only supported by HiPS Aladin Desktop clients version >= 12.61."

⇒ **CDS 官方只对 fpack/RICE 声明「符合 IVOA 1.0 标准」，对 gzip/zstd 明确写「not standardized by the IVOA」。**

**[S12] 客户端生态实测（经独立核验，证据全文见 evidence/IVOA_HiPS_tile_format_report.md，600 行 / 21 条）**：
- Aladin Desktop 手册支持表：HCOMP / FITS-RICE / FITS-GZIP；命令行「images: FITS (gzipped, bzipped, RICE, MEF, …)」。
- Aladin Lite v3 源码：ImageExt { Fits, Jpeg, Png, Webp, fits.fz }；ImageExt::FitsFz => todo!()（**未实现**）；
  GzReader 只出现在独立 FITS 图像加载路径，**HiPS tile 下载路径不经过它**。
- fitsrs（CDS 纯 Rust FITS 库）：支持 GZIP / GZIP2 / RICE（u8/i16/i32/f32）；**无 zstd**。
- **HiPS 1.0、HiPS 2.0 WD、Hipsgen 手册、IVOA 文档总索引全文检索 zstd|zstandard：全部 0 命中。**

**⇒ 标准兼容性结论（明确写）**

> **zstd 压缩的 FITS 瓦片不可作为 HiPS 产品的默认格式。**
> 理由（按否决力排序）：① REC-HIPS-1.0 §4.2.1.3 扩展名 MUST + §4.4.1 hips_tile_format 词表封闭
> ⇒ **规范层面不符合**；② FITS 4.0 §10.4 Table 36 穷举 + 注册要求 ⇒ **即使换容器也不合法**；
> ③ 生态零支持（Aladin 系 / fitsrs / Hipsgen 全无 zstd）⇒ **不可互操作**。
>
> **符合标准的替代（按推荐度）**：
> 1. **传输层压缩**（HTTP Content-Encoding: zstd/gzip/br，由 Web 服务器协商）——REC-HIPS-1.0 §5.1 Note
>    明确覆盖；文件仍是标准 .fits，hips_tile_format 不动，**零产品改动、完全合规**。
>    （Astro Celestial Sphere Database（ACSD） 当前不发布 HTTP 服务，此项属部署侧。）
> 2. **归档/分发容器**（tar.zst / zip）——§5.1 明确允许「any other appropriate method for packaging it」。
> 3. **产品文件内压缩（若确实必要）**：FITS 4.0 §10 tile-compression + quantize_level=0（无损），
>    保持 .fits 扩展名（WD 脚注 1 认可 RICE 路径；Hipsgen 对 fpack 判定「符合 IVOA 1.0」）。
>    **代价**：客户端支持面窄（Aladin Desktop ≥12.61）；HDU 类型从 IMAGE 变 BINTABLE；
>    **实测在本机磁盘上净亏 5.9 ms/瓦片**（§2.4）。
> 4. **零成本替代：TRIM**（WD §4.3.2）。**实测收益见 §2.6**。

### 2.6 TRIM 实测（标准内的零成本体积削减）

code/trim_scan.py 对 **1512 个 signal + support 瓦片**计算「有效域包围盒」：

| 层 | 阶 | 瓦片数 | trim 后/前 | 中位包围盒面积比 |
|---|---|---|---|---|
| signal/support | 0 | 1 | **0.0039** | 0.0039 |
| | 3 | 4 | 0.0480 | 0.0454 |
| | 5 | 7 | 0.3967 | 0.3508 |
| | 7 | 46 | 0.7459 | 1.0000 |
| | 8 | 152 | 0.8218 | 1.0000 |
| | **9（叶级）** | **523** | **0.9238** | 1.0000 |
| | **合计** | **1512** | **0.8669（省 13.3%）** | |

- **整体省 13.3%**，**零 CPU、保留 .fits、保留随机访问、无需解码**。
- 大头在低阶（order 0 省 99.6%），但低阶只有 20 个瓦片；叶级只有 7.6%（因为 76% 的叶瓦片本来就满）。
- 代价：TRIM 关键字由 WD §4.3.2 定义（**草案**），Hipsgen 手册自述「not standardized by the IVOA…
  currently only recognised by Aladin Desktop」；且读端补 NaN 的位模式同样是实现定义（同 §2.2.4 第 3 条）。

### 2.7 战线一结论

| 问题 | 答案 |
|---|---|
| **zstd 压缩率好不好** | **好**：整产物 0.3974（level 3 + shuffle）/ 0.4722（level 3）；四层中 support 近乎免费 |
| **压缩时长可接受吗** | **可接受**：level 3 单线程 0.69 ms/MiB（无 shuffle）/ 0.93 ms（含转置），level 1 更快 |
| **端到端有净收益吗** | **有**：本机磁盘（419/359 MiB/s）下净省 **+2.04 ms/瓦片**；临界带宽 **1,120 MiB/s** |
| **能作为默认吗** | **不能**——REC-HIPS-1.0 §4.2.1.3/§4.4.1 MUST 否决；FITS 4.0 §10.4 表穷举；生态零支持 |
| **适用条件（若用在合规位置）** | 传输层（HTTP Content-Encoding）或归档容器；**level 1~3**（3 优于 1，1 更快）；**必配 byte-shuffle**；level ≥ 9 无收益，19/22 不可用 |
| **标准内该优先做什么** | ① 传输层压缩（部署侧）；② 若必须压产品文件：FITS GZIP_2 + quantize_level=0（无损、逐位一致、但本机净亏）；③ **TRIM（省 13.3%，零 CPU）** |

---

## 3. 战线二：内存累加器（看时间开销）

### 3.1 口径与自检

复用 MEM-DESIGN-01 的**忠实/代理口径**（code/acc_roundtrip.py，与
run/MEM-DESIGN-01/verify/compress_faithful.py 同一重建运算）：
由真实叶级发布面按 aio_hips_writer.cpp:1341-1355 的逐位运算重建祖先 cell 的 f64 累加器：

    z = ((s<<18) | i) >> 2·dk ;  flux = (double)(float)signal × (double)(float)area
    area = (double)(float)area ;  acc[z] += flux / area     （叶按 ipix 升序、i 升序 = 生产同序）

**规模**：**233 个真实祖先 cell**、**4707 次就地更新**（523 叶 × 9 阶）、叶面缓存 1046 MiB。

**自检（判据非退化）**：np.add.at 与显式顺序循环**逐位一致 = True**；对结果注入一个 nextafter 后
同一判据**判红 = True**。⇒ 计时与累加口径可信。

### 3.2 压缩率

| 通道 | 口径 | zstd 1 | zstd 3 | zstd 9 | zstd 19 | zlib9 | lzma |
|---|---|---|---|---|---|---|---|
| flux | 稠密整张（2 MiB，含未触碰的 0） | 0.6054 | 0.6045 | 0.6047 | 0.5973 | 0.6054 | 0.5350 |
| flux | **方案 A 载荷（只存非零元素）** | **0.8017** | **0.8000** | 0.8009 | **0.7889** | — | — |
| area | 稠密整张 | — | **0.0027** | — | — | 0.0045 | 0.0025 |
| 纯 f64 噪声（下界参照） | — | 0.9584 | 0.9600 | 0.9584 | 0.9589 | 0.9607 | 0.9449 |

- **可压的就是那 20%**：zstd 3 把数值载荷从 1.0 压到 **0.8000**（收益 **20.0%**），**恰好卡在
  MEM-DESIGN-01 设定的 20% 门槛线上**；zstd 19 再多 1.1 个百分点（0.7889）。
- **稠密整张的 0.60 是假象**：省的全是「未触碰的 0」，而方案 A 已经**连零都不分配**（0 字节 < 0.1% 字节）。
- **area 通道确实可压到 0.0027**——但 area 只占 1/3 字节，且方案 A 已把它降到 0.756 倍、B 又把驻留降到 8.7%。
- 纯 f64 噪声参照 0.9584~0.9600 与 MEM-DESIGN-01 的 0.96 一致 ⇒ **满尾数 f64 本就近乎不可压**，
  0.80 的收益全部来自「约 20% 的零/稀疏结构」，不是浮点数的可压性。

### 3.3 每次就地更新的摊薄成本（**核心数字**）

累加器是**被反复就地更新的数组**：每来一个叶瓦片就要对同一块内存再写一次。
要压内存必须走 **解压 → 改 → 重压**，代价 ∝ 载荷 × 更新次数。实测（单 cell 2 MiB f64 buffer，中位）：

| 档 | 压缩 ms | 解压 ms | **往返 ms/次更新** | **全产物 4707 次更新** |
|---|---|---|---|---|
| zstd 1 | 2.849 | 1.800 | **4.649** | **+21.9 s** |
| **zstd 3** | 4.106 | 1.920 | **6.026** | **+28.4 s** |
| zstd 9 | 4.862 | 1.798 | 6.660 | +31.4 s |
| zstd 19 | 268.781 | 2.282 | **271.063** | **+1275.9 s** |

**对照基准**：MEM-DESIGN-01 §5.1 实测优化后 writer 重放真实产物的 **user time = 27.68 s**（墙钟 1:50.68）。

⇒ **zstd 3 会让 writer 的 CPU 时间翻倍（+28.4 s ≈ +103%）；zstd 19 是 +46 倍。**

### 3.4 与「稀疏分块 + 流式写出」的对照（能不能再进一步？）

| 方案 | 峰值驻留 | 相对基线 | 额外 CPU | 逐字节一致 |
|---|---|---|---|---|
| 基线（稠密 3ch f64 + count，全驻留） | 1631.0 MiB（实测 RSS 1708.0 MB） | 100% | — | — |
| MEM-DESIGN-01 已选：**A 稀疏分块 + C 按需通道 + B(cons) 完备即流式写出** | **142.4 MiB**（实测 RSS 186.9 MB） | **8.7%** | 仅稀疏块查表（user time +11%） | **是**（3033 文件 0 差异） |
| 在其上再加 zstd 3（flux 载荷 0.8000、area 0.0027） | ≈ 142.4 × 0.87 ≈ **124 MiB**（估） | ≈ 7.6% | **+28.4 s（+103% writer CPU）** | **是**（zstd 是纯字节无损编码器） |

- **zstd 能保持逐字节一致**——它是纯字节无损编码器，「解压-改-重压」的往返是逐位恒等的。
  **所以否决它的不是正确性，是收益/代价比。**
- **收益上界**：A+C 已把可压的 flux 载荷降到 352.4 MiB（总 466.0 MiB 的 0.756），压缩再省 20%
  ⇒ 相对基线省 0.756 × 0.20 = 15.1% 的 flux 字节；但 **B 已经把「完成的 cell」整体写出并释放（0 字节）**，
  驻留的只剩「尚未完备」的 89 个 cell。压缩只能作用在这 142.4 MiB 上，**最多再省约 18 MiB（1.1% 基线）**。
- **0 字节 < 压缩后的任何字节**：对已完备的 cell，流式写出给的是**零驻留**，压缩给的是 0.80×，**严格更劣**。
- 与 ASTROCS_DESIGN.md §8.3 一致：「异步只用于能隐藏延迟的 I/O、预取与**压缩**」——
  内存累加器的「解压-改-重压」是**纯 CPU 同步路径**，不属于可隐藏的 I/O。

### 3.5 战线二结论

> **不采用。** 三条理由（都可复跑）：
> 1. **收益只有 20.0%，恰在门槛线上**（zstd 3 载荷 0.8000；zstd 19 为 0.7889 但往返 271 ms/次，不可用）。
> 2. **代价是 writer CPU 翻倍**（+28.4 s vs 27.68 s 基准）；档位越高越糟（zstd 19 = +1276 s）。
> 3. **结构上不相容**：累加器是「就地反复更新」的数组，压缩必须在每次更新做「解压-改-重压」；
>    而 **B 已经把完备 cell 整体写出并释放（0 字节）**——0 字节严格优于任何压缩后字节。
>    压缩能作用的空间只剩 142.4 MiB 的「未完备驻留」，**上界约 18 MiB（基线 1.1%）**。

---

## 4. 与已有结论的对照

| 已有结论（MEM-DESIGN-01 / FIX-403） | 本次实测 | 一致性 |
|---|---|---|
| 方案 D 压缩：zlib9 = 0.90、lzma = 0.82~0.85（载荷） | zstd 1/3/9 = **0.8000~0.8017**、zstd 19 = 0.7889 | **一致且更优**：zstd 比 zlib9 好 10 个点、与 lzma 同档，但**快 2~3 个数量级** |
| 纯 f64 噪声参照 zlib 0.96 / lzma 0.945 | zlib9 **0.9607** / lzma **0.9449** / zstd **0.9584~0.9600** | **逐位吻合**（互相验证口径） |
| 累加器 233 cell / 523 叶 / 1708.0 MB 基线 / 186.9 MB 优化后 | 233 cell / 523 叶 / 解析 466.0 MiB(flux) / 4707 次更新 | **一致** |
| 收益 < 20% 门槛 ⇒ 不采用 D | zstd 恰好 20.0%（0.8000），**仍不采用**（理由升级为「结构不相容 + CPU 翻倍」） | **一致**（结论不变，理由更强） |
| A+C+B(cons) 峰值 142.4 MiB、逐字节一致 | 未重跑（只读复用其结论作为对照基准） | 接受为前提（§8 第 6 条） |
| 仓内先例 HEALPIX_FORMAT_SPEC.md §5.4「数组不压缩…可在传输层或归档层处理」 | 本次独立得出同向结论（标准也只允许传输层/容器层压缩） | **一致**（但该页「zstd 压缩比有限（2:1 左右）」的说法在**数值载荷上过于乐观**：实测 signal/variance/ivar 数值层只有 0.40~0.41，载荷 0.80；2:1 只对 support 层成立） |

**新增（MEM-DESIGN-01 未测）**：zstd 全档位、byte-shuffle、FITS 原生 Rice/GZIP_1/GZIP_2、
TRIM、标准兼容性、端到端临界带宽。

---

## 5. 明确建议

### 5.1 磁盘（HiPS 稠密数据层）

| 决定 | 内容 |
|---|---|
| **是否默认启用 zstd** | **否，明确「不可作为默认」**。理由：REC-HIPS-1.0 §4.2.1.3 扩展名 MUST（[S1][S2]）+ §4.4.1 hips_tile_format 词表封闭（[S3]）+ FITS 4.0 §10.4 Table 36 穷举与注册要求（[S6]）+ 生态零支持（[S12]） |
| **合规的压缩位置** | ① **HTTP 传输层 Content-Encoding**（§5.1 Note，[S4]）——零产品改动；② **归档/分发容器**（§5.1 tar/zip，[S5]） |
| **若在传输/归档层用 zstd：level 取值** | **level 3**（推荐）或 **level 1**（更快的等价选项）。**必须配 byte-shuffle**。依据：1→3 压缩率差 0.0001；3 比 1 慢 1.6×；level ≥ 9 收益为零；19/22 不可用（33/10 MB/s） |
| **若必须在产品文件内压磁盘** | 唯一标准内无损选项 = **FITS 4.0 §10 tile-compression + GZIP_2 + quantize_level=0**（保持 .fits）。实测**逐位一致**（读端必须传 nulval=0x7FC00000 位模式）。**但本机净亏 5.9 ms/瓦片**（临界带宽 132 MiB/s）⇒ **仅在机械盘/远程存储场景考虑**，且属产品格式变更，需走合同变更流程 |
| **标准内的零成本替代（推荐先做）** | **TRIM**（WD §4.3.2，[S10]）：实测整产物 **省 13.3%**，零 CPU、保留随机访问。同为草案特性，客户端支持面需先确认 |
| **明确不做** | ① RICE_1/GZIP_1/GZIP_2 的 **fpack 默认 −q 4 有损档**——摧毁 NaN 掩码 + 0.2%~1.1% 数值误差，属科学变更；② .fits.zst / .fits.gz 文件名；③ zstd 进 ZCMPTYPE 容器 |

### 5.2 内存累加器

| 决定 | 内容 |
|---|---|
| **是否采用 zstd** | **不采用**。收益 20.0%（卡门槛线）、writer CPU +103%、且 B 流式写出已给出更优的「0 字节」 |
| **保持现状** | A 稀疏分块 64×64 + C 按需通道 + B(cons) 完备即流式写出（峰值 1708.0 → 186.9 MB，逐字节一致） |
| **若将来内存预算再收紧** | 优先做 **B(exact) 精确判据**（MEM-DESIGN-01 已量化：26.9 MiB = 基线 1.6%，需新增 API + 改合同页），**不要走压缩** |

### 5.3 需负责人裁决的事项

1. **TRIM 是否纳入产品格式**（省 13.3%、零成本，但属 HiPS 2.0 WD 草案特性，客户端支持面待确认）；
2. **是否要提供传输层压缩的部署指引**（ACSD 当前不发布 HTTP 服务，此项属交付文档/部署侧）；
3. 本文**不宣布任何发布决定**（ASTROCS_DESIGN.md §13：发布决定只属负责人）。

---

## 6. 负例红/绿（判据非退化）

| 判据 | **绿（正常路径）** | **红（注入 / 负例）** | 证据 |
|---|---|---|---|
| **全零块压缩率** | zstd 全档 0.000048~0.000054（1 MiB 全零 → 49~57 B）；zlib9 0.00098 | 若全零块压缩率 ≥0.01 说明口径坏 | evidence/zstd_tiles_shuf0.csv ctl_zeros.bin |
| **均匀常数 NaN 块** | zstd 全档 0.00005；RICE/GZIP 0.0110 | 同上 | ctl_const_nan.bin / ctl_const_f32.bin |
| **纯随机 f32（不可压上界）** | zstd 1~22 **全部 ≥0.9242**；负档 = **1.0000** | 若 <0.9 说明口径坏 | ctl_rand_f32.bin |
| **纯 f64 噪声（跨实现对照）** | zstd 0.9584~0.9600 / zlib9 0.9607 / lzma 0.9449 | 与 MEM-DESIGN-01 的 0.96/0.945 **逐位吻合** | ctl_rand_f64.bin |
| **np.add.at ≡ 顺序循环（累加口径）** | **True（逐位）** | 注入 nextafter ⇒ **判红 True** | logs/acc_roundtrip.txt 第 1 行 |
| **FITS 无损档 NaN 往返** | 显式 nulval=0x7FC00000 ⇒ **diff = 0/262144** | nulval=NULL ⇒ **diff = 238553/262144** | code/rt_diff.c |
| **FITS 无损档在无 NaN 瓦片上** | 稠密瓦片 GZIP_1_q0/GZIP_2_q0 **diff = 0**（真值无效应 ⇒ 归零） | — | code/fits_loss.c |
| **FITS 有损档（fpack 默认 q4）** | — | RICE_1：**NaN 掩码 238553 像素被毁** + 有限像素 max_rel **1.10e-2** | code/fits_loss.c |
| **无损 Rice 是否可用** | — | RICE_1 + quantize_level=0 ⇒ **195/195 瓦片 status 413 失败** | evidence/fits_codecs.csv |
| **FITS 压缩往返校验** | GZIP_2_q0 在 47/47 个 support 瓦片上逐位一致 | RICE_1/GZIP_*(q4) 在 signal/variance 上 **0/47** 逐位一致 | 同上 |

**判据自检结论**：全零/常数块确实近乎免费（说明测量链路正确）、纯噪声确实不可压（说明没有虚报收益）、
NaN 往返在传对 nulval 时归零而在不传时判红（说明该判据能红能绿）、无损 Rice 明确失败（说明「Rice 更优」
不是被漏测）。**没有一条恒真门。**

---

## 7. 复现命令

**入库位置**（原始 315 MB 基准产物属 gitignore 的 `run/` 产物、可能已被 `run_gc` 回收，**未入库**）：

| 内容 | 位置 |
|---|---|
| 本评估结论（zstd / Rice / TRIM / 内存累加器） | `docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md`（本文） |
| 标准兼容性查证全文（21 条带英文原文引文） | `docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` |
| 机器可读证据锚 | `artifacts/evidence/compress-01/`：`final_numbers.json`、`fill_scan.json`、`trim_scan.json`、`acc_roundtrip.json`、`product_level.json`、`verify/final_numbers.py`、`MANIFEST.sha256` |
| 全部复现脚本 | `artifacts/evidence/compress-01/code/` |
| 未入库（可由下列命令重建） | `data/`（1 MiB 级语料 288 MiB）、`evidence/*.csv`（原始基准数据）、`bin/`（编译产物）、`logs/` |

脚本内的相对路径以 `run/COMPRESS-01/` 为工作根，故复现先按步骤 0 把归档的 `code/` 还原进该工作区，再按步骤 1~8 重跑。

    cd "<仓库根>"

    # --- 0. 重建 run 工作区 + 构建小工具 (纯 C, 链接系统 libzstd / libcfitsio; 不触碰仓库构建树) ---
    EV=artifacts/evidence/compress-01
    mkdir -p run/COMPRESS-01/{code,bin,data,evidence,logs,verify}
    cp -r "$EV"/code/. run/COMPRESS-01/code/
    cp "$EV"/verify/final_numbers.py run/COMPRESS-01/verify/
    bash run/COMPRESS-01/code/build.sh

    # --- 1. 语料提取 (只读真实产物 run/PERF-401/out/real16_w1) + 文件清单 ---
    timeout 900  python3 run/COMPRESS-01/code/extract_corpus.py
    timeout 120  python3 run/COMPRESS-01/code/make_filelist.py

    # --- 2. 填充率 / TRIM 扫描 (只读全部 3024 瓦片) ---
    timeout 900  python3 run/COMPRESS-01/code/fill_scan.py
    timeout 1200 python3 run/COMPRESS-01/code/trim_scan.py

    # --- 3. 战线一主基准 (zstd 各档 / shuffle / 整 .fits / FITS 原生) ---
    bash run/COMPRESS-01/code/run_bench.sh      # 步骤 1-2 (数据段, shuffle 0)
    bash run/COMPRESS-01/code/run_bench3.sh     # 步骤 3-5 (整 .fits + FITS 原生)
    cd run/COMPRESS-01 && timeout 2400 ./bin/zstd_bench data/*.bin \
        --levels -1,1,3,9,19 --reps-c 3 --reps-d 5 --shuffle 4 \
        --csv evidence/zstd_tiles_shuf4.csv

    # --- 4. python 侧对照 (zlib/lzma/zstandard) ---
    PYTHONPATH=run/COMPRESS-01/pylib timeout 3000 python3 run/COMPRESS-01/code/py_bench.py

    # --- 5. 逐位一致 / 有损误差诊断 ---
    cd run/COMPRESS-01
    COMPRESS01_TMP=/tmp ./bin/rt_diff  data/signal_leaf_1371567.bin GZIP_2_q0   # 显式 nulval -> diff=0
    COMPRESS01_TMP=/tmp ./bin/fits_loss data/signal_leaf_1371567.bin RICE_1     # NaN 被毁 + max_rel

    # --- 6. 战线二: 内存累加器往返成本 ---
    cd "<仓库根>"
    PYTHONPATH=run/COMPRESS-01/pylib timeout 3000 python3 run/COMPRESS-01/code/acc_roundtrip.py \
        run/PERF-401/out/real16_w1 run/COMPRESS-01/evidence

    # --- 7. 磁盘吞吐 + 最终数字汇总 ---
    bash run/COMPRESS-01/code/disk_bench.sh
    python3 run/COMPRESS-01/code/product_level.py
    python3 run/COMPRESS-01/verify/final_numbers.py   # 读 evidence/*.csv (步骤 3-4) + fill_scan.json (步骤 2)

    # --- 8. 标准兼容性查证 (无脚本; 21 条带引文证据见) ---
    # docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md

**环境准备（一次性）**：本机 python 无 zstandard 模块且 python3 -m venv 不可用（缺 ensurepip），
故用 python3 -m pip install --target run/COMPRESS-01/pylib zstandard（0.25.0，封装 libzstd 1.5.7）。

**数字自洽性**：`artifacts/evidence/compress-01/final_numbers.json` 是本文 §2/§3 全部整产物数字的唯一来源；
其复跑依赖 `evidence/*.csv`（未入库，由步骤 3-4 生成）与 `fill_scan.json`（已入库，由步骤 2 生成），
即"重跑基准 → 重算汇总"两段都可由上述命令完整重建。

---

## 8. 诚实边界与未核实项

1. **fpack/funpack 二进制未安装**：which fpack funpack 为空。本文用 **cfitsio 4.6.2 的
   fits_img_compress / fits_read_img**（fpack 内部调用的同一内核）作为「fpack 口径」的等价实现。
   **未验证**：真实 fpack 命令行在默认参数下的输出与本次结果逐字节一致（预期一致，未实测）。
2. **quantize_level=0 + RICE_1 的失败是 cfitsio 4.6.2 的实测行为**（195/195 瓦片 status 413），
   不是从文档推出的。**未核实**：是否存在其他实现（如 fpack 的特殊参数组合、或更新版 cfitsio）
   支持无损 Rice on float；也未核实 fpack Users Guide 的原文措辞（该 PDF 在本机网络下 404）。
3. **代理口径（战线二）**：累加器内容是由**已发布**的 f32 叶面（signal 已归一、support 已按 uint8 面积比
   量化）按生产运算重建的，**不是原始 f64 累加前量**（与 MEM-DESIGN-01 §7.5 同一局限）。
   佐证其稳健性：纯 f64 噪声参照与 MEM-DESIGN-01 逐位吻合（0.9607/0.9449）。
   **未实测**：在真实 f64 累加器内存镜像上直接测压缩率。
4. **计时受并发干扰**：测量期间有另一 agent 的单核 100% 任务与一次 p1drz_thread_pr 在跑，
   load ≈ 1~6（16 线程机）。所有数字为 **5~15 次取中位**，对干扰不敏感，但**不是独占机器**的干净数字。
   建议对 level 1/3 的关键数字在独占环境下复测一次。
5. **hips_tile_format 与 NaN 位模式**：本次认定 zstd 违规的核心依据是 [S1]/[S3] 的 MUST 与封闭词表，
   已从 IVOA 官方 PDF 原文逐字核对。**未核实**：IVOA 是否存在非公开的「扩展机制」（如注册新 token 的流程）。
   对 IVOA wiki 的检索返回 403 需鉴权，未能覆盖。
6. **GZIP_*_q0 的「逐位一致」依赖读端传 nulval**：本文实测了 cfitsio 的行为差异
   （NULL ⇒ diff=238553；显式 0x7FC00000 ⇒ diff=0）。**未核实**：其他 FITS 库（astropy 的
   CompImageHDU、fitsrs、JS 实现）读回时给出什么 NaN 位模式；也未验证真实 fpack+funpack 往返。
7. **A+C+B(cons) 的 142.4 MiB / 186.9 MB 未重跑**：直接引用 MEM-DESIGN-01 的实测与解析值作为对照基准，
   未独立复核（其真实产物 run/RELEASE-02/L4-rebuild/mosaic_out_w1 已被 run_gc 回收）。
8. **TRIM 收益只测了 signal + support 两层**（1512 瓦片）；variance/ivar 的 trim 收益未测（有效域与 signal 同构，
   预期一致，**未实测**）。TRIM 的实现代价（写 TRIM1/TRIM2/ONAXIS1/ONAXIS2 + 读端补边）也未评估。
9. **端到端临界带宽是「写一次 + 读 R 次」的模型**：未包含文件系统元数据、page cache、
   并发争用与 fsync 开销。本机 /workspace 是虚拟块设备（/dev/vdb1），
   **未核实**其与生产平台（Windows x64 / Linux amd64 正式节点）的 I/O 特性是否可比。
---

## 9. 证据索引与归档位置

**已入库**（`artifacts/evidence/compress-01/`；逐文件 SHA-256 见该目录 `MANIFEST.sha256`）：

| 文件 | 内容 |
|---|---|
| `final_numbers.json` | **最终汇总数字（本文 §2/§3 整产物数字的唯一来源）** |
| `fill_scan.json` | 全部 3024 个真实瓦片的 NaN/零/有限值占比 |
| `trim_scan.json` | TRIM 包围盒收益（1512 瓦片） |
| `acc_roundtrip.json` | 233 cell 累加器压缩率 + 往返成本 |
| `product_level.json` | 整产物外推（按真实瓦片计数加权） |
| `verify/final_numbers.py` | 最终数字汇总脚本（输入见 §7 步骤 7） |
| `code/` | 全部复现脚本：zstd_bench.c / fits_bench.c / rt_diff.c / fits_loss.c / diag_cfitsio.c / extract_corpus.py / fill_scan.py / trim_scan.py / acc_roundtrip.py / py_bench.py / product_level.py / analyze.py / make_filelist.py / disk_bench.sh / build.sh / run_bench.sh / run_bench2.sh / run_bench3.sh |

**未入库、可由 §7 重建**（原 `run/COMPRESS-01/` 下的 gitignore 产物）：

| 文件 | 内容 |
|---|---|
| `evidence/zstd_tiles_shuf0.csv` | zstd 12 档 × 383 输入（数据段，无 shuffle），4596 行 |
| `evidence/zstd_tiles_shuf4.csv` | zstd 5 档 × 383 输入（数据段，shuffle=4），1915 行 |
| `evidence/zstd_wholefits.csv` | zstd 11 档 × 152 个真实整 .fits 文件 |
| `evidence/fits_codecs.csv` | FITS 原生 6 codec × 383 输入，tile=512×512 |
| `evidence/fits_codecs_defaulttile.csv` | 同上，tile=default（fpack 默认行瓦片） |
| `evidence/py_codecs.csv` | python zlib1/zlib9/lzma/lzma9/zstd3 对照 |
| `logs/*.txt` / `logs/*.err` | 各基准 stdout/stderr（含 RICE_1_q0 的 413 失败记录） |
| `data/` | 1 MiB 级语料（188 真实瓦片数据段 + 载荷副本 + 7 个对照/负例块，383 输入 / 288 MiB） |
| `files_real_fits.txt` | 152 个真实整 .fits 文件清单（`make_filelist.py` 重建） |
| `bin/` | 编译产物（`build.sh` 重建） |
| `evidence/tables.md` | 分层/分阶原始数字汇总表（`analyze.py` 生成；已入库为 `docs/research/COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md`） |
