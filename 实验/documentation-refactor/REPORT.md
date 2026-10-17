# 文档重构：范围、处置与待办

本轮直接在 main 整理文档，科学与数学疑点留待后续统一推导。本报告记录实际改动和覆盖范围；**全仓逐篇实质重写尚未完成**，不能将清单生成、路径迁移或结构检查记为阅读全文、文献审核或科学验证。

## 范围与原文身份

基线为 `63eccbb6133aef56389641d43c55df6e243819c5`。从 Git 跟踪树建立[原始文件清单](docs/ORIGINAL_FILES.tsv)，覆盖 702 份技术文本，包括全部 191 个 docs 文件、246 份实验文本及根说明、源码说明、schema 和构建文本。实验集合比最初 231 份统计多出 .bib/.log 等文本；两个数的分母不同。

每行保存路径、Git blob、字节数、处置、内容归属和实际阅读范围。它是本轮集中处置记录，不建立审批状态。以原 blob 和基线可恢复原文，例如：

```sh
git show 63eccbb61:docs/detail/registry/acsd.phase1.drizzle.md
```

未跟踪的 wiki、归档目录、工作包以及 lib/healpix_db、lib/phase2 不纳入原始清单，也未移动或删除。用户粘贴方案保存在附件中，本报告不改写其完整交付要求。

## 已落实的结构

| 内容 | 归属与实际变化 |
|---|---|
| 根入口与协作 | 重写 README、AGENTS，统一 main 工作方式，修正不存在路径和过强科学能力描述 |
| 最高设计 | 按目标、三阶段、资源、I/O、GUI 和证据重写；公式和文献依据指向专题，不另立简化模型 |
| Normalize 科学链 | 按观测量、依赖、校准、WCS、PSF/光度、条件噪声、球面完成、产品及验证重写；保持参考、单位、GLS 和同次 R 定义 |
| 排异与集成 | 重写为候选资格、归约、六态、参数示例及后选择误差的连续说明；数值策略没有切换 |
| 公共 API | 全文读完来源清单并逐字归档，正式页收缩为接口规则、真实头/模块定位与关键实际声明；全函数实现验证未完成 |
| Runtime 与寿命 | 对照 runtime/module/context/artifact 及 CLI 实际代码，重写 Runtime、所有权合同与基础设施细节，明确调用方创建 ctx 和异步借用边界 |
| 测光文献阅读 | 保留 44 项书目，原研究注记完整归档，冲突模型陈述归问题记录，正式页区分阅读任务与核验结论 |
| 模块正文 | 25 份登记页的完整有效正文归入三个阶段 modules.md 和 infrastructure/hips_writer.md；按生产依赖排序，保留局部 session 与 CLI 的区别 |
| 文档维护 | DUAL_LINE、TRACEABILITY、DOCUMENTATION 并入 DOCUMENT_GOVERNANCE，统一协作、写法、追溯和索引 |
| 科学附录 | CCD 缺陷观察、天测预算、检测记录与 PSF 复算附录独立保留于本单元 docs；PSF 示例另存 code，未执行 |
| 目录导航与机器定位 | 修订 Markdown、源码 README/module.yaml 及 JSON 中的文档路径；JSON 结构、数值和科学配置不变 |
| 科学问题 | [23 项待推导/核验问题](docs/SCIENTIFIC_QUESTIONS.md)及 15 项工程/文档缺口，各列位置、影响和所需证据 |

旧到新的具体文件关系见[路径迁移](docs/PATH_MIGRATIONS.json)。当前 docs 为 167 个文件；减少 28 个原始页，增加 4 个模块正文。目录 README 是阅读入口，不承担第二份接口定义。

原始文档中保留的正式专题不因“文字看起来完整”而标为已验证。数学恒等式、参考通量条件、科学上界前提和用户自由组合政策未在本轮重新选择。生产源码、测试、构建和科学算法均未修改；新增代码只用于文档整理与结构检查，另保留未执行的复算示例。

## 已读与核对范围

完整阅读的主题包括最高设计、统一模型/数据语义/适用域、Normalize 科学链、校准/缺陷、天测/检测、PSF、光度、噪声、Drizzle、背景、排异、集成、投影/重采样、HiPS 格式，以及相关架构、收敛、问题和模块登记正文。原始清单逐行标明阅读或局部阅读范围。

实现抽查了 CLI 的 run_phase1_block 和三阶段入口位置、P1/P2/P3 节点分派、Normalize 公共类型、sidecar 公共结构、parse_reference_config、Drizzle precision staging、QR 信息相减与集成 reducer。模块原文中未逐函数重新核对的实现陈述保留其限制，不能因此称全部 API 已核实。

公共 API 清单全文已读，来源字节保存在[PUBLIC_API_SOURCE.txt](docs/PUBLIC_API_SOURCE.txt)，检查器与基线逐字比较。Runtime、Module、Context、Artifact 公开头及 CLI 的工厂/借用调用已经核对；它们不代表全部算法函数逐一实现验证。

校准算法页已读，其他超长算法资料、尚未处理的工程合同和实验树仍只部分查阅，不能记为全仓精读。外部论文与标准没有逐项重新联网阅读；文献目录也未完成统一核验。

## 待推导和实现风险

[问题记录](docs/SCIENTIFIC_QUESTIONS.md)区分数学定义、已有条件证明、仍缺误差证书和静态代码风险。主要待办包括跨响应的输出估计量、完整域空间标度、PSF 求积与拟合一致性、QR 秩及通量精度、参考到场景转换、稀疏支持、背景规范/交叉项、排异后选择以及曲边与导出误差。

静态抽查还发现：p1_op_drizzle 销毁 PipelineFrame 后，缺有效 EXPTIME 的分支仍可能读取该 frame 的 header。该路径已列为 E07，没有运行验证或代码修复。其余已定规格的迁移继续由[实施差异](../../docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)承担，不当作自由选择新模型的授权。

历史实验原文和故障注入脚本没有批量改写。它们可能引用退役路径，按基线 blob 或迁移表解释。配置 JSON 的旧 quote、章节和内容哈希也不会因文件路径更新自动获得正确性，问题记录 E09 明确这一限制。

## 检查与过程

可复跑结构检查：

```sh
python 实验/documentation-refactor/code/check_documentation.py
git diff --check
```

检查覆盖当前 docs、根入口及本单元新增 Markdown；历史实验不属于本轮修复链接集合。它验证 YAML 重复键/路径、索引与当前文件集、上游文件、模块正文完整保留、迁移后新增失效链接及 JSON 元数据变更。结果保存在[CHECK_RESULTS.json](docs/CHECK_RESULTS.json)，只能解释文档结构。

整理过程出现的失败保留如下：

- 第一次 apply_patch 同一路径同时 Delete/Add 被工具拒绝，没有写入；随后改为明确文件内容写入。
- 迁移脚本遇到 lib/algorithms/platesolve/cpp/ipv/make_clean_err.txt 的非 UTF-8 编码报 UnicodeDecodeError，后续明确跳过并保持原文件。
- 初次索引检查误把未跟踪 __pycache__ 当正式文档；改为基线 Git 跟踪集加明确新增文档，不删除缓存。
- 首次链接检查发现 REPORT.md 尚未生成的两处新增链接；补齐本报告后重新检查。
- 历史 doccheck 夹具被路径替换触及，已从基线逐字恢复；历史行证据不转写到新文件的旧行号。
- 检查结果文件首次生成前，报告已经链接它，触发新增链接错误；先写 NOT_RUN 标记再执行检查，成功结果由真实检查覆盖，避免把占位当通过。
- `[r_i-r_j](p)` 是显示公式的一部分，首版简易链接扫描将其误判为本地文件；扫描器明确排除代码与 LaTeX 数学域，保留真正文件链接检查。
- 一次批量 patch 因预期行不匹配整体拒绝，没有写入；API 全文读取后改为完整重写。一次工具存储键不可用发生在文件写入后，随后直接读文件确认写入状态，没有重复覆盖归档。

未执行构建、科学单元测试、数值推导、真实 FITS 管线或图像视觉验收。纯文档与路径整理不生成这些证据。

## 完整任务仍需完成的内容

剩余范围在清单中明确标为尚未精读或局部阅读：长篇算法资料的数学/工程拆分、其余工程合同的全文改写、模块的全函数/API 独立核验、实验文档逐篇归类、统一文献库核验，以及全体系再次穿行复读。PUBLIC_API 的正文拆分已落实，逐函数实现与并发验证仍在上述剩余范围。

