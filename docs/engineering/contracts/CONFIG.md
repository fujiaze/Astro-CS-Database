# 配置合同

配置把科学意图、输入来源和输出位置交给三个独立命令。本文依据最高设计[1]规定配置的职责和解释规则；科学公式由科学正本定义，字段结构由对应 schema 表达。解析器、模板和生产节点必须共同实现这份合同，任一实现文件都不单独获得高于设计的权威。

## 三类配置

| 对象 | 写入者 | 内容 | 边界 |
|---|---|---|---|
| `phase_config` | 用户或 GUI；CLI 提供模板 | 输入、输出目录、科学参数、算法选择 | 不用 CPU 型号或线程数改变科学含义 |
| `cpu_profile` | `benchmark` | 机器身份、可用 ISA、内核与资源调优结果 | 不替用户选择科学模型、通带或参考源 |
| `run_manifest` | 运行时 | 实际配置、软件与输入输出身份、阶段状态和诊断 | 记录本次事实，不是供用户编辑的阶段配置 |

机器画像自动读取；失配必须报告，不能伪称已使用相应基准结果。发行目录不可写时使用明确报告的用户可写位置。运行清单应冻结实际生效值，不能只保存模板值或未经消费的输入字段。输出位置与事件流由命令行协议规定[2]，产品来源链由数据语义规定[3]。

## 三命令 phase_config 与模板

三个命令使用相同的分块结构。一块是一个命令的一次运行，包含输出名称、运行参数和一组输入；不同块使用各自的非空 `output_dir`，分别写清单。顶层 `blocks[]` 与平铺单块简写互斥，不能混用后静默取一。

```json
{
  "schema_version": "1",
  "blocks": [
    {
      "name": "example",
      "input_lights": ["path/to/light.fits"],
      "output_dir": "path/to/normalize-output",
      "drizzle": {"nested": 1, "pixfrac": 0.8, "precision_mode": 1}
    }
  ]
}
```

这个片段只说明 Normalize 块的结构；真实运行还需按输入状态提供校准、天测、测光等参数。模板里的占位路径不表示输入已具备，示例数字不自动成为科学默认值。平铺单块写法把同一块的字段移到顶层并保留 `schema_version`。

| 命令 | 输入字段 | 主要运行参数 | 输出 |
|---|---|---|---|
| `normalize` | 非空 `input_lights[]`；块级 `master_bias/master_dark/master_flat` 按校准合同解释 | `wcs`、`star_detection`、`photometry`、`noise`、`snr`、`drizzle`、响应信息 | 标准化单帧 HiPS 与相应科学描述 |
| `mosaic` | 非空 `hips_paths[]`，每项是一个产品路径 | `upm`、`reject`、`algorithm_rejection_method`、SNR 消费路径 | 马赛克 HiPS |
| `export` | `source: {"hips_dir": "..."}`，一个产品 | `center`、`width_px`、`height_px`、`scale_deg_per_px`、投影与采样、`output_mode`、可选 `crop` | 平面 WCS FITS |

完整字段结构由 `eng/contracts/schemas/phase_config_normalize.schema.json`、`phase_config_mosaic.schema.json`、`phase_config_export.schema.json` 表达。生产可达性还须检查 CLI 解析及节点消费：schema 接受某键，并不证明该键改变了运行结果。CLI 的帮助与动态模板共用 `lib/infrastructure/cli/session_commands.h` 的 `config_fields`；结构检查在 `parser.cpp`，科学值域与依赖检查在所属模块。

当前 CLI 对含 `phase_name` 或数组形态 `inputs[]` 的配置明确拒绝并给迁移提示。schema 中仍保留这种结构分支，不能据此把它写成受支持的 CLI 用法。应使用 `blocks[]` 或平铺单块形态。

## 字段的物理意义

### 校准、响应与测光

校准母版必须具有可解释的曝光、增益、温度与处理状态。曝光差的提示阈值只是检查策略，不能代替暗电流、偏置残留与噪声传播的科学判据。参数来源和适用条件由校准正本规定[4]。

响应曲线的名字用于选择输入数据。已知响应应记录滤镜、探测器 QE、波长单位、归一方式和来源；未知或缺失响应必须保留未知状态，不能补成理想响应。不同设备和通带的合法输入不因名字不同而被拒绝，但响应未知或不同会限制绝对定标和合成结果的解释[1][3][5]。

当前曲线扫描器从可执行文件目录下的 `config/response_curves/` 非递归读取 JSON 文件。每文件一条曲线，含 `name`、`channel`、`wavelength_nm`、`value`、`n_points` 和 `provenance`。名字按字节精确比较，不做大小写、空白或别名转换；重名、结构损坏和所请求曲线缺失须报告具体错误。QE 以 `channel="Q"` 区分类别。这个实现约束描述曲线装载，不能扩大为所有产品必须来自同一物理通带的准入规则。

