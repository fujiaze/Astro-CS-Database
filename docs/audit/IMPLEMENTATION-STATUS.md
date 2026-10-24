# ACSD v2 实施与验收状态正本

> 文档核查日期：2026-10-24；受检源代码基线：[`13740080d44a587947652b7544ba7ce5d0af7921`](https://github.com/fujiaze/Astro-CS-Database/commit/13740080d44a587947652b7544ba7ce5d0af7921)。本轮只修文档，未编译或重新运行生产C++测试。原始第二轮报告检查的是`a17b199c2e17...`；本页已核较新分支，不能把不同快照混作同一当前裁决。旧失败/报告保留，不回写成通过。

## 三种完成标准与状态

- 架构完备：固定框架、资源、API、数据边和维护责任清楚。
- 算法规格可复现：陌生开发者只凭公开规范/API即可确定估计量、步骤、参数、停止、状态和固定例。
- 实现验收：真实代码在冻结输入、独立Oracle和明确环境中符合规范。前两项不能替代这一项。

`DESIGN_REVIEWED`只描述已审范围；`IMPLEMENTED_NOT_VERIFIED`表示有代码但证据不足；`PASS_SCOPED`只认证具名冻结case的全部实际断言；`FAIL`是对应冻结运行有失败；`PENDING`包括未运行/未覆盖/依赖变动待复验；`HISTORICAL`保留旧SHA结论。新增`BLOCKED_CERTIFICATION`仅是本页的验收标签，表示存在已知数值包含阻碍，不能宣称相应严格证书通过，不是新增公开Code。

## 当前证据身份与Runtime快照

当前仓库的[runtime manifest](../../validation/runtime/manifest.json)记录16/16 case通过，包含[multi01的42/42检查](../../validation/runtime/multi01/comparison.json)，`git_commit=b592d88e01f9b6cfcaf32ef554ae6fc7a9c0197e`，生成时间2026-10-24T10:20:12Z。此前6f08c4e归档的15 pass/1 fail、multi01 39/41是历史失败，不能再称为“当前仍失败”。

从b592d88到本页受检13740080相差4个提交；本次比较可见PSF实现、验证harness/case/观测及文档改变，未见Runtime主体源代码变化。这保留了Runtime冻结PASS的有限价值，但**没有重新构建/执行新HEAD，也没有证明新fixture、当前规范或全部科学链通过**。M0—M4 smoke/资源断言更不能替共享interval、超越函数和全链科学数值认证背书。

| 范围 | 已有代码/证据 | 当前裁决和未覆盖边界 |
|---|---|---|
| 架构与公共接口 | 五份foundation、22 process、57 typed operation；runtime/model/modules头与registry已有实现 | 已有设计复核；逐算法合同本轮再补，不以登记数当生产支持数 |
| M0 | reg01冻结记录29/29 | PASS_SCOPED于该代码/fixture；复杂typed/codec负例及全部57路径分别核验 |
| M1 | life01 20/20、work01 16/16、a01 23/23 | PASS_SCOPED；控制节点动态内存、并发快照/CPU归因继续按新增反例核验 |
| M2 | floor/fork/order/barrier/multi/pipe六项冻结PASS | PASS_SCOPED；multi不再列当前FAIL，真实跨模块三段/分支/barrier低H组合仍PENDING |
| M3 | io/b01/storage01/d01_off/on冻结PASS | PASS_SCOPED；Dyadic typed读取、全部乘加溢出和新进程完整恢复未闭合 |
| M4 | cal01冻结41/41，另有校准故障证据 | PASS_SCOPED于实际断言；完整allocator/adapter故障与严格数值认证仍需复验 |
| 共享严格数值 | interval/linalg已有代码，本页受检版本仍有下表阻碍 | BLOCKED_CERTIFICATION；普通point输出/字段名/收敛不授予严格证书 |
| M5 | 多个typed process/科学实现与分散记录已存在 | 按operation/例子分别IMPLEMENTED_NOT_VERIFIED或PASS_SCOPED；本轮算法补全不提升生产状态 |
| M6 | 工作流/产品/入口代码与历史11case部分记录 | PENDING完整当前Normalize→publish→new-process open→Mosaic→Export、双平台、性能与GUI协议验收 |
| GPU | 只有扩展合同，本期CPU | 不属于本期实现；模拟Completion不等于设备验收 |

## 必须优先闭合的数值阻碍

下列是对受检源码及数学反例的核对，不是本轮重新执行C++得到的comparison。优先级针对严格证书，不意味着已证明所有产品污染。

| ID/等级 | 受检问题/反例 | 实施及关闭标准 |
|---|---|---|
| N-01 P0 | interval.cpp以down(a+b)接收已求值double；0.1/0.2按精确二进制输入的和可被错误下界漏掉 | 按NUMERICS在运算前指定舍入/显式MPFR，独立有理Oracle验证四则、抵消、次正规数与溢出 |
| N-02 P0 | sin/cos的k固定枚举-8..8；sin[1000,1002]漏极大1 | 全相位存在性包含/粗[-1,1]，N-PERIOD和真实WCS/geometry链复验 |
| N-03 P0 | 一般幂只交叉两个端点；[2,3]^[2,3]应包含[4,27] | 正底exp(Y log X)及整数/0域规则，N-POW与下游幂模型复验 |
| N-04 P0 | atan2四角极值未覆盖负实轴切线；x=[-2,-1],y=[-1,1]需含±π极限 | 定义域切分/主值闭包、原点拒绝，N-ANGLE及坐标链复验 |
| N-05 P1 | linalg的重建残差/正交缺陷/范数仍以常规浮点求，字段rank_certified不证明向外 | 独立向外重算残差/范数/输入半径和真实rank含义，多RHS证书AND，N-LINALG/上游score复验 |
| N-06 P1 | 一个interval_primitive_status不能证明全部超越/积分/矩阵/根能力 | 分族/定义域/环境记录依赖能力；有未验证原语就不发布依赖它的certified结果 |

修实现后先过原语及复合检查，再复验受影响的WCS/overlap/Drizzle/PSF/photometry路径；对已存在产品按source/recipe/算术身份追踪影响。没有经过分析的产物只标待追踪，不宣称全部已坏或全部安全。本轮没有修改任何上述生产源代码。

## 其它当前闭合项

| ID | 责任范围 | 任务与实际边界 |
|---|---|---|
| E-01 | 证据/状态 | 旧multi失败改列HISTORICAL，较新16/16按真实被测SHA记PASS_SCOPED；harness/规范改变时按依赖复验，不保两个相反当前结论 |
| E-02 | M5资源 | 所有vector/字符串、矩阵/排序、GMP/MPFR/codec workspace和控制节点计真实容量；低H、allocator日志和RES-META反例，不只sizeof(T) |
| E-03 | 产品/工作流 | 新进程full重开保共享根/unknown/精确端点，无原始FITS仍完整；真实文件/失败回滚/三命令/双平台/性能各自证据 |
| E-04 | probe | 每个process实际生产采样及单位/index，DEV_MODE OFF/ON科学等价和故障/丢样，不止登记目录 |
| E-05 | Storage | 当前prepare_read仍将itemsize=0的Dyadic路径unsupported，且cache路径有total*itemsize/begin*itemsize等直接乘法；按OBJECTS typed精确路径及ST-OVERFLOW落实 |
| E-06 | Task/Pipe | frontier已写规范且单项有PASS；补PIPE-CHAIN真实跨模块、多段fork/join/barrier、E=0、P=1及在途取消，不再笼统说“无接口” |
| E-07 | 数值/科学调用 | 欠定QR已有m<n具名拒绝；原5-source例重跑上层not_identifiable/unsolved及无假产品，不把旧length_error直接指控为当前事实 |
| E-08 | 响应/量纲 | 约5.54e20的photon数量级可由平坦谱解析式得到；按I-UNIT核实际标志/单位/转换，不能仅凭大数指控科学错误 |
| E-09 | 算法规格 | 本轮22篇补了各自步骤/参数/状态/反例，重点measure/认证积分/streaming CSR；按ALGORITHM_CONTRACTS陌生开发者复现和真实生产路径分别验收 |

## 证据随提交同步的可执行纪律

每份case/索引分别记录：tested_source_sha及实际构建的依赖内容摘要，test/fixture/oracle身份，spec/必需断言身份，命令、输入/期望hash、compiler/flags/依赖库、actual backend、attempts/successes/failures、observed/comparison及证据归档提交。evidence_commit可以在后续索引补链，不能要求证据文件预知自身commit或形成自hash循环。

后续实施在现有验证工具中加入可机检的一致性门：manifest状态/attempts与case比较文件逐项相合；required缺项或零有效样本不能PASS；被测源码依赖、fixture/oracle/断言/flags任一变化时，旧结论转历史/待复验；仅文档或证据归档变化且依赖摘要无变，可引用原有限结果，但需明示不是新运行。当前本轮没有修改验证脚本或生成产物，故该自动门仍属待实施要求。

汇总器据有效case依赖覆盖计算M0—M6，不手写第二份pass计数；本页只作人读导航，必须与manifest一致。规范语义改变也要标待按新要求验证，即使二进制相同；不能因为source hash相同就把新科学断言认证为已测。每个修复保留旧失败attempt、新运行与影响范围，禁止覆盖历史证据以凑通过。详见[第二轮整改记录](round2-algorithm-contracts.md)。