本轮完成的是可审阅的入口重写、模块/规范归并、证据分离和问题集中记录。它没有完成用户方案中所有 191 篇逐篇六动作，也没有把问题登记当作科学推导或全仓实质重写的替代品。

## API 与 Runtime 续整理记录

全文检查 API 清单时，确认它混有 legacy orchestrator、局部 session 和真实 CLI 八/七/五节点的陈述，重复表格之间也不一致。重写后的正式文档通过真实模块和公开头定位有效接口，不把来源中的“全部真实存在”“bitwise”“未进根构建”等字样当当前科学事实。

Runtime 续查确认：run_pipeline 使用 RuntimeResourceBudget 工厂，在调用方栈上创建 RunContext，协作取消监视线程在同步 run 后 join；IModule 的公开成员不含 create/destroy，模块寿命来自工厂与 RAII。ThreadBudget 的三种获取政策和 ThreadLease 的真实预留/兼容大小租约分别说明。

截至本次整理，20 个原始文件已重写、28 个来源页已归并、4 个附录已分离。完整来源身份和逐文件范围以清单为准。没有修改 C/C++ 实现或运行科学程序。

## 日志与资源主题续整理

用户补充要求每次提交立即推送，已写入 AGENTS。第一次直连推送连接重置，第二次连接超时；随后发现本机可用 SOCKS 代理，成功 fetch。远端已推进到 `5b33e0417`，先合并其 Normalize Round 3 代码与实验记录，再将文档提交和合并结果以 `015a18b26` 推送 main。没有强推或重写远端历史。

此次新增全文重写6个原始文件：日志错误合同、错误模型、日志系统、可观测性、性能模型、资源监控。结构化日志修订来源权威与运行层接线说明；开发模式、探针标准、运行图全文已读，保留其专题内容。日志/资源旧正文完整保存于 OBSERVABILITY_SOURCE.txt 与 RESOURCE_RECORDS_SOURCE.txt，历史数字和相互冲突的评价规则没有丢失，也未变成新的运行结论。

静态核对覆盖核心 Error/Result/Cancel/ExitCode、CLI 实际域映射与失败 manifest 分类、probe.h/dev_diagnostics.h、CMake 开关、快照锁位置、monitor.py 的字段和指纹函数以及 trace_feed.py 完整 observer。新增 Q24 资源测量问题和 E16–E19 接线/身份/重复类型记录；本轮不改资源阈值或采样实现。原有 E01–E15 是合并前事实，按新代码逐项重核，不能用旧结论覆盖远端修复。

累计26个原始文件已全文重写，28份归并、4份附录分离，24个待推导/核验问题与19项工程记录。结构检查仍覆盖167个 docs、25份归并模块正文及15份仅导航字符串变化的 JSON。源码是合并远端而更新，文档整理自身没有修改 C/C++。尚未执行科学运行或宣称全部191篇完成。

## 构建与发行主题续整理

日志资源主题以 `2d0d442b5` 提交后立即成功推送。继续全文阅读 BUILD_GRAPH、BUILD_NODES、RELEASE、DEPENDENCY 和 ABI，核对根 preset、CMake 目标/链接声明、安装规则、图生成器、构建指纹声明源集、版本生成器与两平台打包入口。

重写前四份构建/依赖正文，原文按 blob/字节数完整保存于 BUILD_SOURCE.txt。过时的34目标手抄表、旧复现任务编号和未接线声明归档；正文改为当前构建关系、配置条件、工具局限及可执行入口。源码路径列表摘要、声明源内容摘要与实际二进制哈希分别解释，Windows CLI zip 和目标 Inno Setup GUI 安装包分别解释。新增 E20–E22，没有改编译器、指纹、打包器或产品算法。

累计30份原始文件全文重写；文档检查和源码阅读不代替构建、打包或实际科学验证。

## 数据、溯源与发布主题续整理

构建主题提交 `f23ce01e1` 后立即推送。继续全文阅读工件、产物库、溯源、阶段交换、原子发布、运行清单与统一工程对象，重写 ARTIFACTS、ARTIFACT_STORE、PROVENANCE、ATOMIC_PUBLISH 四篇，原文完整存入 PRODUCTS_SOURCE.txt。保留既有 DATA ID 的定位，删除旧表对模型、单位、未知误差和最终辅助通道的重复定义，统一引用科学正本和阶段交换政策。

实际阅读 Python Store 的 start/publish、HipsOutputStore::publish_directory 与覆盖恢复、provenance 版本解析/摘要声明、完整 atomic_publish.h。区分 Python 语义层与 C++ 生产接线；内容/清单/sidecar多次rename不称整体事务；发布可见性、完整性与持久化分别报告。新增 E23–E26，未执行 I/O 故障注入或更改发布代码。

累计34份原始文件全文重写。完整任务继续以 ORIGINAL_FILES.tsv 的实际范围推进，历史来源归档不作为余下文档精读的替代品。

## 空间索引、PSF与本地星表续整理

产品主题 `d21ec9b2c` 已立即推送。完整阅读公共 Gaia/噪声/诊断算法、HEALPIX_MAPPING、STAR_PSF_ALGORITHMS 和 Gaia detail，另核对完整共享HEALPix/Gaia头及PSF头、缓存比较实现。重写HEALPix数值说明、Moffat4算法说明、Gaia模块接口，PSF布局/调用细节并入已有 star-psf 模块段，原文完整存入 SPATIAL_NOTES_SOURCE.txt。

保留Moffat4公式、积分与残差系数，有限样本历史读数按证据归档；科学与数学疑点只记录，未换模型。新增Q25离散往返容差及E27–E29缓存/测试载体/状态布局问题。星检测逐算子长页只部分读到，未记全文完成。

累计37份原始文件全文重写、25个待推导问题、29项工程/文档记录；未完成的超长算法及其余正式和实验正文仍继续处理。

## 目录入口与工程标准续整理

空间主题 `e014863df` 已立即推送。阅读全文核对28份目录指南并改为对应主题的实际Markdown阅读路径，修正退役DOCUMENTATION指针、旧“build目录未跟踪”描述、噪声旧吸收任务和三输出模式等过时入口。Git实际列出构建四文档，未改.gitignore。

阅读全文核对CODE、COMMENT、CONCURRENCY、CACHE、COMPATIBILITY、OPTIMIZATION、NUMERIC，重写前六项；旧文保存在STANDARDS_SOURCE.txt。科学单位/浮点正文保留，其历史诊断附录仍待移出。标准统一接口/寿命/资源责任，区分本地查询与外部网络、受控多轴预算与隐藏嵌套、科学容差与逐位复现，新增E30。

累计71份原始文件全文重写（含根与目录入口），另有已读保留与归并主题；该数包括短导航，不能把导航改写量当作超长科学算法或实验精读的替代。科学问题仍25项，工程记录30项；全部剩余范围继续如实列出。

## 调度与异步队列续整理

目录/标准主题 `10e78d380` 已立即推送。全文阅读 PIPELINE_BLOCK、SCHEDULER、ASYNC_IO，核对完整scheduler.h与async_io.h、executor注册头、两类事件schema；重写后两份合同，旧文保存在SCHEDULING_SOURCE.txt。纠正探针与stdout协议同格式、Mosaic全局拟合可按窗口完全独立、错误取消后必排空、容量最低1即满足预算等过强描述。新增E31–E32，本轮未运行或修改调度/队列。

累计73份原始文件全文重写，25项待推导问题、32项工程记录。仍按清单处理超长算法、剩余工程/detail及实验正文。

## FITS/HiPS与AIO续整理

调度主题 `1bef89b3b` 已立即推送。全文阅读FITS_IO、HIPS_IO、AIO detail及common两份统一对象/模型页；重写前三份，区分生产物理值读取、低层存储值流式C接口、内部C++发布writer。核对fits.h全文、C接口头相关段、aio_fits.cpp实际缩放位置及发布机制，新增E33–E34。

原文完整保存在IO_SOURCE.txt；正式文档不再把归档目标、纯C接口、格式库枚举和阶段发布能力混为一体。累计76份原始文件全文重写，科学问题25项、工程记录34项；检查只覆盖文档结构，不是I/O运行验证。

## 配置、CLI与运行清单续整理

I/O主题 `759541170` 已立即推送。全文阅读CONFIG、CLI_PROTOCOL、CLI detail及运行清单合同，核对完整command_tree、cmd_verify、write_run_manifest与预检/parser相关分支。重写MANIFEST_VERIFY与CLI detail，修正协议中的取消/源码镜像描述，原文保存在CLI_RECORDS_SOURCE.txt。

