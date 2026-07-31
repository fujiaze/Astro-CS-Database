# Stage1/HISS 修复验收清单

## 批次A：HISS数据模型修正

### 步骤1：Tile父子模型
- [ ] `n_leaf_per_tile = (NSIDE/tile_nside)² = 4^d`（不是`tile_nside²×12`）
- [ ] `d = min(9, log2(NSIDE/16))`
- [ ] `NSIDE_tile = NSIDE/2^d >= 16`
- [ ] 满Tile最多`4^9=262144`个叶像素
- [ ] NSIDE=64, tile_nside=16 → n_leaf=16
- [ ] NSIDE=32768, tile_nside=64 → n_leaf=262144
- [ ] local_to_global/global_to_local映射正确
- [ ] 单元测试通过（合成数据）

### 步骤2：sum_area语义统一
- [ ] Drizzle累加器：`sumArea = Σ a_jp`（球面重叠面积）
- [ ] HISS输出：`support = sumArea / A_p`
- [ ] Drizzle和HISS对sum_area语义一致
- [ ] 单元测试通过

## 批次B：Drizzle科学语义修正

### 步骤3：球面HEALPix重叠
- [ ] 源像素通过WCS/SIP映射到球面顶点
- [ ] 球面drop多边形与HEALPix像素球面边界重叠
- [ ] 球面excess公式计算面积
- [ ] float64内部精度
- [ ] 通量守恒：drop未截断时Σsignal=L_j（误差<1e-5）
- [ ] 球面面积误差<1e-6
- [ ] 极区不产生异常
- [ ] 大视场（>3°）不产生异常
- [ ] 单元测试通过

### 步骤4：候选像素查询
- [ ] 不限于1-ring（候选数可>48）
- [ ] 根据源像素球面多边形覆盖范围查询
- [ ] 高NSIDE+大源像素测试通过
- [ ] 单元测试通过

### 步骤5：自动NSIDE
- [ ] 上限`2^22=4194304`
- [ ] 至少9×9网格采样（不是5点）
- [ ] 钳位[16, 4194304]
- [ ] 0.1"输入支持
- [ ] 单元测试通过

### 步骤6：pixfrac和RING校验
- [ ] `pixfrac<=0`返回错误（不进入快速路径）
- [ ] `pixfrac>1`返回错误
- [ ] `nested=false`返回错误
- [ ] 单元测试通过

## 批次C：Stage1集成

### 步骤7：signal保存累计通量
- [ ] `signal[p] = float(sumFlux)`（不除面积）
- [ ] TEST 06/07重写
- [ ] 通量守恒测试通过

### 步骤8：HissWriter接入DrizzleEngine
- [ ] `writeHis()`调用新`HissWriter`
- [ ] 元数据无完整WCS（cd/crval/crpix/sip）
- [ ] 旧`aio_hiss_write/read`改造成新后端
- [ ] 集成测试通过

### 步骤9：Gaia测光比例应用
- [ ] `I_photo = k_photo * I_cal`在Drizzle前应用
- [ ] `PHOTSCAL`记录比例
- [ ] `PHOTAPPL=TRUE`
- [ ] `apply_photometry=false`但`BUNIT=ASTROCS_RELATIVE_FLUX`时Writer拒绝
- [ ] 单元测试通过

### 步骤10：流式写入
- [ ] Tile压缩后立即写入临时池
- [ ] 内存不保留compressed_data
- [ ] 1000 Tile内存峰值测试通过

### 步骤11：BITMAP/SPARSE有效数据
- [ ] FULL：n_leaf_per_tile个值
- [ ] BITMAP：n_valid个值+occupancy位图
- [ ] SPARSE_LIST：n_valid个值+索引列表
- [ ] 稀疏数据体积<FULL体积
- [ ] occupancy模式Writer自动选择（不由调用方传入）
- [ ] 单元测试通过

## 批次D：容器修复与集成

### 步骤12：transform正式路径
- [ ] Writer执行byte-shuffle/delta/varint
- [ ] Reader执行逆向变换
- [ ] 实验复用正式路径
- [ ] 往返测试通过

### 步骤13：SNR布局和未知块拒绝
- [ ] SNR布局：`[n_points:u32][points:n*8B]`
- [ ] 每点：`local_ipix(u32)+snr(f32)`
- [ ] 不含snr_phot/median_snr/idw_power
- [ ] Reader拒绝未知必需子块
- [ ] 往返测试通过

### 步骤14：CLI和Browser接入
- [ ] CLI stage1参数和诊断
- [ ] Browser按Tile查看signal/support/SNR
- [ ] Browser首次打开不加载整文件
- [ ] Browser数值查询与Reader一致
- [ ] 集成测试通过

## 批次E：测试与实验重做

### 步骤15：重写测试
- [ ] 契约不满足必须真正失败
- [ ] 禁止`ASSERT_TRUE(true)`软通过
- [ ] 测试框架维护正确通过/失败计数
- [ ] 覆盖所有17步修复项
- [ ] 全部测试通过（无软通过）

### 步骤16：真实数据C++实验
- [ ] 使用testdata/真实数据
- [ ] 从真实Stage1导出Tile
- [ ] DQ-001~DQ-007重新实验
- [ ] 磁盘随机读取P50/P95/P99
- [ ] 实验复用正式Writer/Reader路径

### 步骤17：详细性能报告
- [ ] 真实CPU利用率
- [ ] 峰值RSS增量
- [ ] Stage1各阶段profile
- [ ] 不是估算值

## 最终自审

- [ ] 直接修改旧仓库，没有新建替代仓库
- [ ] 没有实现或修改Stage2
- [ ] 没有自动运行710全量回归
- [ ] 没有用Python原型冒充正式C++实现
- [ ] Wiki已冻结规范全部进入
- [ ] 冲突旧页面已标注SUPERSEDED
- [ ] Wiki说明未决项只做实验，不自动定案
- [ ] 三种校准模式公式正确
- [ ] 最优Dark失败后有明确诊断并回退曝光比例
- [ ] Flat只做允许格式和结构检查
- [ ] PlateSolve星点只检测一次并复用
- [ ] Gaia比例已在Drizzle前应用
- [ ] 自动NSIDE和NESTED正确
- [ ] pixfrac和球面面积重叠正确
- [ ] signal/support内部float64，最终float32/uint8
- [ ] 自适应Tile规则正确
- [ ] FULL/BITMAP/SPARSE均可往返
- [ ] 独立子块可单独读取
- [ ] 每子块有codec/transform/size/checksum字段
- [ ] RAW可用
- [ ] 未知可选/必需块兼容规则正确
- [ ] Header前置、attachments后置，无Footer/Checkpoint
- [ ] 不保存完整WCS/SIP
- [ ] 元数据保持精简FITS风格
- [ ] 所有压缩与阈值实验由C++完成
- [ ] 保留CSV/JSON原始结果
- [ ] 报告运行环境、重复次数和波动
- [ ] 只给推荐，没有把未决项写成冻结默认值
- [ ] ZIP只包含必要差异文件和报告
- [ ] 有git patch、删除列表、应用说明和manifest
- [ ] 没有仓库副本、构建产物、大数据和大型日志