扫描器检查样本数及 `provenance.curve_stats` 中的范围、和与平方和。这些量可发现部分数据错配，但不是无碰撞的内容身份；完整内容身份应另外记录字节或规范内容摘要。即使原始文件摘要一致，也只证明引用了同一份数据，不证明曲线匹配当前设备。

`None` 是单位透过率的定义性通带，`Ideal QE curve` 是定义性的理想探测器响应；二者不证明真实设备理想。空字符串表示未给定曲线选择，不能在合成积分中暗中等同 `None`。生产测光要求的输入不足时应明确报告不可计算的定标量，并按可用科学信息继续解释信号。

曲线出处使用 `source`、`version`、`acquired`、`original_file_sha256`、`checksum_status` 和相应状态描述。仓内中间文件摘要不能冒充原始文件摘要；按定义构造的条目用 `defined_by_construction` 与 `exact_by_definition` 表达。`source_recorded` 只表示来源已记载，`verified` 应限于声明的来源核对范围。滤镜与 QE 分别记录证据，不互借摘要。

测光残差、颜色趋势和独立标准源可用于检验响应一致性；残差小不足以唯一辨认通带，因为参考星光谱集合、噪声和拟合自由度可能使不同响应不可区分。标度因子应来自同一合成通量定义，不能由一个滤镜名字或预置零点推定[5]。

### 参考源与不确定度

`snr.reference` 的参考通量、位置、SED 或响应选择必须组成可复用的物理身份。`frame_overrides` 按输入出现位置绑定逐帧信息，不能将第一帧的响应传播给所有帧。详细字段设计和可用性由 Normalize 合同说明[6]。

上述按 occurrence 绑定是目标合同。当前 CLI 实际读取 `photometry.frame_overrides[]`，按输入路径、basename 或 frame_key 找到首个匹配项后使用；它还不能严格区分同一路径重复出现的输入。该实现语法与目标块级逐帧绑定分开登记，不能把目标字段直接写进配置并假定生产节点已经消费。实际支持字段应与解析器、帮助及数据合同同步核对[14]。

缺参考源时允许精度对象为 `unavailable`，不能把 `reference_flux_adu`、源通量中位数或任意常数自动升级成物理参考源。参考源测量方差、背景方差和实际场景方差是不同量；权重必须由所声明估计器的方差推导[3][7]。`I_F` 不能直接充当背景逆方差，`T=1` 也不能作为缺信息的补值。

噪声参数必须标明它作用于哪一种像素单位。若图像乘正标度 `alpha`，方差和电平阈值分别乘 `alpha²` 和 `alpha`；像素长度、计数和无量纲比值不随这种强度标度变化。控制点间距转换为角密度时必须使用对应位置的实际 WCS 几何，不能用球面目标网格尺度代替输入探测器尺度。

### 精度、权重与排异

大科学数组的 FP32/FP64 由数据精度选择决定，元数据、稀疏模型和关键归约可具有不同精度要求。当前 Normalize 节点要求显式 `drizzle.precision_mode`（0 为 FP32，1 为 FP64）；Export 使用 `bitpix=-32|-64`。不得新造同义 `precision` 字段并假定三条命令均消费它。允许的精度与数值误差预算由数值标准规定[8]。

权重是科学估计器的派生量，不提供让用户任意替换其物理意义的集成权重模式。配置的排异方法决定接受集合；实际使用的方法、有效样本数及不可辨识状态必须进入诊断。自动路由的样本数口径、方法值域和阈值由排异正本与算法说明共同承载[9]，配置文档不另立阈值表。

### 导出几何与裁剪

`source.hips_dir` 是产品路径，不假设产品来自 Mosaic。`output_mode` 必须明确测量意义；缺少相应校准或单位信息时，输出必须明确限制或不可用，不能只改变标签。当前生产链的投影、响应、精度和单信号层限制见 Export 细节[10]。

`crop` 可省略，省略表示不再对既定输出画幅裁剪，不表示该画幅已经覆盖全部输入。支持两个互斥结构：

```json
{"crop_form":"pixels","pixels":{"x0":1001,"y0":2001,"x1":1512,"y1":2512}}
```

```json
{"crop_form":"sky","sky":{"ra_min_deg":83.5,"ra_max_deg":84.0,"dec_min_deg":-5.6,"dec_max_deg":-5.2}}
```

像素范围为 FITS 1-based 闭区间；天空范围为 ICRS 度，赤经下限大于上限表示跨零点。非法值、互斥形式同时给出、越界和空窗口必须具名报告，不能静默夹取。