新增E35–E36，区分doctor实际校验、未接线画像hash、收尾now时间与真实运行起止、flush/rename与持久化确认、运行JSONL与非运行单JSON。没有运行doctor、生成模板或修改CLI代码。累计78份原始文件全文重写，25项待推导与36项工程/文档记录。

## CPU后端、画像与loader续整理

CLI主题 `922ecfb65` 已立即推送。全文阅读benchmark两页、BACKEND、CAPABILITY_PROBE、AVX2_PROVIDER、ISA_VARIANTS和SECURE_LOADER，重写这7份正文，原文含全部历史读数/位表/测试矩阵保存在CPU_SOURCE.txt。实际核对当前CMake相关目标、AVX2查询门面、capability的classify、CLI画像生成/保存与消费及secure_loader平台分支。

正式解释区分两族ABI、清单、能力位、门面/计算TU、实际路由与可构建DSO；不将共享源当独立oracle，不从两次同阈值比较推出科学误差等价。新增Q26及E37–E39，保留原数值容差与算法；没有执行benchmark、反汇编、加载或科学运行。

累计85份原始文件全文重写，26项科学/数值待核验问题，39项工程记录。长篇算法、GUI/测试、部分detail/存储及实验资料继续处理。

### GUI 与浏览组件重写

全文核对 GUI_DESIGN、hips_browser、CLI 事件合同及 STF 入口，重写两页并逐字节保留 GUI_SOURCE.txt。区分完整 GUI 目标、已有 Qt 浏览组件、规划工程、实际事件字段与显示算法；登记 E40–E43，不修改显示/分组算法。清单累计重写原文件 87 篇，仍有未精读主题；此数字不代表全仓完成。本轮未构建 Qt 或执行事件回放。

### 测试与验证证据重写

全文阅读 TEST 与 VALIDATION_EVIDENCE，重写两页，将旧轮次门表、mutation矩阵和流程原文完整保存为 TESTING_SOURCE.txt。正式正文保留独立真值、仿真/真实观测分工、计数与失败记录、适用域和视觉评价，明确测试范围按当前影响选择；容差数值保持，普适性疑点列 Q27，旧模式最优性与接缝阈值列 Q28，另登记 E44–E45。累计重写原文件89篇、待推导28项。本轮没有执行科学测试，不代表全仓精读或产品验收完成。

### 存储形态合同与接线细节

全文阅读并重写 HIPS_STORAGE_FORM 与 storage_form，保留原字段、序列化/身份、归档/索引目标和TRIM条件，完整原文保存在 STORAGE_SOURCE.txt。核对 parser、dead_config_keys、tree_hash、geometry setter与manifest、publish_finish及打洞原语；分别说明目标支持和真实分支。新增 Q29 覆盖量化/保守候选、E46–E48生产者冲突/打洞失败/几何校验问题。累计重写91篇、29项待推导；未执行归档、打洞或科学产品读取。

### 共享库与排障、来源字节核对

全文阅读 shared_libraries、troubleshooting 和相关公开头/真实实现，重写两页并保存 SUPPORT_SOURCE.txt。登记 E49–E51诊断器旧映射、帧身份覆盖与精度/摘要误用；未更改实现。结构检查新增来源档案边界、字节数、Git blob与原提交对象核对，46份来源正文均一致。索引167篇、25份模块内容保留、无当前Markdown断链，15份机器元数据JSON仍仅字符串迁移。累计重写93篇；尚未完成的算法长文和实验精读继续，不把这些结构检查当科学验收。

### Normalize 远端实现后的文档同步

全文补读 NORMALIZE_DATA、接口规格、DATA_FLOW及上层生产架构/实施差异，阅读空间PSF完整新增源、相关远端diff、生产adapter与第三轮报告。四份文档按实际新增能力定点更新：空间PSF/alpha、逐点诊断、显式残差、因子基础单位检查和frame_science文件边；保留未满足的Runtime对象仓、SED、支持/状态和真实applied运行限制。旧正文保存 NORMALIZE_STATUS_SOURCE.txt，来源字节核对累计50份。Q30记录留出χ²及不同指标比较解释，E52–E53记录更新事实和支持边界。未改科学实现、未执行报告中的程序；清单不把定点更新当整篇重写数量。

### Mosaic 归约与旧UPM算法卷

全文读 UPM_SOLVER 与 PHASE2_INTEGRATION，核对完整integrate头/实现、真实p2_op_integrate和背景库/生产调用，重写两页为数值方法补充；真实API/六态/计数与并发补入模块正文。旧正文完整保存在 MOSAIC_REDUCERS_SOURCE.txt，来源核对累计52份。Q31记录控制方差/弱锚/归一统计疑点，E54–E56记录过期五态与生产路径、真实计数和无效值分工。累计重写95篇，原模块内容仍保留；未运行科学测试或改求解/归约实现。

### 星检测算法与接口分层

全文读检测算法卷和逐算子detail、重读科学正本、当前公开头与关键实现/CLI消费。算法卷保留行差分/平滑递推/候选分裂/高斯拟合/质量常数与数值域，detail改为实际六入口、13参数、列精度、所有权、坐标和生产消费者；旧长文、备选方法、召回表及引用冲突保存 DETECTION_SOURCE.txt。新增Q32核/边界/去混叠等价疑点、E57–E59参数/坐标/能力分支问题，已有统计疑点继续沿Q07。累计重写97篇，来源原文字节核对54份；无当前Markdown断链。未改变检测实现、阈值或PSF模型，未重跑历史实验。

### 天测与光度数值方法、接口归属

全文读 PLATESOLVE、PHOTOMETRIC_FIT，重读天测/光度正本、公开IPV参数/结果与实际拟合/积分/消费者段。两算法页保留匹配/投影/积分/稳健方法，API/内存/ABI/原点/状态补入模块正文；旧长文、失败流程、测试/引文与历史读数存 ASTROMETRY_PHOTOMETRY_SOURCE.txt。Q33登记稳健尺度/中心/效率主张，Q34登记严格同亮度位次的有效域，E60–E61说明真实触顶发布与生产流程。累计重写99篇、待推导34项、56份来源字节核对；未改变选星、积分或IRLS，不运行科学验证。

### 第二次远端整合与参考约定差异

提交e6d7524f0后首次push因远端领先被拒；fetch得到75e0b2b01..fec3dba76五个提交，无冲突merge为120c1446e并立即push成功，无强推。保留参考通量/PSF/应用身份/量化测试和全部新实验。阅读相关生产diff、ROUND4/7/9全文及所列空间alpha/PSF留出日志，三份实现说明同步当前静态事实，旧文保存REMOTE_PRECISION_SOURCE.txt。Q35登记完整响应积分与位置参考参数的不同约定，不据新实验改科学正本；E62–E64记录接线更新、应用摘要精度风险和SED建议边界。来源字节检查59份；本轮未执行远端测试或复跑其科学数字。

### 坏点、坏列与方差记账

全文阅读 COSMETIC_ALGORITHMS 与 CCD_DEFECT，核对检测、逐点修复、列修复、方差及 FP64 包装实现。重写算法正文，将真实接口、掩码、计数、生命周期与精度限制补入模块页；旧正文完整保存在 COSMETIC_SOURCE.txt。Q36 登记修复位置方差与邻域传播/非线性中值的区别，E65–E66 记录零检出、串行实现、降精度、参数与列计数行为。累计重写100篇，36项待推导，60份来源字节核对通过；当前索引167篇、无 Markdown 断链。未修改或执行缺陷算法，后续长文与实验范围仍继续整理。

### Issue 6：有效设计信息复核——检测算子与接口

依据用户新指令与 https://github.com/fujiaze/Astro-CS-Database/issues/6 ，重构重点改为有效设计信息完整。先前 SOURCE 逐字节保存不能证明正式正文语义完整；已发现检测逐算子细节、真实签名、初值/退化/异常和外部参考只留档案的遗漏，本批补回正式层，不用篇幅或重写篇数替代完成判断。PUBLIC_API、PLATESOLVE、PHOTOMETRIC_FIT及其余191篇的第二次语义复核仍继续。

本批重新完整阅读基线 star_detection.md（519行）和 STAR_DETECTION_ALGORITHMS.md（434行），与现行 science/detail、当前 star_detector.h全文、sdet_api主生产链/逐算子/分配释放、sdet_image递推、nls_lm声明与真实停机段对照。正式detail恢复全部O1–O16，science补观测模型、Jacobian、阻尼与通量推导；六个API完整签名、13参数、九extras/六核心列、预测身份、异常/空表/并发/复杂度及输入到API例自足给出。

