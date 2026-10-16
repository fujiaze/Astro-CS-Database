# 存储形态实现与接线

字段、布局、哈希和兼容规则统一由[存储合同](../../engineering/contracts/HIPS_STORAGE_FORM.md)定义；本页连接实际 writer、parser、存储原语与消费者，避免第二套形态定义。产品发布见[原子合同](../../engineering/contracts/ATOMIC_PUBLISH.md)，读取见[AIO](aio.md)。

## 当前组件

lib/infrastructure/cli/parser.cpp 的 session_keys 识别 normalize 的 storage_form 和 mosaic 的 coverage_index，session_commands.h 提供字段说明；键识别并不证明 writer 或 coverage 节点已消费。eng/contracts/ledgers/dead_config_keys.json 仍记录这两项生产接线缺口，该登记是核对线索，完成状态须沿实际生产调用确认。

现有 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 写产品集与 FITS 裸瓦片；不能从合同默认 archive 推断它已经逐成员编码、生成产品索引或写归档清单。archive 打包、索引生产和随机 reader 必须分别找到真实入口、调用者和可复跑产物后才报告支持，不使用整包临时展开冒充索引随机访问。

Python lib/infrastructure/aio/io/hips_output_store.py 的 tree_hash 实现条目三元组排序/紧凑 JSON 摘要，和 scheduler 的带域前缀规范文件摘要不是相同协议。Python Store、C++ writer 与生产命令分别核对，不把某族 helper 存在解释成三命令全接线。

## 几何与清单

aio_hips_set_geometry(AioHipsProductSet*, const char*) 保存调用者传入的 JSON 字符串，非空时设置标志；发布组装 manifest 时直接写出。setter 本身不解析并证明 WCS 合法，合法 JSON、同帧身份、原点和系数完整性由调用边界检查并独立读取验证。未设置省略键。

产品 geometry、存储引用、运行清单与科学 sidecar 分别持有各自对象，不用朴素全文键搜索代替嵌套 JSON 解析。原源码仍有 provenance/geometry 文本搜索的兼容风险，按当前 reader 核对；本轮未更换 reader 或清单格式。

## 打洞真实路径

aio_sparse_punch.h 的 punch_all_zero_blocks(path,out,verify) 扫描字面零块、探测卷、调用系统打洞并记录前后摘要、大小、分配量与状态。输入路径和结果由调用方持有，verify=true 启用读回复算；原语不拥有产品级发布决定。Windows/Linux 分支分别实测，存在分支不能代替平台行为证据。

aio_hips_writer.cpp 的 publish_finish 顺序为校验、文件同步、打洞/读回、后续改名与目录同步。PUNCH_VERIFY_MISMATCH 删除暂存并失败；当前 PUNCH_NO_RELEASE 也删除并失败，其他非 OK 记警告继续。原 detail 却称 NO_RELEASE 跳过后照常发布，现以当前实现说明，是否符合优化不阻断目标登记 E47，不改代码。尤其 post_hash_failed 等 IO_ERROR 与普通“不支持优化”不能混称已证字节不变，仍需故障注入。

当前 writer 还可异步提交 publish_finish；调用返回与尾部持久化完成不是一个时刻，最终产品发布须等待任务结果，取消与析构也要保持句柄和路径寿命。线程池与环境配置的工程纪律单独核对，不能从本页机制图推断所有权已经正确。

## 目标归档链与验证

目标为私有 staging 写合法内容，编码完整 tar/zstd并同步，生成/核对外置索引与摘要，再发布整组可识别产品。读者先检查形态/索引与身份，查询块候选，再按压缩范围读取单帧和相应文件。每步错误保留原因，不把不可读产品当空天区。

验证保存真实产品、索引和命令，覆盖短读、错误偏移/长度、重复ipix、错误阶、丢成员/终止块、哈希失配、歧义形态、缺索引、发布中断和字节保持。覆盖量化只供调度参考，保守候选责任见 Q29。旧 compress-01 数字与任务步骤在[来源档案](../../../实验/documentation-refactor/docs/STORAGE_SOURCE.txt)按原身份保留；本轮未压缩产品、执行系统打洞或运行 reader。
