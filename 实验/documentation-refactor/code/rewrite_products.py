"""Archive reviewed product contracts and install the curated explanations."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
    'docs/engineering/data/ARTIFACTS.md': r'''# 数据工件

工件把科学对象绑定到单位、坐标、形状、有效域、所有权与序列化。物理定义以[数据语义](../../science/unified/DATA_SEMANTICS.md)为准，对象职责见[统一工程对象](../UNIFIED_OBJECTS.md)，阶段一复合字段见[Normalize 数据](../contracts/NORMALIZE_DATA.md)。本页登记承载关系，不从旧 schema 或枚举推导新的物理模型。

## 1. 每份工件携带什么

| 项 | 要求 |
|---|---|
| 身份 | run/frame/input occurrence、对象类型、revision、生产者及输入版本 |
| 数组 | dtype、shape、axis 顺序、stride/布局、所有权与借用寿命 |
| 物理 | 被估计量、实际单位、标度是否施加、应用身份、必要 reference/响应 |
| 几何 | 坐标系、像元原点、WCS 或 NESTED order/ipix、tile-local 映射 |
| 有效性 | 覆盖、掩膜、排异、模型能力与缺项分别表达 |
| 持久化 | 格式/schema、相对路径、字节数、内容哈希、完成状态与模型引用 |

没有来源的单位和误差不能补成 ADU、0或1。FITS 读取施加 BSCALE/BZERO 后保存物理标度；校准值、测光应用后值、电子数和 SI 量分别具名。内部零基像元中心与 FITS WCS 一基约定只转换一次，tile-local 与 NESTED 的手性由共享实现处理。

## 2. 既有 DATA ID 与承载

下表保留既有 ID 的定位用途。相同 ID 在旧登记中有歧义时，以科学对象和具体 API 类型解释并登记迁移，不能称全部已明确。

| ID | 对象与承载 |
|---|---|
| `DATA-IMG-RAW-001` | 原始亮场，[H,W] y,x；FITS/XISF 读取后的线性值 |
| `DATA-IMG-CAL-001` | 校准图，[H,W]；保留有限负值及真实单位 |
| `DATA-IMG-VAR-001` / `DATA-IMG-IVAR-001` | 与当前平面、标度和被估计量配对的方差/逆方差 |
| `DATA-IMG-WEIGHT-001` | 旧表用于[0,1]质量权重，旧积分映射又用于逆方差；两者需按实际类型区分 |
| `DATA-IMG-SUPPORT-001` / `DATA-IMG-MASK-001` | 支持与坏像元标记；状态和几何不作统计精度 |
| `DATA-WCS-001` | WCS 描述与来源，角度单位 deg；无解显式登记 |
| `DATA-CAT-PSF-001` / `DATA-CAT-PHOT-001` | PSF/测光源表，含坐标、真实通量单位与测量状态 |
| `DATA-HIPS-SIGNAL-001` | signal HiPS，NESTED、实际线性单位/sr |
| `DATA-HIPS-VAR-001` / `DATA-HIPS-IVAR-001` | 输入帧可有来源的辅助精度；单位分别为 signal 单位平方及其倒数 |
| `DATA-UPM-MODEL-001` | 与所拟合信号同单位的加性背景模型，不能一律写探测器 ADU |
| `DATA-UPM-CONTROL-UNC-001` | UPM control cell 统计量自身的方差/逆方差，含其相关性模型与支持 |
| `DATA-REJ-MAP-001` | 具名拒绝原因或资格载体 |
| `DATA-FRAME-ID-001` | 帧身份登记；内容身份与输入 occurrence 不能混用 |
| `DATA-P3-FITS-001` | 二维 signal WCS FITS；内存行主序 [H,W]，FITS NAXIS1=W/NAXIS2=H |
| `DATA-P1-PHOTPROV-001` | p1_phot.json 逐帧应用状态、标度与来源；组级摘要不能覆盖失败帧 |
| `DATA-GAIA-001` | Gaia 行及光谱数组，实际坐标历元、光谱采样、单位与内存释放由 Gaia API 说明 |
| `DATA-COV-001` | coverage 联合 MOC 单元及输入引用，调用方数组与两阶段容量协议 |
| `DATA-HIPS-001` / `DATA-TILE-001` | HiPS properties 和 float FITS tile 的输入角色；几何/单位来自实际产品 |
| `DATA-UNC-001` | 既有不确定度容器登记；名称存在不表示完整生产噪声模型 |
| `DATA-P2-VAR-001` / `DATA-P3-UNC-001` | 旧辅助图像/扩展登记；不属于当前最终 Mosaic/Export 科学图像发布集合 |
| `DATA-P2-REJ-001` / `DATA-P3-REJ-001` | 内部样本接受位、拒绝计数及诊断；具体载体以真实调用为准 |
| `DATA-P2-PROV-001` | Mosaic 来源/能力元数据；不能凭键存在推断不确定度已验证 |

旧完整字段表、接口分支、常量与未接线测试名称保存在[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)。历史 ID 保留可定位性；当前阶段产品政策由[交换合同](PHASE_PRODUCT_EXCHANGE.md)唯一规定。

## 3. 统一对象与机器 schema

`eng/contracts/schemas/unified/` 的13类对象为 signal、variance、ivar、source_snr、depth_m5、frame_snr、point_information、sparse_snr_layer、support、coverage、validity、rejection、provenance。其职责、精度与当前载体见统一工程对象，机器 schema 编码形状与兼容值域，不拥有物理定义。

Normalize 只发布一份 canonical frame_science.json；Catalogue、帧摘要和规则像素 SNR 网格是派生/兼容视图。available/unavailable 形状夹具中的假身份和哈希不能作为可消费科学产品。reader 要验证真实引用、模型单位/域与跨字段关系。

新 typed/JSON 用显式 status、optional/null 表示未知精度。既有 variance=0/ivar=0 的 unavailable 哨兵只在 legacy adapter 解释，不能拿0当真实零方差，也不能求倒数得到 Inf。无覆盖、未知噪声、损坏输入与排异分别表示。

## 4. 易混字段的消费者解释

积分 reducer 消费调用方传入的 value/weight/support/accepted，不负责选择科学权重。点源 `SNR²/F_ref²` 是通量信息量，变成面亮度权重还需要同条件 T；不能把它直接写成 signal 图像的通用精度。质量因子、几何支持与 UPM 控制权也具有不同单位。

UPM `raw_w=quality_factor×control_ivar` 和每 control 的归一化是当前实现步骤，科学统计意义回到[背景模型](../../science/sky/UPM.md)。`k_corr`、MAD一致化系数、帧 SNR、PSF宽度和角像元尺度分别引用专题，不能由一个泛称 sigma/weight 的列承载。

## 5. 所有权与发布

大数组由 Runtime/调用方按明确合同持有，节点借用，异步任务在回收前完成或转移所有权。持久化对象通过[产物库](ARTIFACT_STORE.md)和[原子发布](../contracts/ATOMIC_PUBLISH.md)管理，跨阶段仅消费完成磁盘产品。保留过程失败与来源，真实观测 FITS 不入 Git；构建与发行材料留存见[发行合同](../build/RELEASE.md)。
''',
    'docs/engineering/data/ARTIFACT_STORE.md': r'''# 产物库

产物库负责工件寻址、暂存、发布、校验读与恢复。科学对象来自[工件合同](ARTIFACTS.md)，HiPS 产品发布见[原子发布](../contracts/ATOMIC_PUBLISH.md)。本页区分目标 Runtime 接线、Python 语义实现与当前 C++ 实际消费者。

## 1. 对象与接口

`lib/infrastructure/aio/runtime/artifact_store/production_store.py` 提供 `ArtifactStore`、`Writer`、`StoreIO`、`ManifestRead` 和 `Digest`：

```text
start → new_writer → stage_bytes → stage_manifest → publish
                                               ↓
                          bind_as_input → read_verified
```

`with_provenance` 注入构建/后端/输入事实；带溯源产品使用 `bind_product_input` 验证。Python 类和 C++ `acsd::core::ArtifactStore` 是不同实现，不能因为名字相同就声称生产 CLI 每次读写都经该 Python Store。

目标接口向模块提供已校验的 reader/writer/handle，真实路径仅在 I/O 层解析；阶段内数值对象还要遵守[Runtime](../contracts/RUNTIME.md)与[所有权](../contracts/OWNERSHIP_LIFETIME.md)。当前各生产节点仍有工作 JSON/FITS 载体，接线差异见[实施地图](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 2. Python 实现布局

```text
<root>/runs/<run_id>/stage/
<root>/runs/<run_id>/objects/<artifact_id>
<root>/runs/<run_id>/manifests/<artifact_id>.manifest.json
<root>/runs/<run_id>/manifests/<artifact_id>.manifest.sha256
<root>/runs/<run_id>/manifests/<artifact_id>.provenance.json
```

最后一项在配置溯源时生成。HiPS 目录 `products/<user_path>/` 属另一个发布器的布局，不能与 typed 对象目录混用。artifact_id/type/revision/schema/producer 保持一致；storage_uri 是寻址身份，不由算法模块解释为任意文件路径。

## 3. 写入与读取

Writer 在私有 stage 写入字节并持久化。stage_manifest 检查登记形状、类型、生产者、摘要和大小；publish 重算内容 SHA-256/大小，与声明核对。manifest JSON 的现行序列化是 `ensure_ascii=False`、紧凑分隔、保留 dict 插入顺序；它不是任意键重排后都保持摘要的通用 canonical JSON。

当前 publish 顺序为内容→COMPLETE manifest→manifest 哈希 sidecar→可选 provenance，每个 rename 原子；这一串不是多文件事务。进程在中间停止时可能已有 COMPLETE manifest 而缺后续文件，不能沿用“任一失败必无 COMPLETE 文件”的旧断言。

start 仅把内容、COMPLETE manifest 和 hash sidecar 三者存在的对象纳入基线索引；损坏 provenance 可被忽略而对象仍按基线索引。带溯源消费必须走 bind_product_input。恢复索引不等于全量内容已重算，最终读取仍需 read_verified 检查 type、内容大小与摘要；实际校验范围需沿调用核对。

## 4. 失败、并发与边界

同 ID 唯一生产者；已发布对象不隐式覆盖。取消/释放终止未完成 writer，并清理私有暂存；中途发布残留按真实完成条件处理，不以单个文件名判断成功。多个 run 隔离，同一对象生产由调用方串行化，异步读者不得超过 Store/对象寿命。

StoreIO 的 spy 只证明被测路径经过它，不能证明整个 CLI 没有直接 I/O。Python 实现中 manifest/provenance 的部分打开还直接使用 open；“全部读写均经 StoreIO”需进一步收敛和验证。磁盘写满、中断在每个发布步骤注入，核对残留与恢复；没有实际执行记录就不声明通过。

Windows DLL 与 Linux 运行时是否具有相同发布状态机，必须以各平台生产调用和实验为据，Python 语义层存在不等于它们已接线。旧全文与验收意图保存于[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)。
''',
    'docs/engineering/data/PROVENANCE.md': r'''# 产品溯源

溯源解释产品由哪些输入、配置、算法、构建与执行条件生成。它与内容完整性互相支持，哈希本身不认证来源真实性。运行级字段见[清单合同](../contracts/MANIFEST_VERIFY.md)，typed Store 的旁挂记录由本页定义。

## 1. revision 与身份

| 字段 | 当前 Python 绑定 | 解释边界 |
|---|---|---|
| `revision.product` | producer.module_build_id | 构建/产品版本身份 |
| `revision.module` | producer.module_id | 模块身份；字段名不意味着另有模块版本值 |
| `revision.abi` | 当前由 manifest 文档形态 v1 派生 | 文档形态不能自动证明已核验动态 C ABI |
| `revision.data_schema` | type_id.schema_version → vN | 数据结构修订 |
| `doc_revision` | provenance 文档自身修订 | 与数据 schema、产品版本分开 |
| `history` | replaced 与 superseded_by | 明确的替换链及理由 |

算法模型版本、数据 schema、C ABI、文档版本、构建与输入 occurrence 分别记录。当前 Python 绑定不具备全部维度的独立值时，说明缺口，不据字段存在制造完整版本语义。历史替换关系不能由一个版本大小比较推断。

## 2. 确定性摘要

`lib/infrastructure/aio/runtime/artifact_store/provenance.py` 实现 `acsd.provenance/v1`：

```text
sha256(canonical_json({
  provenance_schema, version, artifact_id,
  revision{product,module,abi,data_schema}, source_commit,
  config_digest, provider_digest, worker_digest,
  input_digests, science_ids
}))
```

输入产物引用和 science_ids 稳定排序。run_id、created_utc、phase、strategy、history、doc_revision 是旁挂事实，不参与现行摘要。因此“同输入配置”必须指上述全部字段相同，不能只指原始帧相同。strategy 目前被排除尤其不能用来证明采用了同一算法。

source_commit 由调用方明确提供，Store 不自行猜 Git；构建源摘要与实际二进制身份见[发行合同](../build/RELEASE.md)。provider/worker 摘要需要对应真实观测或明确声明，不能填占位串。重算成功证明所列字段一致，不证明所列事实真实或实际运行使用了它们。

## 3. Store 接线与校验

`ArtifactStore.with_provenance` 配置后，publish 构造 sidecar，先核对 data_schema 对应、历史替换、隐私，再按实际顺序写入。文档保持 artifact manifest 的严格形状，provenance 单独放 `<artifact_id>.provenance.json`。

publish 的多个文件 rename 并非单一事务；恢复 start 可在缺损 provenance 时保留基线索引。需要溯源的产品必须用 `bind_product_input`，重算 provenance_digest 并核对数据 schema 与版本条件。完整性、来源、可消费性分别判断，不能因对象已索引就认为全部通过。

`assert_not_superseded` 检查指定类别是否在 history.replaced；`min_product_version` 比较另一个约束。当前 parse_version 只抽取数值版本、忽略预发布后缀，不能宣称实现完整 SemVer 次序。替换记录的身份、摘要、类别和当前接替者一起核对。

## 4. 隐私与证据

scan_privacy/scan_privacy_doc 检查绝对用户路径、UNC、URL凭据、Bearer及密钥样式。结构化身份使用受限词法；自由文本先脱敏或拒绝输出。规则匹配有其适用域，不能据“扫描通过”保证任意文本绝无敏感信息。必要原始路径可在用户本地运行记录保存受控映射，公开产品引用稳定身份与相对路径。

验证分别覆盖字段篡改、输入重排、同一摘要不同运行时间、缺 sidecar、错误 schema、预发布版本比较、跨对象替换、隐私和实际消费者。旧测试类名是验收意图，未提供执行器时不当作已执行证据。Python 溯源层与三阶段 C++生产接线分别核验，完整原文见[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)。
''',
    'docs/engineering/contracts/ATOMIC_PUBLISH.md': r'''# 原子发布

本合同定义正式产品的可见性、完整性与持久化；[阶段交换](../data/PHASE_PRODUCT_EXCHANGE.md)定义消费者，[产物库](../data/ARTIFACT_STORE.md)定义 typed 对象。单文件原子 rename、整棵目录发布和多文件完成协议是不同机制，不能统称一次不可分事务。

## 1. 发布协议

1. 在本运行私有 stage 生成内容，确认输入、目标路径、类型与覆盖政策。
2. 完成写入和关闭/flush/fsync；按格式核对结构、校验和、单位、几何及必要引用。
3. 计算实际内容哈希与大小，等待所有 AIO 任务完成。失败不能把未完成对象列入产品。
4. 同文件系统提升文件或整目录；需要多个文件的产品保留明确的完成协议。
5. 最后发布消费所需完整清单，说明真实文件、模型、哈希、能力和来源。

完成表示产品按声明完整，precision 可以显式 unavailable；它不表示模型已经独立科学验证。兼容外部 HiPS 可按输入合同读取信号，不要求它由 ACSD 的某个 Python 发布器生成。

默认不覆盖已完成产品。显式覆盖时先完成新内容暂存，旧产品保持可恢复副本，成功后再回收；失败报告实际目标和恢复状态。路径来自显式输出配置，输入只读，产品不落源码或进程 CWD 的隐式目录。

## 2. C++ 机制接口

`lib/infrastructure/aio/product_io/include/astro/aio/atomic_publish.h` 提供 `atomic_publish_file`、`atomic_write_bytes`、`atomic_publish_directory` 和 `PublishResult`。实际机制先写 tmp、rename，再按 VerifyFn 重开目标验证；可能在验证前短暂可见，验证失败按选项撤销。需要完成清单的消费者不能只凭目标路径出现就读取。

生产者可在 rename 前做内容验证，机制层可在 rename 后重开验证；二者针对不同窗口，并不构成无条件强弱排序。rename 后外部修改、验证后的再次修改、目录持久化失败与覆盖旧目标的风险分别核对。单纯“重开校验通过”不能证明之后永不损坏。

| `PublishDurability` | 可见性与调用责任 |
|---|---|
| `kNotPublished` | 本次目标未发布或已撤销；按状态清理暂存与决定重试 |
| `kDurable` | 已发布且目录项持久化已确认 |
| `kNotDurable` | 目标已可见，持久化尚无证据；明确记录并重试确认，不直接删除回滚或静默当确认成功 |

durability 与 PublishStatus 正交，renamed、target_checked、target_present 说明实际观测。不同平台目录持久化能力不同，status=OK 也不能单独推出 kDurable。调用方不能把“所有错误必无正式产品”作为清理规则。

## 3. Python HiPS 发布器

`lib/infrastructure/aio/io/hips_output_store.py::HipsOutputStore` 使用：

```text
<root>/runs/<run_id>/stage/
<root>/runs/<run_id>/products/<user_path>/manifest.json
```

publish_directory 校验相对路径与允许文件名，在 stage 写完并核对每个图像 tile，再让旧目标移至 `.replaced-*`，逐文件换入新目录并最后写 COMPLETE manifest；失败尝试恢复旧副本，恢复失败保留并报告副本位置。这是完成清单协议，非一次整树 rename。

允许 properties、Moc.fits、规定的 Norder/Dir/Npix FITS 和数据集级 coverage.index.json。metadata、frame_science 与模型文件等正式产品组成由各产品合同和实际发布器说明，不能套用这份局部文件白名单拒绝所有其他科学元数据。

路径词法拒绝绝对路径、父目录、空段、反斜杠与不安全字符，并检查已有符号链接组件。权限模式是具体平台实现；路径检查与实际打开之间的竞态仍需专门验证，不能凭词法检查宣称全部文件系统逃逸已排除。同 run 同目标的发布由调用方串行化。

## 4. 清单、tree hash 与存储形态

Python 产品清单为 `acsd.hips-output-manifest/v1`，包含 manifest_version、COMPLETE、run_id、user_path、product、publisher、tree、tree_hash、fitsverify、created_utc 和可选 producer。tree 每项为 path/size/sha256；tree_hash 对这些项归一为三元组，按(path,size,sha256)排序，以 UTF-8 紧凑 JSON 后取 SHA-256。

fitsverify 只记录实际检查范围：图像 tile 的结构和所用校验卡，Moc.fits 的 BINTABLE 不在该图像验证器支持域内。未检 tile 不计入 tile_count；没有 DATASUM 不能写已通过 DATASUM。结构和校验和不证明天区、科学单位或像元映射正确。

裸 `.hips/` 与 `.hips.zst` 的互斥形态、索引、容器哈希和逻辑产品 tree_hash 见[存储形态](HIPS_STORAGE_FORM.md)。归档容器完整性与解压后内容等价分别验证。运行级 storage 元数据和产品级 tree 清单分别保存，不能互相替代。

## 5. 当前边界与验证

typed Python Store 的内容/manifest/hash/provenance 是另一多文件顺序；C++ CLI、Mosaic writer、Normalize signal+sidecar 是否共享事务，分别按生产调用登记，见[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。库接口存在不能推断所有命令都符合目标发布协议。

故障实验覆盖写满、部分写、取消、每个 rename 前后、验证失败、目录 fsync失败、覆盖恢复、并发同目标及读者过早消费，保留失败尝试。逐文件原子性、整产品完成性、持久化、读后完整性和科学正确性分别报告。旧原文、历史测试名与未接线注入保存于[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)，本轮未执行发布实验。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Product source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
