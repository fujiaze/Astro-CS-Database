# Stage1/HISS 修复规范

> 依据：用户审查反馈（2026-07-31）+ 02_FROZEN_STAGE1_HISS_SPEC.md + 09_DELIVERY_REVIEW_CHECKLIST.md
> 性质：工程重构 + 功能实现，必出三件套

## 1. 背景

上一轮交付（commit 78a9121）被用户判定为"不能验收，不建议直接应用"。存在多项阻断性科学和集成错误。本规范定义17步修复的详细要求。

## 2. 修复范围

17步优先修复顺序（用户指定，不得调整顺序）：

### 批次A：HISS数据模型修正（步骤1-2）
1. 修正Tile父子模型：单Tile叶像素数 = (NSIDE/tile_nside)² = 4^d
2. 统一sum_flux与sum_area/A_p语义

### 批次B：Drizzle科学语义修正（步骤3-6）
3. 实现真实球面HEALPix重叠
4. 修正候选像素查询，去掉固定1-ring限制
5. 修正自动NSIDE到至少2^22，并扩大/自适应采样
6. 拒绝pixfrac<=0和RING HISS

### 批次C：Stage1集成（步骤7-11）
7. signal直接保存累计通量，重写错误测试
8. 将新HissWriter真正接入DrizzleEngine::writeHis()
9. 真正应用Gaia测光比例
10. 将压缩子块流式写入临时池，不在内存保存全部Tile
11. 让BITMAP/SPARSE只保存有效signal/support

### 批次D：容器修复与集成（步骤12-14）
12. 实现transform正式路径
13. 修复SNR布局和未知必需子块拒绝
14. 接入CLI和Browser

### 批次E：测试与实验重做（步骤15-17）
15. 重写测试，任何契约不满足必须真正失败
16. 使用真实或从真实Stage1导出的Tile重新做C++实验
17. 最后再提交详细性能报告

## 3. 详细修复规范

### 步骤1：Tile父子模型修正

**当前错误**：使用`tile_nside² × 12`作为单Tile叶像素数（这是全天像素数）

**修正为**：
- `d = min(9, log2(NSIDE/16))`
- `NSIDE_tile = NSIDE / 2^d`
- `n_leaf_per_tile = (NSIDE/NSIDE_tile)² = 4^d`
- 满Tile最多`4^9=262144`个叶像素

**影响范围**：
- Tile数组长度
- local_ipix范围
- occupancy位图大小
- signal/support数组映射
- query_pixel
- 文件体积估算

**新增文件**：`lib/astro_image_io/src/hiss_tile_model.h/.cpp`

**验收**：NSIDE=64, tile_nside=16 → n_leaf=16（不是3072）

### 步骤2：统一sum_area语义

**当前错误**：Drizzle中sumArea=切平面裁剪面积，HISS Writer中sum_area=已归一化覆盖率

**修正为**：
- Drizzle累加器：`sumArea = Σ a_jp`（球面重叠面积，球面度）
- HISS输出：`support = sumArea / A_p`（A_p=目标HEALPix像素面积）
- 两边统一语义：sumArea都是球面重叠面积

### 步骤3：球面HEALPix重叠

**当前错误**：切平面近似 + 人工HEALPix菱形 + Sutherland-Hodgman平面裁剪

**修正为**：
- 源像素通过WCS/SIP映射到球面顶点
- 计算球面drop多边形与目标HEALPix像素球面边界的重叠
- 使用球面excess公式计算面积
- float64内部精度

**新增文件**：`lib/healpix_db/healpix_drizzle/spherical_overlap.h/.cpp`

**验收**：
- 通量守恒：drop未截断时，Σsignal = L_j（误差<1e-5）
- 球面面积误差 < 1e-6（已知球面多边形）
- 极区不产生异常

### 步骤4：候选像素查询

**当前错误**：固定中心+四角各1-ring，候选数≤48

**修正为**：
- 根据源像素球面多边形覆盖范围查询所有可能相交的HEALPix像素
- 不限于1-ring
- 高NSIDE+大源像素时也能覆盖

### 步骤5：自动NSIDE

**当前错误**：上限2^20，仅5点采样

**修正为**：
- 上限提升到`2^22 = 4194304`
- 至少9×9网格采样（SIP畸变极值可能在边缘）
- 钳位 [16, 4194304]

### 步骤6：pixfrac和RING校验

**当前错误**：pixfrac<=0进入"点采样快速路径"

**修正为**：
- `pixfrac <= 0` 或 `pixfrac > 1`：返回错误，拒绝执行
- `nested = false`（RING）：返回错误，拒绝执行
- 不提供任何"快速路径"替代

### 步骤7：signal保存累计通量

**当前错误**：`signal = sum_flux / sum_area`（平均面亮度）

**修正为**：
- `signal[p] = float(sumFlux)`（直接保存累计通量）
- 重写TEST 06/07（当前按错误公式设计）

### 步骤8：HissWriter接入DrizzleEngine