| 基线实质设计项 | 现行正式归属及语义结论 |
|---|---|
| O1/O2噪声、核与边界 | science保留差分/相关和平滑传播推导；detail给3轮5σ、少于8点、常量边界种子及有限填充；纠正“完整Triggs矩阵”和平滑已并行的过强描述 |
| O3/O4/O8阈值、岛、峰与边界 | detail保留严格比较、4/8邻接、3×3最少高邻居、11×11与平局；数值平顶与物理饱和分开 |
| O5–O7/O9/O10 | 中心过零式、实际平台边缘盒、零交叉4096步/回退、盒采样/局部背景与振幅门完整；纠正未饱和重心、带符号距离和LM初值误述 |
| O11/O15 | 29个内部阈层、根/枝阈底、轴矩+0.25/确定argmax、候选/拟合后两种去重、排序/截断保留；删除自动替换授权，不称与原实现完全同构 |
| O12/O13 | 直接七参数+解析Jacobian、正规方程Cholesky、6/5参数重试、失败与五码/O13b实义保留；旧SX/fr参数化不作为当前实现；E67登记真实统计/停机风险 |
| O14/O16 | 方窗总和、幅度A、输出饱和列及Gaia预测计数分开；给来源重绑、提前返回/全NaN计数范围 |
| 研究结论与备置方法 | 召回历史读数不升为生产界；CLEAN/DAOFIND/Kron有价值方法及未启用身份保留正式正文，常数/转引不作已证实理论 |
| 参考代码与许可 | 已访问SExtractor 2.28.2 refine.c与SEP1.4.1/Netlib索引，核对根流量、随机重分配及GPL/LGPL差异；未取得作者论文PDF/出版入口，明确不冒认逐页核验 |

本轮没有构建、执行检测/LM/召回实验。引用的tests/p1star路径在当前跟踪树未查到，不将源码注释当测试已存在；完整函数异常屏障和生产坐标/来源绑定仍需实现Agent核验。原始文件清单直接更新这两篇的语义保留结论，不另建台账或审批体系。

### Issue 6：天测选星、匹配与完整IPV接口补写

重新完整阅读基线PLATESOLVE.md（361行）和现行天测科学/算法，核对ipv_api.h全文、select样本/割线/交付段、triangle.cpp全文、itertrans拟合/裁剪/双向重匹配、solver生产后处理与重投影、WCS提取/逆拟合/后置拒绝、entry结果/ABI/缓存段。PLATESOLVE补回样本集合、位次、计数割线的离散公式与伪代码、三角投票、TRANS与SIP数值步骤、误差/失败和参考；新IPV模块详细设计给全部14公开声明、所有参数/结果字段、回调N×6/inlierN×9、所有权、线程覆盖、状态及连贯调用例。索引和公共API入口指向该完整正文，避免“看源码”代替接口。

关键订正：0.002是描述符距离，±20%才是初始尺度；三角匹配全枚举最坏n^6，不是KD O(nlogn)；选星NaN尾排不等于排除；n_detected/n_catalog当前公开输出置0；逆SIP内部7阶不经36项C结果导出；非线性线性项奇异已明确失败，旧“仅warn”不当作现缺陷。E68登记当前is_ok初值、trans.sig平方单位/消费、截断启发式及选样风险，不改代码或算法。稳健扩增参数按公开头/调用解释，未声称逐条实现/运行已独立验证。

原文F1–F6的有效验证意图归正式测试说明和连贯例，历史数值/过往实跑仍保原证据身份；所有原引文恢复正式著录或具名参考，不借版本标签声称本轮已读完其源码/论文。PUBLIC_API整篇与光度等复核尚在进行；未运行IPV、构建或科学实验。

## Issue 6：光度积分、稳健估计和实际接口语义复查

完整重读基线PHOTOMETRIC_FIT的319行，逐项核对F1–F7、尺度路线、伪代码、退化、确定性、积分缓存、复杂度、参考及未接入孔径设计。正式算法回填物理模型、nm到SI的单次换算、Akima斜率/Hermite、全部Simpson分支/误差条件、量化传播、严格双向匹配、完整固定尺度IRLS、所有退化及当前复杂度；未把历史“50%继承/11.9%/中心一阶偏差”作为已证定理恢复。新增光度detail含7个完整C签名、全部实参语义/布局、frame三种对象全部字段默认、资源/调用/状态和孔径路径，历史SOURCE不替代设计。

阅读当前公开头、质量位头、frame头与完整装配、v2主实现及相关simple/旧f64、投影包装、SpectrumIntegrator/StarMatcher和孔径实现；对照合成口径/推导与路线报告（报告仅作既有证据，不把其中包络称严格上界）。已区分已修的NO_DATA/fit_ok、NaN初始化、Simpson三段与孔径框扫描；新静态差异见E69。核对Akima作者出版页、SciPy方法说明、Gaia论文入口和NIST作者原文入口；1992表格/所有旧论文尚未逐项获取全文，不称引文全验。未构建、未运行光度/真实帧，生产源码保持未改；PUBLIC_API的2325行基线整体语义复查尚未完成，191篇不能据本批宣告完成。

## Issue6：位置参考、当前GLS、PSF与量化证据同步

完整复读NOISE_SNR、UNIFIED_SCIENCE_MODEL、NORMALIZE_DATA、Normalize接口规格、PSF、实施地图、GAIA_QUERY以及ROUND4/RUN/ROUND7相关原文，核对当前sparse_reference显式残差实现与已有alpha调用。按本次明确授权统一F_native、F_ref=α_*F_native、F_applied,total三者及模板，保留全域归一的另一参数化，给出V_total=c²V_ref、I/T及SNR换算；h_sphere均用R h_ref，原生P保持analytic_total。Q03/Q35改记已统一语义和剩余误差，而非继续拒绝同步。

Q10旧t_norm2−proj已修，历史反例保留，剩余秩阈值/误差证书分开；现代码i_f相对平方阈也明确解释。空间PSF采用同518星train414/hold104、同L1的0.0986/0.2058，旧3.5倍不当现结论；GAIA_QUANT0.21%/1.8%缺真实原样本，不能因构造能过的总体补出处。ROUND9/issue5的单位/身份生产修复截至本批fetch尚无新远端提交，仍按现代码记录，不抢改生产源。上述为语义同步，并非这几篇完整基线的再次逐项审计，更不代表191篇已经完成；本轮未重跑科学验证。

## Issue6：PUBLIC_API完整基线精读与Normalize接口回填

完整精读PUBLIC_API基线2325行（不是关键词定位），按Gaia、CAL/COS、Drizzle/HiPS、Noise、PHOT、PSF、三个局部session、STAR/WCS、Coverage/SMP/UPM/REJ/INT、Mosaic工具、P3 WCS/RSMP/FITS、来源/所有权与并发逐段对照现正式模块；完整复读Normalize/Mosaic/Export模块与writer、Runtime合同，核查当前astro_calibration/dynamic_psf/hp_drizzle_api/snr_estimator/runtime/module及三个session头，Noise注册表/包装/fill实现和PSF相关入口。

正式Normalize modules已补18校准/缺陷完整声明、7 PSF与13字段、27 Noise和15结构、14 Drizzle及结果/反向/异步/捕获，参数含义/单位/容量/异常/所有权随声明解释；Runtime合同补全部公开类型/字段/方法，PUBLIC_API增15会话完整签名和唯一主题归属，避免改回“回历史/头自己拼”。保留旧错误码差异但不把工具裸整数与CLI同数值称缺陷；纠正Noise无锁风险已修、PSF压缩回映射、取消点与生产调用图，E70留核对边界。

仍未完成：Mosaic和Export全部详细API形参/结构及实现分支的逐项语义验证；HiPS writer完整实参/布局；Gaia接口再核与191篇其余专题（包括既有重写篇的再次内容保全对照）、246实验按需读取及最终跨层复读。本批“API基线全文已读”不等于API所有主题已经闭合；原始清单现行重写统计100不能当191完成率。未编译、未跑科学/ABI/并发测试，不改Agent生产源码。

结构检查的旧“25模块正文必须是基线全文连续子串”拒绝插入完整接口，也会强迫保留已过时PSF调用描述；此规则不能证明语义保全，现去掉该伪语义断言，仅核基线来源非空、迁移锚存在及原文身份，结果名改为mapped_module_pages。语义保全仍由逐项人工对照、现有REPORT/原始清单记录承担，不将结构检查通过当内容完整，也不新设门禁或登记体系。

