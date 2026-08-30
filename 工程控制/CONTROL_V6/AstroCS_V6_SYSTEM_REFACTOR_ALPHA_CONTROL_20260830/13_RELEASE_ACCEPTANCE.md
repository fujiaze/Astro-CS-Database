# `0.10.0-alpha.1` 发布验收矩阵

## 1. 可发布定义

执行 Agent最多给出 `READY_FOR_OWNER_REVIEW`。以下全部通过且负责人完成最终视觉审核后，才是 `ALPHA_RELEASE_APPROVED`。

## 2. 硬门

| Gate ID | 验收项 | 自动证据 | 失败条件 |
|---|---|---|---|
| R-SCI-01 | Phase1 SCI→ALG→DATA/API→source→TEST | traceability matrix | 任一断链或公式冲突 |
| R-SCI-02 | Phase2 UPM/rejection/integration 科学闭环 | synthetic reports + contracts | 接缝靠未定义校正；权重混淆 |
| R-SCI-03 | Phase3 正式科学设计与实现 | WCS/unit/oracle reports | prototype/硬编码单位/串行生产 |
| R-ARCH-01 | 唯一 Pipeline Runtime | build/call/link graph | 第二生产 scheduler 可达 |
| R-ARCH-02 | 类型化 Artifact 连续传递 | IR/trace comparison | phase之间重新猜路径或ID断裂 |
| R-ARCH-03 | I/O 只负责 I/O/Artifact | dependency checker | I/O内含stage调度 |
| R-ARCH-04 | 模块注册与合同完整 | registry checker | 未注册/重复/缺端口合同 |
| R-CPU-01 | ACR 不在生产依赖 | CMake/link/symbol/runtime list | 任一生产 ACR 符号/模块 |
| R-CPU-02 | baseline/AVX provider正确选择 | benchmark/profile reports | 硬编码ISA；不支持路径加载 |
| R-CPU-03 | 无单线程重计算 | serial checker + resource reports | workers=1 或active workers<2 |
| R-CPU-04 | 资源利用达标 | monitor gate | 低利用未解释或锁/不均衡 |
| R-IO-01 | 图像所有权正确 | ASan/LSan | leak/UAF/double free |
| R-IO-02 | CFITSIO并发安全 | doctor/stress/TSan | 非reentrant生产并发；共享handle |
| R-DOC-01 | L0负责人文档完整 | doc checker | 无法从REVIEW判断状态/风险 |
| R-DOC-02 | API与代码一致 | Clang AST diff | 导出签名漂移 |
| R-DOC-03 | Pipeline文档与运行一致 | static/trace diff | 文档节点未执行/隐藏节点 |
| R-QA-01 | 编译警告/静态分析 | GCC/Clang/MSVC reports | P0/P1或blanket suppression |
| R-QA-02 | Sanitizers/线程安全 | ASan/UBSan/LSan/TSan | 未解决错误 |
| R-PLAT-01 | Linux amd64 | clean build + synthetic + smoke | 任一必需项失败 |
| R-PLAT-02 | Windows amd64 | MSVC + synthetic + benchmark + smoke | 任一必需项失败 |
| R-REAL-01 | 银心32R最终全链 | manifest/trace/artifact reports | 少帧、旧产物、断链、运行失败 |
| R-REAL-02 | 接缝/黑洞/条纹/排异 | metrics + locked previews | 数值门失败或负责人拒绝 |
| R-REAL-03 | Phase3 FITS | WCS/header/coverage verify | 单位/WCS/coverage错误 |
| R-PKG-01 | 双平台发布包 | install smoke/checksums/SBOM | 多入口、缺baseline、含脏文件 |
| R-AUD-01 | 审核包 | validate_audit PASS | commit混杂、大文件、历史/数据 |

## 3. 接缝专项门

接缝曾经修复后回退，不能只用主观预览。最终至少报告：

- 每个 overlap 的校正前后 median difference、robust RMS、低阶 gradient residual；
- overlap 两侧点源 flux ratio 和扩展结构差异；
- zero-support/low-support 像素数量与空间分布；
- black-hole/stripe detector 的 synthetic recall 和真实候选清单；
- UPM surface 幅度、平滑度、gauge 和每连通分量状态；
- 同一锁定 stretch 预览。

阈值由 SCI-002/TEST-001 在实现前冻结。不得在看完 32R 后调阈值让结果通过。

## 4. 发布包布局

Windows：`astrocs.exe`、baseline/AVX provider DLL、必需 runtime、default pipelines/schemas、README/licenses/SBOM/checksums。  
Linux：`astrocs`、baseline/AVX provider so、default pipelines/schemas、README/licenses/SBOM/checksums。

用户只操作一个 `astrocs` 入口。内部 DLL/so 不等于多个产品 CLI。

## 5. 已实现/未实现声明

Release status 必须逐 Phase 写：IMPLEMENTED/EXPERIMENTAL/NOT_IMPLEMENTED。Phase3 只有完成 G6 与平台验证才能 IMPLEMENTED；ACR 明确 DORMANT_NOT_IN_PRODUCTION；GUI 明确 NOT_INCLUDED。

## 6. 最终抽查

负责人抽查建议由 Agent准备以下最小材料：

1. SCI-P2-UPM 与 seam synthetic 报告；
2. Phase1/2/3 静态图与同 run trace；
3. 一个真实 public API 的 AST→API→module→test 链；
4. Phase2/Phase3 resource timeline；
5. AIO leak修复证据；
6. 32R locked viewer views；
7. 发布包从 clean directory 的一次命令运行。

抽查失败则对应 Gate 退回 FAIL，不能只改 L0 文档。