**当前错误**：writeHis()仍调用旧`hiss_write()`，新HissWriter孤立

**修正为**：
- `DrizzleEngine::writeHis()`改为调用新`HissWriter`
- 移除元数据中的完整WCS（cd/crval/crpix/sip_order/sip系数）
- 旧`aio_hiss_write/read`接口改造成新Writer/Reader后端

### 步骤9：Gaia测光比例应用

**当前错误**：`apply_photometry`和`photscal`声明但未使用

**修正为**：
- Drizzle前应用：`I_photo = k_photo * I_cal`
- `PHOTSCAL`记录实际应用比例
- `PHOTAPPL=TRUE`
- 若`apply_photometry=false`但`BUNIT=ASTROCS_RELATIVE_FLUX`，Writer拒绝

### 步骤10：流式写入

**当前错误**：所有Tile压缩数据保留到finalize()

**修正为**：
- 每个Tile压缩后立即写入临时子块池
- 内存只保留SubblockDescriptor（不保留compressed_data）
- 释放Tile内存

### 步骤11：BITMAP/SPARSE有效数据

**当前错误**：无论什么模式都保存完整长度signal/support

**修正为**：
- FULL：保存n_leaf_per_tile个值
- BITMAP：保存n_valid个值 + occupancy位图
- SPARSE_LIST：保存n_valid个值 + 索引列表
- 稀疏数据体积 < FULL体积

### 步骤12：transform正式路径

**当前错误**：transform_id只有声明，Writer不执行变换

**修正为**：
- Writer执行byte-shuffle/delta/varint
- Reader遇到非NONE transform执行逆向
- 实验必须复用正式路径（不是benchmark单独实现）

### 步骤13：SNR布局和未知块拒绝

**当前错误**：Writer/Reader SNR布局不一致；未知必需块不拒绝

**修正为**：
- SNR子块布局：`[n_points: uint32][points: n_points*8B]`
- 每点：`local_ipix(uint32) + snr(float32)`（8字节）
- 不包含snr_phot/median_snr/idw_power
- Reader遇到未知必需子块返回`HISS_ERR_UNKNOWN_REQUIRED`

### 步骤14：CLI和Browser接入

**当前错误**：新HISS未接入CLI和Browser

**修正为**：
- CLI stage1参数和诊断
- Browser按Tile查看signal/support/SNR
- Browser首次打开不加载整文件
- Browser数值查询与Reader一致

### 步骤15：重写测试

**要求**：
- 契约不满足必须真正失败
- 禁止`ASSERT_TRUE(true, "已知问题")`
- 测试框架维护正确通过/失败计数
- 覆盖：Tile模型、球面重叠、通量守恒、support语义、pixfrac校验、NSIDE、BITMAP/SPARSE、transform、SNR、未知块、checksum、流式写入、WCS移除、DrizzleEngine接入、Gaia测光、CLI

### 步骤16：真实数据C++实验

**要求**：
- 使用testdata/真实数据（Galaxy_Center/LDN43/NGC1727等）
- 从真实Stage1导出Tile做实验
- DQ-001~DQ-007重新实验
- 包含磁盘随机读取P50/P95/P99

### 步骤17：详细性能报告

**要求**：
- 真实CPU利用率
- 峰值RSS增量
- Stage1各阶段profile
- 不是估算值

## 4. 执行策略

### 4.1 子代理并行策略

每个模块分配一个子代理，用合成数据进行单元测试。完成后再由子代理进行集成测试。

### 4.2 并行工作包

**第一批（并行，无依赖）**：
- WP-A: Tile模型（步骤1-2）
- WP-B: NSIDE+pixfrac校验（步骤5-6）
- WP-C: Gaia测光应用（步骤9）

**第二批（依赖WP-A）**：
- WP-D: 球面重叠+候选查询（步骤3-4）
- WP-E: Writer核心改造（步骤7,8,10,11）
- WP-F: SNR+未知块修复（步骤13）

**第三批（依赖WP-E）**：
- WP-G: Transform正式路径（步骤12）
- WP-H: CLI/Browser接入（步骤14）

**第四批（依赖所有）**：
- WP-I: 测试+实验+性能报告（步骤15-17）

### 4.3 依赖约束

- WP-A是基础数据契约，WP-D/WP-E必须等WP-A完成
- WP-E的Writer改造依赖WP-A的Tile模型和WP-C的测光应用
- WP-G的transform依赖WP-E的流式Writer
- WP-H的CLI/Browser依赖WP-E的Writer接入
- WP-I的集成测试依赖所有修复完成

## 5. 验收标准

见 `checklist.md`

## 6. 禁止事项

- 禁止新建仓库
- 禁止修改Stage2代码
- 禁止自动运行710帧回归
- 禁止用Python代替C++实验
- 禁止冻结实验结论为默认值
- 禁止交付完整仓库或大型ZIP
- 禁止在一个文件堆叠上千行代码（新增代码按职责拆分）