## Issue6：Coverage完整408行基线对照和实质重写

基线与现行差异仅为导航，因此按同一408行全文逐项精读，另完整读coverage.h/coverage.cpp、AIO/sampler层级消费者相关段、Mosaic管线与UPM上层。保留NESTED父包含推导、并集/外包络、全伪代码、所有错误和两遍协议、复杂度、输入属性、几何/支撑/有效性分名、F1–F6有效检验意图与参考。6完整函数/5结构和每项容量/参数/所有权进入Mosaic正式detail；不是回档案找API。历史全过程追加同一MOSAIC_REDUCERS_SOURCE，按其固定599637e源blob逐字保留，未新建来源体系。

订正原文已过时filter全等拒绝、tile阶误作像元阶、KlogK与漏驻留内存、所有异常已捕获等；当前未检查填充容量/跳过tile错误的静态风险见E71，未修改生产源码。原混阶例增加必要父号条件，粗父重合不证明真实有效重叠。官方HiPS1.0/MOC2.0入口已核，未宣称已阅读全文标准/参考库；未构建/运行Coverage。191篇其余长卷与跨层复读仍继续，原始清单100→101仅表示重写分类，不能当语义闭合完成数。

### Issue6 批次：局部Mosaic session全语义对照

完整复读63eccbb6的PHASE2_SESSION 402行（现文仅链接改动）与当前p2_session.h/cpp全文、基础ABI头及真实工厂注册；上层最高设计、science/engineering/detail索引、Mosaic管线、集成与维护规范已读，UPM沿用此前全文阅读。旧有效设计逐项归modules.md#session：五完整声明、所有参数/span布局/host握手、全部配置及默认、完整manifest/partial能力表、调用顺序/计数、分配/释放/并行/取消、错误和验证负例。算法页保留完整顺序伪代码与复杂度归属，旧任务原文追加同一SOURCE（固定原archive revision精确blob）。当前模型仅run局部且无RAII、partial状态、独立七节点工厂等纠正历史误述；E72记录异常/重复run/rc0空模型等静态风险，不改生产代码。只作静态/文档检查，未运行科学或ABI测试；191篇完整语义对照仍未完成。


## Issue #6：ARS × Runtime/AIO/Block/API 的接口级整合（2026-10-16）

**本批范围**：阅读当前 `PROCESS_RUNTIME_ARS.md`、`PROCESS_RUNTIME_IMPLEMENTATION.md`、`RUNTIME.md`、`PIPELINE_BLOCK.md`、`ASYNC_IO.md`、`OWNERSHIP_LIFETIME.md`、`SCHEDULER.md`、`ATOMIC_PUBLISH.md` 及其工程/细节入口；对照 `PUBLIC_API.md`、`ARCHITECTURE.md`、`docs/detail/infrastructure/{runtime,aio}.md` 的真实现状、`lib/include/acsd/abi/{module_api_v1,host_api_v1,artifact_api_v1,status_codes}.h`、`lib/include/acsd/core/{module,block_frame,executor,memory_pressure}.h` 和 `lib/infrastructure/aio/include/aio_pipeline.h`。其中 v1 标准头与 Block/Executor/Module 的可见声明已读；现有大型生产源文件的真实业务行为沿当前已有调用记录对照，**本批未全文逐函数审计大型 module_adapters.cpp**，不能据此声称生产迁移已经实现。

**发现与处理**：此前两份 ARS 正本给出架构与路线，但缺少 v2 C ABI POD 形状与多 operation 分派、typed store 的 attempt/revision/read lease、原子 MemoryCredit live/reserved 转移、CPU TaskGroup quiescence→AIO drain→真实回收→retry、Host AIO ticket 与 bounded bytes、完整多文件 revision pointer 读者协议、Benchmark 有效域与可执行调度伪代码。新增正式 [PROCESS_RUNTIME_DETAILED_DESIGN](../../docs/engineering/architecture/PROCESS_RUNTIME_DETAILED_DESIGN.md) 逐层说明字段、输入输出、单位/身份、调用序、失败/取消、不变量、伪代码、反例及对应目标代码与测试。回填既有 Runtime/Block/Ownership/Scheduler/AIO/Publish/API 与 infrastructure detail，明确哪部分属现行代码事实、哪部分是 TARGET。把三份 ARS 文档登记进 `DOCUMENT_INDEX.yaml` 并加入工程正常阅读入口，不新建另一套审批/状态台账。

**具体语义纠偏**：现行 BlockFrame 持数值字节和声明消费者而不是跨异步 read lease；RunContext artifact 是描述符而非完整 typed payload；ABI v1 的 execute 走 JSON manifest 且无多 operation/credits/tickets；压力 `evicted` 标记不是释放内存或停止 CPU/AIO 的证明；单文件 rename 不足以证明 signal+sidecar/model 的完整产品事务。此批不更改已有 science 量或生产头文件；Issue #7 既有 EXPTIME 生命周期修复视为后续代码事实，不在此批重复指派。

**覆盖与证据边界**：这是 ARS 主题的文档详细设计和跨合同整合，**不是 Issue #6 原 191 篇全部语义审计终版**。此前的 PUBLIC_API、PLATESOLVE、PHOTOMETRIC_FIT、Star Detection、Coverage、P2 session 等基线精读结论继续按本报告原有批次有效；本批没有把它们的阅读重新计数，也没有深读余下全部原文、Mosaic/Export 全部 API、HiPS writer、Gaia 与 246 份实验文本。未编译新 v2，未执行 ABI、E2E、科学真值、内存故障或性能实验，记录 `NOT_RUN`。新详细设计是 TARGET，不等于 Runtime 已具备这些能力；Issue #6 须保持 open，继续分批审阅其余原文及最终跨层对照。


## Issue #6：ARS-02 接口冻结与校准双 Operation（2026-10-16）

复审 [cebea102] 的 ARS 设计后，按 #6 评论 6218177842 精读当前模块化 ABI v1 的 `module_api_v1.h/host_api_v1.h/status_codes.h`、`core/module.h`、ARS 详细设计；本批还对照 `lib/algorithms/calibration/{include/astro_calibration.h,CMakeLists.txt,module.yaml,src/module_entry.cpp,src/ac_api.cpp,src/calibrator.cpp,src/cosmetic_corrector.cpp}` 和现行 CLI `runtime_client.cpp`/P1 cal→cos 注册/数据流。唯一目标入口位于 [Process ABI v2 冻结设计](../../docs/engineering/api/abi/PROCESS_ABI_V2.md)，嵌入可从单头抽取并独立编译的 C11 函数/结构、Host credits/store/scratch/cancel/logger/AIO/task 服务表、layout/static asserts、buffer probe/fill、Instance serial 策略、两 operation 真实调用、错误/取消/释放/loader pin、正反例与机器迁移入口。

**关键生产信息保留**：两个 op 复用现有 `ac_calibrate_frame` 和 `ac_correct_frame` 科学算子，采用 FP32 单帧单 work unit 和每实例串行；原 `acsd.phase1.calibration/acsd.phase1.cosmetic` 是两个旧注册 module_id，而 DLL 新身份 `acsd.p1.calibration`，显式映射不改旧 IR 默认；Cosmetic 节点的 `ac_correct_columns_ex2` 坏列修复必须继续保留，不能以单个 v2 point-cosmetic op 替代整节点。传统 dark_opt 两式与配置给定 K，不引入虚构的自动优化；method=1 实际是四正交方向逆距离修复而非真正 bilinear。

**范围/验证**：本批抽取参考头后，本地 GCC C11 和 G++ C++17 严格警告（`-Werror`）**编译通过**，LP64 sizeof/offsetof 与正式合同一致；Windows MSVC、DLL 链接/装载、实际 CLI、AIO、内存压力与科学数值回归 **NOT_RUN**。这不是新增可链接 ABI v2；尚未在 main 创建/修改生产 .cpp/.h。#23 科学与资源基线仍为 #24 施工前置；#19 的旧测试失败 26 failed/56 passed 需要独立分类而非计入本批成功。此批推进的是 #6 的 ARS-02 专题，仍不代表原191篇全文审计结束，Issue #6 保持 Open。


## Issue #6 批次：#10 v2 合同、#12 Simpson、#74/#75、ARS-02 科学静态复核（2026-10-17）