像素窗口从零基位置 `(xoff,yoff)` 开始时，限制到窗口后的 WCS 保持 `CRVAL` 与 `CD`，`CRPIX` 减去该整数偏移。这保证同一数学天球映射；不同求值顺序下的浮点计算不承诺逐位相同。天空矩形转换还需处理投影奇点与连续边界极值；固定数量边界采样不构成严格包围证明[10]。

## 默认值与登记

`eng/packaging/config/defaults.json` 保存已登记默认值，`config_registry.json` 保存配置归属和来源信息。默认值必须包含单位、适用数组或估计器、来源与有效域；缺证据的值保持待确认，不能因登记存在就视为科学成立。

schema 的 `default`、C++ 结构体初值、解析缺省、动态模板、打包模板必须分别核对。模板值只在明确引用默认来源时具有默认意义，不能将某个实验的参数固化为通用常数。配置登记中的文档引文或指纹随文档变动而需重新核对；登记器本身不能决定文档内容，也不建立第二条锚点权威链。

`module.yaml` 可描述模块端口、构建和合同；字段究竟由科学配置、机器画像还是运行策略拥有，应按语义决定，不能通过固定文件数或表头形式推导。注册表页负责逐模块说明实际消费参数[11]。

## 内部配置与运行清单

`lib/infrastructure/pipeline/orchestrator/configs/` 及 `stage2_common` 定义的内部配置服务特定调用接口。它们的 `stage1.*`、`integration.*`、`model.*` 结构不是用户 `phase_config` 的通用同义写法。适配层必须显式完成转换；只在内部工具生效的配置不能登记为 CLI 已支持。生产调用路径由架构与模块页说明[11][12]，不在此复制调用表或测试编号。

`cpu_profile.schema.json` 同时描述机器画像的兼容分支；当前机器身份包括 CPU、OS、软件和 provider 信息。`host.os_abi` 的交付值域为 `linux` 与 `windows`。兼容分支接受结构不代表实际路由器消费了该分支的全部字段。

`run_manifest.schema.json` 的预留词表含 `manifest_schema`、`software_sha`、`config_hash`、`manifest_input_hashes` 等；`commands.cpp::write_run_manifest` 实际写出 `schema_version`、`kind`、`acsd_version`、`config_sha256`、`phases`、`artifacts`、`status` 等字段。这是尚需统一的生产合同差异，不能声明现有产物已经通过该 schema。具体清单验证职责由清单合同规定[13]。

## 一致性验证与实现差异

配置验证至少覆盖结构正例、未知键、互斥形态、缺输入与输出目录、非法精度、跨类字段混入，以及“合法键确实到达生产消费者”的行为检查。科学参数还需在独立实验中检验其有效域。测试名、表格断言和源码可达性不等于一次通过的运行记录；没有可复现入口与输出时只能登记验证要求。

当前需集中跟踪的差异包括：

- 打包 Normalize 模板的 `pixfrac=0.8` 与 CLI 动态模板的 `pixfrac=1.0` 不一致；Mosaic/Export 的打包模板为分块形态，动态模板为平铺形态，应验证两者等价性而非声称字节一致。
- schema 保留的 `phase_name/config/inputs[]` 分支被 CLI 拒绝；部分字段只获解析接受，生产消费未完整落实。
- `snr.reference`、逐输入响应和科学对象合同仍需与严格 schema、帮助和运行节点逐项对齐；参考与响应不足不得伪造可用精度。
- 运行清单词表、固定输出单位、通用投影与响应传播尚需统一；裁剪的数学映射与连续几何边界应分别验证。

差异的集中处置见实现差异表与待解决事项[14][15]。本文订正语义和证据等级，不代替代码修复或发布验收。

## 参考文献

[1] [ACSD 最高设计](../../ACSD_DESIGN.md)。

[2] [命令行协议](CLI_PROTOCOL.md)。

[3] [数据语义](../../science/unified/DATA_SEMANTICS.md)。

[4] [图像校准](../../science/calibration/CALIBRATION.md)。

[5] [测光科学](../../science/photometry/PHOTOMETRY.md)。

[6] [Normalize 接口与配置](../../detail/normalize/contracts.md)。

[7] [噪声与信噪比](../../science/noise_snr/NOISE_SNR.md)。

[8] [数值标准](../standards/NUMERIC.md)。

[9] [排异科学](../../science/integration/REJECTION.md)。

[10] [Export 管线](../../detail/export/pipeline.md)。

[11] [模块注册表](../../detail/registry/README.md)。

[12] [Normalize 生产链](../architecture/NORMALIZE_PIPELINE.md)。

[13] [运行清单验证](MANIFEST_VERIFY.md)。

[14] [科学实现差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

[15] [待解决事项](../governance/UNRESOLVED.md)。
