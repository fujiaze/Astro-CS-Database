# Stage1/HISS 修复任务分配

> 每个工作包分配一个子代理，用合成数据进行单元测试。
> 完成后由集成子代理进行集成测试。

## 公共约定

所有子代理必须先阅读 `docs/stage1_fix/00_COMMON_CONTRACTS.md`，严格遵守接口定义和数据结构。

## 工作包定义

### WP-A: Tile模型修正（步骤1-2）

**依赖**：无
**文件**：
- 新增：`lib/astro_image_io/src/hiss_tile_model.h/.cpp`
- 修改：`lib/astro_image_io/src/hiss_common.cpp`（signal/support输出）
- 修改：`lib/astro_image_io/include/hiss_format.h`（Tile几何结构）

**任务**：
1. 实现Tile几何计算：`d = min(9, log2(NSIDE/16))`，`n_leaf_per_tile = 4^d`
2. 实现`local_to_global`/`global_to_local`映射
3. 修正`hiss_common.cpp`中signal/support输出：
   - `signal[p] = float(sumFlux)`（累计通量，不除面积）
   - `support[p] = round(255 * sumArea / A_p)`（面积比）
4. 单元测试：`lib/astro_image_io/tests/test_tile_model.cpp`
   - NSIDE=64/tile_nside=16 → 16叶像素
   - NSIDE=32768/tile_nside=64 → 262144叶像素
   - signal/support语义验证

**验收**：单元测试通过，n_leaf=4^d（不是tile_nside²×12）

---

### WP-B: NSIDE+pixfrac校验（步骤5-6）

**依赖**：无
**文件**：
- 修改：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp/.h`

**任务**：
1. `compute_auto_nside`：上限提升到`2^22`，至少9×9网格采样
2. `drizzle()`入口校验：
   - `pixfrac<=0`或`pixfrac>1` → 返回错误
   - `nested=false` → 返回错误
3. 移除"点采样快速路径"
4. 单元测试：
   - 0.1"输入 → NSIDE>=2^21
   - `pixfrac=0` → 错误
   - `pixfrac=1.5` → 错误
   - `nested=false` → 错误

**验收**：非法参数被拒绝，NSIDE支持2^22

---

### WP-C: Gaia测光比例应用（步骤9）

**依赖**：无
**文件**：
- 新增：`lib/calibration/src/photometry_apply.h/.cpp`
- 修改：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`（应用photscal）
- 修改：`lib/astro_image_io/src/hiss_writer.cpp`（PHOTAPPL/PHOTSCAL/BUNIT校验）

**任务**：
1. 实现`I_photo = k_photo * I_cal`在Drizzle前应用
2. Writer元数据：`PHOTSCAL`记录比例，`PHOTAPPL=TRUE`
3. 若`apply_photometry=false`但`BUNIT=ASTROCS_RELATIVE_FLUX` → Writer拒绝
4. 单元测试：
   - photscal=2.0 → signal×2.0
   - apply_photometry=false + BUNIT=ASTROCS_RELATIVE_FLUX → 拒绝

**验收**：测光比例真正应用到signal

---

### WP-D: 球面重叠+候选查询（步骤3-4）

**依赖**：WP-A（Tile模型完成）
**文件**：
- 新增：`lib/healpix_db/healpix_drizzle/spherical_overlap.h/.cpp`
- 修改：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`（替换切平面近似）

**任务**：
1. 实现球面多边形面积（球面excess公式）
2. 实现球面drop与HEALPix像素重叠面积计算
3. 实现候选像素查询（不限于1-ring）
4. `drizzle_engine.cpp`的`processPixel`改用球面重叠
5. 单元测试：`lib/healpix_db/healpix_drizzle/tests/test_spherical_overlap.cpp`
   - 已知球面多边形面积验证
   - 通量守恒：drop未截断Σsignal=L_j（误差<1e-5）
   - 极区/大视场不异常
   - 高NSIDE候选数>48

**验收**：球面重叠误差<1e-6，通量守恒

---

### WP-E: Writer核心改造（步骤7,8,10,11）

**依赖**：WP-A（Tile模型），WP-C（测光应用）
**文件**：
- 新增：`lib/astro_image_io/src/hiss_stream_writer.h/.cpp`
- 修改：`lib/astro_image_io/src/hiss_writer.cpp`
- 修改：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`（writeHis接入）
- 修改：`lib/astro_image_io/src/healpix/aio_healpix_io.cpp`（旧接口改造）