**读过并实质核对（本批限定）**：Issue #6 规划评论及 #8/#9/#10/#12/#13/#74/#75 正文与 #12 独立尺度反例；现行 `NORMALIZE_DATA.md`、`docs/detail/normalize/contracts.md`、`PHOTOMETRIC_FIT.md`、`DATA_SEMANTICS.md`、`PROCESS_ABI_V2.md`、`RELEASE.md`、`SCIENCE_IMPLEMENTATION_DELTA.md`；生产 `normalize_science.cpp/.h` serializer/reader/model 接口与发布、`module_adapters.cpp` 实际 p1 drizzle/precision/staging/writer、cal/cosmetic、P2 sparse probe 与 weight 入口、`normalize_precision.cpp` 捕获窗/GLS/模型、`spectrum_integrator.cpp` Simpson/cache/XPSD；根 CMake version/build-stamp、`eng/tools/gen_build_stamp.py`、`build_stamp.cpp/h`、CLI run provenance；Cal `ac_api.cpp`、`astro_calibration.h`、`module_entry.cpp`。部分大型源代码只精读点名的函数、片段，没有逐行审完整 17565 行 scheduler。

**本批新增有效设计**：正式冻结 frame-science/v2 唯一几何/单位/版本、产品状态与函数级施工映射，确定不读 v1、无 +9 迁移/compat；写明当前机器 schema/writer 仍 v1 的 TARGET 实际差距。Simpson 选择等距 nm、双重相对/IEEE 误差限定、合法域、1/3+3/8 解析 oracle 与真实 Gaia XPSD 校验方法。对低 Nside 明确分段测量位置、复杂度可疑但根因未知；对构建版本戳区别现有 build_source_digest 与真实加载 binary hash，并补正 HEAD 与增量依赖。ARS-02 静态确认两真实 F32 内核、坏列 `ac_correct_columns_ex2` 和统一 DLL 双 Operation 的唯一路径；#8 Fisher/面亮度 T、#9 一次转换单位、#13 球面消费必要条件以独立反例写进正式科学正文。

**原191篇审计范围声明**：本批是现行正式专题与生产代码的定向内容精读，**没有重新取出 `63eccbb6` 对应全部 191 原 blob 并逐篇全文对照**；不将上述专题自动记为“原文全文无损复核完成”，`ORIGINAL_FILES.tsv` 未给未读篇章虚增状态。既有已验证原文件/来源统计沿用前批报告；191篇余下原文、其他 246 实验文本以及高压缩四文后续逐项跨层核对仍未完结。结构、链接或 Git blob 归档正确不等于有效设计完备。

**未运行/待裁决**：真实 Gaia/XPSD 输入及算法二进制复测、Nside=512/2048 的 profiling、v2 reader/writer/发布和 P2/P3 端到端、#8/#9 实际 Fisher/单位差异、MSVC C11/C++17 ABI loader/CLI E2E 均 NOT_RUN；没有更改生产 C++、JSON schema 或示例，未声称原 v1 产品已经修复。Agent 下一工程批次先实施 #10 的 **schema+writer+reader+产品验证+两下游消费** 同步切换；#12 的数学错误修补在独立批次按新规范实施；#74 先做取证，#75 先验证已有生成机制，再改接真实构建与二进制身份；ARS-02 保持 S0→同一 DLL 双 op 的已选 ABI。


### 原191篇实质语义补核：Gaia XPSD 模块原文第 1 篇（本批新增）

完整逐行复读 `63eccbb6133aef56389641d43c55df6e243819c5:docs/detail/infrastructure/gaia_xpsd_client.md`（147行、原清单 9390 B），对照当前压缩后的 44 行以及生产 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.{c,h}` 的缓存、树剪枝、create/查询接口关键段，补回现行 `docs/detail/infrastructure/gaia_xpsd_client.md §6`。恢复原文中仍有效的 TTL/字节/槽位及客户端解压缓存预算、锁序、复杂度、保守极冠 AE 条件、输入/输出与确定性试验；纠正原文的缓存 double“逐位比较”（实际上为数值 `==`）、只有 db_type/file_count 不能证明 dataset 内容身份、赤道经验 1.2 裕度未经零假阴性证明，以及性能测量未运行。该原文的有效信息已在正式模块设计有独立正文而非只留在 SOURCE。其它原191篇本批没有声称新增逐篇全文核对，下一批继续按真实 baseline 原文对照。


**#75 生成规则精确性补注（2026-10-17）**：再次核对现有 `gen_build_stamp.py::build_stamp/render_header/write_if_different`，发现已有 header 内容包含由 `datetime.now` 产生的 `stamp_utc`；若仅新增 always-run Ninja target 会每次构建改变 header 并触发重复编译。现将正式工程合同裁定为：always-run 比较四项身份，身份相同保持原 UTC/header 字节不变；身份变化才刷新生成时间和小 TU，采样检查时间另行记录。纯文档改 HEAD 的源码摘要不变但 build_head 必须更新；不造二进制 SHA。本批只更新文档，生产构建与增量矩阵仍 **NOT_RUN**。


## Issue #6 D5｜原 191 篇两篇长文的完整语义保全与当前生产对照（2026-10-17）

**任务唯一范围与证据**：本批完整逐行复读 `63eccbb6133aef56389641d43c55df6e243819c5:docs/science/algorithms/mosaic/PHASE2_UPM_IMPL.md`（**580行、44844 B、Git blob `f9cb801d4dbef42da8caa96bc6c65dc07a29986b`**）和 `63eccbb6:docs/science/algorithms/mosaic/PHASE2_MOSAIC_WRITE.md`（**524行、40606 B、blob `ade52b4f0f307c7503a29b224eb04c7bd81c7483`**）。原文条目已有 `ORIGINAL_FILES.tsv`；此批只将这**两行**实际状态标为全文语义保全/重写，不将其他 189 篇、实验集合或已有部分阅读统计伪增为全部完成。审查生产冻结 HEAD 为 `322da061bd9f46cd001a5a45c0f226a05b57777b`，对照 `docs/science/sky/UPM.md`、`docs/science/integration/INTEGRATION.md`、`docs/science/noise_snr/NOISE_SNR.md`、`docs/detail/mosaic/modules.md`、`docs/detail/export/modules.md` 与大文件 `module_adapters.cpp` 定点函数（upm-fit/apply/write/p3_properties/p3_writer）、`coverage/src/{sky_plane,upm}.cpp` 的模型/接口，以及 `sky_plane.h`。源码为定点函数和相关头/推导对照，**未宣称逐行审完整 17565 行 module_adapters**。

**原文一 UPM 已恢复且可独立实施的有效部分**：
- 原 F1–F6 的背景 control 方差/资格与原 `quality_factor×control_ivar`、仅作消融的 SNR 权重、局部归一、Huber/M+C/CG、centered 8×8/tile 内插值、frame-control 图与逐分量锚、模型序列化/dense stale，明确标为**退役旧 `upm.h/upm.cpp`**（非生产）；另完整保留旧 convergence 的 `tolerance/tolerance_relative`、`converged=0/1/2/3`、stalled 与 gs_damping 等，独立复现实验，不让其覆盖现行参数。
- 现生产 `module_adapters.cpp::p2_op_upm_fit`（11309）→ `p2_sky_plane_build_joint`（11542）→ `p2_sky_plane.bin`（11617）；`p2_op_upm_apply`（11769）→ `p2_sky_plane_open/eval_delta_block`；`sky_plane.cpp` 的 `B_ref + delta_poly + F_k`、Schur `H_red`、Huber IRLS、列均衡有效秩、按真实控制构图逐分量 gauge/anchor、逐帧 own 的同资格估计与去公共模、效应 hash（#77/#78 当前已有静态修补）、错误码、单位、矩阵诊断和外推限制均写进算法及模块 API。**库默认 `max_nodes=2048` 与 adapter 默认覆写 `8192` 两者区别明确**。完整 C ABI 函数族、输入输出 POD、寿命、出错状态及测试入口在 `docs/detail/mosaic/modules.md`，不会要求执行者回看旧 source 才能施工。
- P5 #76 是 `g·H S+a+noise` **残余乘加**的研究问题，P1 增益已施加后不重复施加；公共加性/乘性 gauge 不可识别方向、Jacobian 秩、同响应与异响应边界、真实背景 shot noise、模型误差/交叉协方差、负例 `g=1` 与 E2 负责人目检保留 OPEN。原文历史“去天光不影响 SNR、拟合平滑即完全消缝、乘性联合已上线”的过强表述不进入当前正本。

**原文二 Writer 已恢复且可独立实施的有效部分**：
- ALG-P2-HIPS-001..004 有效的 FITS/HEALPix/NESTED local↔FITS index、`K_leaf=K_tile+9`、面积/flux numerator/covered area、候选加权/状态/NaN、AIO staged/finalize/verify、运行资源估算、错误与独立 oracle 已按真实 `p2_op_write`（14461–15035）重新编排。**最终只有 `signal/` 一个 HiPS 科学图像子产品**；内部 `signal/support/wsum` 与 `uncertainty_available` 等仍用于计算和审计，但不能把旧 `signal+support+variance+ivar` 四科学层强制对外发布。`p2_final.products=["signal"]`、合法混合滤镜、不拒绝跨滤镜，旧 `coverage/tools/stage2.cpp`、`acsd-stage2` 命令、ACR fallback/老退出码/固定 microchunk 上界均明确标为历史实现而非当前 CLI。
- 名义 `obs_filter` 与有序 `response_distribution` 对每帧标签多重集对称，空标签参与种类计数；#11 的冻结真实 overlay **12/12 CLI + 185/185 signal tile 数组不变** 证明其当时标签层修复，**不**证明不同响应物理等价，也不自动证明其旧版四图层产物就是当前 D02 单 signal。实际 `p3_op_properties`（16055）与 `p3_op_writer`（16614）不消费 P2 响应；#79 已在真实 Export 成功时观察 `p3_props.json/FITS` 缺 filter/response。本批将 P2 response/QE/model/occurrence 身份、P3 props、FITS FILTER(sole known only) + 目标 `ACSDRESP/RSPHASH` + 外部响应 JSON/hash/readback 规定为唯一工程目标，**机器 Schema / FITS 关键字仍未实现**。
- 真实不同 `R_k(λ)` 的 `R_eff(λ,ω)` 依赖位置有效权重/PSF/rejection，`response_distribution` 只有 nominal label count；用户自由混合，但不能据首帧 filter 宣称一个统一物理 bandpass。最终只发布 signal，不制造方差图，也不从较小 seam RMS 声称科学完成。

**Issue #80 的观察与未裁决**：在 #11 真实 2×Halpha600s +1×Red300s 输入中 coverage/sample 产生 116 union cells、17237 observations、5807 overlap controls，`p2_sky_plane_build_joint` 报 `reduced normal matrix not SPD`，无 `p2_sky_plane.bin`、apply 中止/CLI exit2；已写成 `M_k/H_red/H_solve` 三层矩阵的对称性、最小 pivot、列均衡谱/秩/条件、组件 gauge/响应/控制权重/FP64 Schur cancellation 的**可执行取证方案**。原因保留 `ROOT_CAUSE_UNKNOWN`；本批没有在最新 main 重跑此真实构型，更未“加 ridge 判绿”或将异响应本身认定为根因。

**真实代码与失败实验的对应证据**：`sky_plane.cpp::{p2_sky_plane_build_joint,sky_make_residual_fields,p2_sky_plane_save,chol_spd,p5_component_graph}`、`sky_plane.h` 的 API enum/布局；`module_adapters.cpp::{p2_op_upm_fit,p2_op_upm_apply,p2_op_write,p3_op_properties,p3_op_writer}`；[Issue #11 真实报告](../../实验/issue11-passband-identity/evidence/REPORT.md)、[#79](https://github.com/fujiaze/Astro-CS-Database/issues/79)、[#80](https://github.com/fujiaze/Astro-CS-Database/issues/80)、[#76](https://github.com/fujiaze/Astro-CS-Database/issues/76) 和 P5 S07 阴性实验。#77/#78 已有源码层修正，但**本批未重跑**全套模型 hash、控制资格、数值矩阵与 E2。

**本批边界/验收状态**：仅修改正式 `docs/`、本 `REPORT.md` 和两行 `ORIGINAL_FILES.tsv`。**未修改生产 C++/机器 Schema/科学算法/测试**；未编译、未跑 CLI/Issue #80 profile、HST/真实 E2、P3 #79 回归、数值 Monte Carlo 或原191篇其余文章，均 `NOT_RUN`。本批的覆盖是这两篇**有效信息经当代正式算法与工程章节可独立执行**，不是全项目问题闭合。

**下一批 D6 的精确范围（只处理原始两篇）**：`63eccbb6:docs/science/algorithms/mosaic/PHASE2_SAMPLER.md` 与 `PHASE2_REJECTION.md` 原文全文，对照 `coverage/src/sampler.cpp,rejection.cpp` 及公共头、`module_adapters.cpp::p2_op_sample/p2_op_reject` 和正式 SAMPLE/REJECTION/NOISE_SNR 分册。重点是背景 control 统计量 `median/MAD/k_corr` 及 mask/污染/有效域/控制精度、噪声与相关传播、Gaia halo/PSF/SNR 来源、重复/退役对象、排异六态/预筛选/样本数/回归/权重的条件选择、代码形参与内存/CPU预算/负例；直接修正式文件、已有文档报告与清单，不改算法，不扩大到其它长卷。**D5 完成后停止，D6 不在本批执行。**


## Issue #6 D6 — Sampler / Rejection 两篇原始长卷全文语义保全与当前生产复核（2026-10-17）

**批次冻结**：本批只处理原 63eccbb6133aef56389641d43c55df6e243819c5 的 docs/science/algorithms/mosaic/PHASE2_SAMPLER.md（**1004行、77933 B，blob c45afdeb67ae2a3dac4a810e18a04f239f5d8aa6**）及 PHASE2_REJECTION.md（**839行、64329 B，blob daae10b08edf33765afea8f724318f08195e330d**）。两篇现行稿与原文几乎等长，原内容中已有的详细数值公式和参考未被再次压缩；新增实质 SCIENCE_TARGET 与 CURRENT 实现差别，并对几处错误物理单位及异常状态断言就地更正。检索/对照基线是原先远端 main f0f28f5f28a1ef00a548b85031c85a07dd9bd9e6；实际最终 commit 在 Issue #6 评论登记。原191条清单只把这两行由“局部查阅或未精读”改为“D6全文精读/语义保全”，**不虚增其余189篇的原始精读数**。

**Sampler 保全与纠偏**：原1004行中的 background-clean Stage A/B/C/D/E、全 union 几何/HEALPix FITS↔NESTED、median/MAD 单侧3σ3轮剪裁、污染和邻域门、偶数中位数、N_ret retained、有限N渐近方差的独立MC记录、k_corr 矩阵的实验来源、空样本/零 MAD、Gaia halo 半径和回退式、P2ControlNode/P2ControlObservation/P2SampleStats、内容稳定 frame_id、1与N workers与 TEST-P2-SMP 的9类正反例均在正式原算法正文保留并补入当代实现解释。主科学纠偏为 **value/sigma/uncertainty 是真实已施加 P1 的 L/sr，control_variance=(L/sr)^2 和 control_ivar 反量纲**；旧文固定 ADU/sr、把三种不同估计量 BUNIT 并列当 signal 许可、旧 upm.cpp 数值同量级就“证明单位”的主张不成立，#9 未施工。Drizzle k_corr 的当前源码有 pixfrac 0.5/0.8/1.0×scale 300/600 arcsec/pixel 表（域外回退1.4，pixfrac 域内 clamp），真实小像素尺度几乎总在表外；这不是独立精度证书。**源码仍以1e−12替代零 MAD 的尺度计算伪 cvar/ivar**，不能把旧文理想“零精度不可用”写成生产已修。源代码在 sampler.cpp 的 simple连通 veto/Gaia halo veto 目前以 #if 0 注释；p2_op_sample 的 Gaia query 也 #if 0，以 source=disabled-halo-falsified 和空帽通过。SNR catalogue veto 仍启用；旧“全部 Gaia/简单星掩膜生产已上线”与“所有接口完全废弃”两个极端均不正确。原来的 pass3 accepted/rejected 重复计数点已有源注释修补，但未全链重测；sky-plane 的单帧 clean sky_samples 与仅 overlap P2ControlObservation 已明确分开。新增对 PSF halo、星云低频、相关/非高斯观测、单位和源信号损失的目标 oracle。实际相关问题 #8/#9/#13/#17/#71/#78 OPEN 或未复测。

**Rejection 保全与纠偏**：原839行 F1–F14、10显式核（sigma/winsorized/averaged/linear_fit/ESD/RCR/percentile/median_sigma/minmax/extreme_prior）、NONE 与 AUTO 性质、逐方法合法域、typed 参数、RCR/ESD/NIST/Siril次生对照、四reason/八status、all rejected/小栈/归一化、strided gather、src_idx映射、large_scale与移动源分类、复杂度/测试和真实 FPR 历史读数均保留。按最新 module_adapters.cpp::p2_op_reject 核实，**当前生产在每像素原始 support>0 的 geometric N** 上按 acsd_adaptive_pixel AUTO 规划：N<6→percentile，N>=6→winsorized_sigma；eligible N≤3 是欠定全接受，非 N>15 linear_fit 的 WBPP 旧对照档。**真实 production large_scale.enabled=1（8像素组件、低/高各2 px grow），库默认0**；独立分类 API 没被 adapter 调用。gather 当前只传 FP64 corrected 值，weights/support/valid/quality/frame_ids 在 gather 都是 null，之前对全部资格源的声称过强。**关键状态消费风险**：p2_reject_stack_ex 返回 rc=0 可能带 INVALID_* status/reasons=UNDERDETERMINED，生产 adapter 目前只检查 rc、不检查 dec.status；当前 valid AUTO/finite gather 下未证实触发，必须用未来错误注入才能裁决。排异后的条件化方差、相关噪声、不同滤镜/PSF、真实移动源与卫星线分离和低频星云保护要求独立真值；不能以 n_kept/reject ratio/seam RMS 推论无偏/SNR提高（#20/#21）。

**正式实施载体**：原 docs/science/algorithms/mosaic 两篇正文全文保全 + 新 D6 CURRENT/SCIENCE_TARGET/RESEARCH_OPEN 段；docs/detail/mosaic/modules.md 新 D6 补全部实参函数族、重要 POD 字段/配置、真实 adapter/与库默认差异、权重对象、所有权、容量、CPU/AIO/工作集与错误；docs/science/integration/REJECTION.md、docs/science/noise_snr/NOISE_SNR.md 补科学正本边界；Mosaic README 增两篇入口。已复核这些定点函数及上述两篇原文各章节，**不声称逐行审完全部 module_adapters.cpp**。GitHub repository 出口仅 docs 的 Markdown、现有 REPORT 和 ORIGINAL_FILES.tsv **两行**；没有修改生产C++、机器Schema、算法阈值/参数或新文档治理台账。

**验证事实**：原文有 NIST、Siril、M42、Monte Carlo 的历史报告；本 D6 的新 E2E/MC/M42/HST/Gaia、跨平台 ABI/编译、Windows 构建、真实 rejection 误拒/后选择协方差、完整 Markdown 渲染科学和代码回归均 **NOT_RUN**。GitHub links/原清单两行变更及远端 diff 范围单独做静态检查；无法以读源证明新的科学统计正确性。#8/#9/#13/#17/#20/#21/#71 等保持开放研究/实施分界。

**下一批 D7 不在本批执行**：按 Issue #6 规划先逐篇全文精读 **docs/science/algorithms/export/PHASE3_PROJ_IMPL.md 和 DRIZZLE_GEOMETRY.md**（以 Git tree 中实际路径核对）；随后每批最多2篇处理 PHASE3_RSMP_IMPL.md、PHASE3_FITS_IMPL.md，再 PHASE3_RESAMPLE.md、HIPS_WRITER.md；总队列聚焦 #79 response/provenance、#15 曲边光通/面积守恒、Export FITS/WCS/BUNIT 与阶段二单Signal。D8再查 API、detail 和实验文本。D6 完成后立即停止，不提前施工 D7。


## Issue #6 D7-A：原两篇长文全文审读与真实 P1 曲边 / P3 投影分界（2026-10-17）

**起点**：远端 main d35bfc501d1cf257eb9267a571f1061c0c1fe27a。原始基线63eccbb6133aef56389641d43c55df6e243819c5；PHASE3_PROJ_IMPL.md 原724行/55288B，blob 5a0bb9fd778348fbea5203bad4c38ed7740cce77；DRIZZLE_GEOMETRY.md 原469行/44423B，blob a432fd9915e44994a5450ff2f9dac5ddac9d7c3d。两篇全部章节已全文阅读，按最新源核准后直接在原当前文档保留全部有效公式、算法分支、接口、错误、实验参考与关键数据，并增加 D7-A 正式 CURRENT / SCIENCE_TARGET / RESEARCH_OPEN 规范。仅精读本批两篇，不将其他 Export 长文虚记完成。

**Phase3 WCS 的真实责任**：源 p3_wcs.h(374行)、p3_wcs.cpp(594行)、p3_projection_registry.h(312行)、p3_session.cpp(355行)、module_adapters.cpp p3_op_properties/wcs/resample/writer/verify、p3_resample.h/cpp、p3_output.h。原 G1/G2 中 CRPIX(1-based)/零基像元中心唯一+1桥接，ICRS CRVAL、CD-only手性（east_left/right 均 detCD<0）、rotation、TAN正反映射、RA wrap、裁剪、四角和密点适用域测试保全。关键状态：设计冻结八投影；v6内部核四种；产品声明与生产只有TAN，非声明码显式UNSUPPORTED。库支持 rotation_pa 非零，但真实 p3_op_wcs 将 rotation_pa_deg=0.0，不冒充CLI有旋转选项。pix2world 中切平面半径 r≥π/2 拒绝是额外实现限制而非真正TAN天球前半球边界；四角像元中心足够涵盖中心矩形，但不能证明±0.5像元外边界。crop精确调整CRPIX/画幅不重新采样；真实CURRENT P3 writer仅PRIMARY Signal，uncertainty unavailable。#79真实生产 props 不携 nominal response，mono/unknown/mixed FITS FILTER/hash均只是目标合同；#9 applied L/sr 与canonical BUNIT一致性未闭合。

**Stage1 HEALPix Drizzle 的真实责任**：原 DRIZZLE_GEOMETRY 虽在 export/，实际是 P1 Normalize 的 forward球面面积交叠，源 drizzle_engine.cpp、spherical_overlap.cpp、hp_drizzle_api.h、astro_sphere_sink.cpp、aio_hips_writer.cpp。保留原 drop/pixfrac、a_jp、A_drop/A_pixel、F/N/D、sumVarNum、auto-nside、SIP、3层候选、1/N确定性、非有限分类和九项DISP与HST/M42设计。核实 #15：nside≥256 真生产 cached/uncached 路径均用4角测地线叶替代真实HEALPix曲边；极冠apex 约−9.968368384%是特定叶区域面积误差，不是已测FITS或星点 flux 误差。分清自分区闭合、常数面亮度、真实leaf membership、曲边真实 overlap 和天空信号保真。就地订正原无条件 S_p=ΣB_ja_jp/Σa_jp：正确代码表达为 S=ΣB_j a_jp(A_pixel,j/A_drop,j)/Σa_jp(A_pixel,j/A_drop,j)，只有面积比对所有有效 j 一致才可约掉。不得替换 sumNorm=N_p 为 sumArea=D_p。唯一 SCIENCE_TARGET：HEALPix face/chart参数真曲边、面积Jacobian和自适应求积/误差证书，以及独立 membership oracle；在证明范围内保留快四角，否则精细曲边路径。实验 nside256/8192/2^18、12faces、极冠/|z|=2/3/RA wrap、pixfrac.5/.8/1、不同drop尺寸、PSF/星点孔径/低频/M42/HST、输入像元相关噪声分组，绝不把P3 bilinear当P1精确leaf overlap。

**D6后代码新修事实**：#81 生产 large_scale 已用 eligible_before_growth 裁剪ring，#84 zero candidate 初始 acc=0；#89 p3 bilinear_nanmask_ex 已初始化weights[4]为0，#88 p3_uncertainty_propagate 已按完整leaf_ipix归并重复角系数再传播条件方差。四项均 STATIC_SOURCE_CONFIRMED，不依据旧D6描述回退，也不声称已真实FITS验收或消除完整covariance。Phase3 slim仍不发布方差层。

**正式修改范围**：两篇原算法长文、docs/science/drizzle/DRIZZLE.md、docs/science/projection/HIPS_TO_FITS.md、docs/detail/export/modules.md、export/README.md、当前REPORT、ORIGINAL_FILES.tsv严格两行；不新增治理文档，不改生产C++/schema/科学参数/test。静态源审/原文保全/路径/链接/最终diff/远端SHA可复核；外部WCSLIB/Astropy、独立曲边积分MC、真实M42/HST、编译/CPU-RSS/E2E全部NOT_RUN。#15/#79/#9/#14/#21未闭合。

**建议D7-B仅两篇原文**：docs/science/algorithms/export/PHASE3_RSMP_IMPL.md 和 PHASE3_FITS_IMPL.md；核实际Phase3 resample/output及#88/#89与#79和D7-A WCS的完整单位、FITS、流式/裁剪、错误、验证，不提前执行。