**任务**：
1. signal直接保存累计通量（步骤7）
2. `DrizzleEngine::writeHis()`调用新HissWriter（步骤8）
3. 移除元数据完整WCS（cd/crval/crpix/sip）
4. 旧`aio_hiss_write/read`改造成新Writer/Reader后端
5. 流式写入：Tile压缩后立即写入临时池（步骤10）
6. BITMAP/SPARSE只保存有效signal/support（步骤11）
7. occupancy模式Writer自动选择（不由调用方传入）
8. 单元测试：
   - signal=累计通量（不除面积）
   - BITMAP体积<FULL体积
   - SPARSE体积<FULL体积
   - 1000 Tile内存峰值不增长

**验收**：Writer接入DrizzleEngine，流式写入，BITMAP/SPARSE节省空间

---

### WP-F: SNR+未知块修复（步骤13）

**依赖**：WP-A（Tile模型）
**文件**：
- 修改：`lib/astro_image_io/src/hiss_writer.cpp`（SNR布局）
- 修改：`lib/astro_image_io/src/hiss_reader.cpp`（SNR布局+未知块拒绝）

**任务**：
1. SNR子块布局：`[n_points:u32][points:n*8B]`
2. 每点：`local_ipix(u32)+snr(f32)`
3. 移除snr_phot/median_snr/idw_power
4. Reader遇到未知必需子块返回`HISS_ERR_UNKNOWN_REQUIRED`
5. 单元测试：
   - SNR往返（n_points一致，每点值一致）
   - 未知必需子块 → 拒绝

**验收**：SNR往返成功，未知必需块被拒绝

---

### WP-G: Transform正式路径（步骤12）

**依赖**：WP-E（流式Writer完成）
**文件**：
- 新增：`lib/astro_image_io/src/hiss_transform.h/.cpp`
- 修改：`lib/astro_image_io/src/hiss_writer.cpp`（调用transform）
- 修改：`lib/astro_image_io/src/hiss_reader.cpp`（逆向transform）

**任务**：
1. 实现 byte-shuffle（forward + inverse）
2. 实现 delta（forward + inverse）
3. 实现 delta+varint（forward + inverse）
4. Writer在压缩前执行transform
5. Reader在解压后执行逆向transform
6. 单元测试：
   - byte-shuffle往返
   - delta往返
   - delta+varint往返
   - ZSTD+shuffle往返

**验收**：transform在正式路径执行，往返成功

---

### WP-H: CLI+Browser接入（步骤14）

**依赖**：WP-E（Writer接入完成）
**文件**：
- 修改：`lib/orchestrator/cpp/src/cli_command.cpp`（stage1参数诊断）
- 修改：`lib/healpix_db/healpix_browser_qt/`（按Tile查看）

**任务**：
1. CLI stage1参数和诊断输出
2. Browser按Tile查看signal/support/SNR
3. Browser首次打开不加载整文件
4. Browser数值查询与Reader一致
5. 集成测试：
   - `orchestrator stage1 <input.fits> -o output.hiss` 生成HISS
   - Browser打开HISS按Tile查看

**验收**：CLI端到端生成HISS，Browser按Tile查看

---

### WP-I: 集成测试+实验+性能报告（步骤15-17）

**依赖**：所有WP完成
**文件**：
- 重写：`lib/astro_image_io/tests/hiss_correctness_test.cpp`
- 新增：`lib/astro_image_io/tests/test_drizzle_integration.cpp`
- 修改：`lib/astro_image_io/tests/hiss_benchmark.cpp`（真实数据实验）

**任务**：
1. 重写所有测试，契约不满足必须真正失败（步骤15）
2. 测试框架维护正确通过/失败计数
3. 使用testdata/真实数据做C++实验（步骤16）
   - Galaxy_Center/LDN43/NGC1727等
   - 从真实Stage1导出Tile
   - DQ-001~DQ-007重新实验
   - 磁盘随机读取P50/P95/P99
4. 详细性能报告（步骤17）
   - 真实CPU利用率
   - 峰值RSS增量
   - Stage1各阶段profile

**验收**：所有测试通过（无软通过），实验用真实数据，性能报告非估算

## 并行执行顺序

```
第一批（并行）：WP-A + WP-B + WP-C
    ↓
第二批（并行）：WP-D + WP-E + WP-F（依赖WP-A）
    ↓
第三批（并行）：WP-G + WP-H（依赖WP-E）
    ↓
第四批：WP-I（依赖所有）
```

## 子代理指令模板

每个子代理收到任务时：
1. 阅读 `docs/stage1_fix/00_COMMON_CONTRACTS.md`
2. 阅读本工作包定义
3. 实现修复
4. 用合成数据写单元测试
5. 运行测试确认通过
6. 报告完成状态和测试结果
7. 不得修改其他工作包的文件
