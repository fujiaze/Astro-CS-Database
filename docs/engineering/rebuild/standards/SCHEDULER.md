# 调度、资源准入与管道进展实施算法

本篇是EXECUTION的详细附件，规定首期CPU调度器实际采用的算法。不能在各模块分别选轮转、各建池或用无限队列规避资源规划。公共字段/签名见[FOUNDATION_API](FOUNDATION_API.md)，硬件分层仍唯一见SYSTEM。GPU只沿同一资源向量扩展，本期不实现。

## 固定配置与队列

| 配置 | 首期值/校验 |
|---|---|
| P | 用户threads与CPU quota、硬件可用线程数的最小值；必须≥1 |
| command.weight | 整数1..16，默认1；显式执行配置，不进科学hash |
| compute ready window | 全进程最多2P个已就绪队列项；其余保存在已计费的有限计划窗口，不创建无界future |
| active read | 最多min(P,4)个物理read；每请求原始/压缩缓冲均已预留 |
| active write | 最多2个物理write；不含纯metadata原子提交，后者仍受唯一写队列约束 |
| I/O chunk | ≤8MiB，且≤该stage已预留buffer；允许小于规范1MiB数组块 |
| bulk cache | ≤H/8，当前不可逐出pin及各模块存活模型也计H；保底arena不借给cache |
| control metadata | 每plan显式control_bytes及ScopeLimits.metadata_bytes；包括Task/continuation/取消节点/管道项/descriptor请求副本 |

每个command分别维护ready_compute、ready_io、waiting_dependencies、waiting_resources四个逻辑集合；底层可共享节点存储，但不得把等待磁盘的队首挡住ready计算。所有等待项都已有节点额度，唤醒不依赖临时new成功。终态节点在所有观察/清理引用释放后回收。只有有限窗口的cohort可展开Task，未展开部分以范围游标保存。

## 资源账本的原子操作

账本按scope树记录held、allocated、peak_allocated、work_consumed、work_reserved。host含pinned子集，device/spill独立。每个原子事务先计算所有受影响祖先的增量并检查溢出/上限，再一次写入；失败不得部分占有。首期可用一个短账本mutex，锁内不执行allocator、用户回调、hash或I/O。

```text
reserve(scope, vector):
  check vector合法且pinned<=host；检查所有祖先held+vector<=ceiling
  若任一失败，返回resource_exhausted，不改账
  否则各祖先held+=vector，生成后台绑定arena
claim(arena, vector):
  check arena open且available>=vector；失败不改账
  available-=vector；claimed+=vector，生成容量票据；不增physical peak
commit(ticket, actual_allocation):
  allocator成功后claimed-=vector；allocated+=vector，更新祖先peak
release(ticket):
  committed票据先等allocator/实际I/O释放，再allocated-=vector
  未commit票据只claimed-=vector；两种状态互斥且只能转换一次
  arena仍open则available+=vector，否则祖先held-=vector
close(arena):
  原子标closed，祖先held-=尚未claim的available，available=0
```

容量票据状态严格为claimed→committed→released，或claimed→released。allocate失败/未commit票据最后释放时减claimed，不减allocated；按arena open回available，closed则退祖先held。实际allocator成功后commit把claimed转allocated，即使arena已close也必须接受此已准入票据，不另申请额度；已close只阻止新claim。committed释放先物理free，再减allocated并按同样规则归还。每个状态转换用后台CAS，重复回调不双减，物理分配失败不得更新peak_allocated。

共享Lease复制不新claim。物理同源块的读cache命中归原创建域，调用者新增copy/解压才归新域。物理峰值在成功commit时更新；release减少当前allocated但不减少历史peak，claim/失败分配不增物理peak，不做进程累计peak差分。scope进入stopping只禁止新science；预留control/cleanup仍按FOUNDATION_API执行，到排空才closed。scope关闭，其存活输出仍归该scope，直至最后lease释放；不会把峰值转到下一次调用。

TaskSpec.work_upper是本次执行段可消费的上限：准入时原子增加所有祖先work_reserved，要求consumed+reserved+upper≤max_work；执行段通过其task子scope的consume_work把reserved逐项转为consumed，不能重复扣父预算。结束归还未消费reserved，实际consumed永不归还。工作量无法一次有界时拆成下个执行段重新准入；已声明上限不足返回resource_exhausted，不在运行中越过承诺。工作flow/模块helper不重置祖先work。

## 从计划算出可保证进展的容量

首期采用**单cohort保底执行线＋有限弹性窗口**。cohort是一次有界输入范围及其全部必需分支/join，不是一个OS线程。全局photometry/sky分解等不能独立逐块完成的阶段为barrier region，plan必须显式给完整参数/求解工作集；不能假装普通map来隐藏K²或共享状态。

1. 按region DAG拓扑拆开计划，对每个region分别执行以下步骤。stream贡献先提交R中的ordered_accumulator即算该cohort到sink，下游barrier无需同步接收它；barrier等全部cohort/上游EOF才ready，其F届时才借。校验region内stage/edge有限且无环；用stage.id最小优先的拓扑序。反馈环必须由工作流分成新epoch，不能作为pipe环接受。
2. 每个stage给每cohort的scratch/output_upper、控制字节、必要输入buffer和work_upper。edge给max_block_bytes、可共享allocation group及输出pipe边界。未知上界不进入调度，先缩输入域或返回unsupported/resource_exhausted。
3. 在元数据上模拟一个cohort按该拓扑序串行执行：当前live集合记录每项allocation group及尚未消费的必需边数。执行stage时同时计live输入、新input加载、scratch、output和I/O buffer；stage结束释放scratch，输入最后消费才从live移除，输出加入live。
4. 分支共享同一allocation group只计一次物理容量，但每条edge各占逻辑slot/bytes信用。没有显式共享证明时按不同allocation保守相加；这是上界预留，不是在实际账本重复计算同一物理allocation。
5. 对每一种内存domain取所有模拟步骤的最大值，再加该region控制节点/codec工作集。结果F是**本算法保底执行的峰值上界**，不是数学上最小RAM承诺。持久输出/最终模型/索引清单预算另为R，不得藏在F外。
6. 原子reserve(R+F)后，给该region唯一progress lane。F不能被bulk任务/cache借走；其空间按模拟的生存期复用，不为每stage再分别reserve一遍F。CPU/I/O token仅在执行段借用，不长期保在F中。
7. 剩余额度E可给其它cohort/预取/高key计算，且必须在启动整个额外cohort前保守预留其最大附加live集，不能只按第一块启动。E不足就只走progress lane，正确性/科学分支不变。

Host显式调用Operation.bounds及plan(ctx,input,PlanOptions)；初始拟定block_elements取bounds.max_block_elements，若R+F超限，按固定顺序降弹性window到1→逐出未pin cache→block_elements向下减半直到process.min_block_elements→输出/边转spill并以新PlanOptions重新调用plan→仍不够则resource_exhausted。只调整执行块/存储位置，不改求解模型、容差、秩或选择规则。R中的最终metadata自身过大也必须拒绝。

region的progress cohort始终是尚未完成的最小Key。该lane所需的输入、分支产物、join和sink输出buffer都在F中；入队时同时为其每条必需edge保一个slot及max_block_bytes信用。高级key不能借走这些保底信用。下一cohort只能在当前cohort的必需输出已被下游接收/写入、对应F空间可复用时接任保底lane。持久结果从F转到R时原子转交已有claim，不重新收费；R不够则封存前失败，不把F偷留给无界历史结果。

## 有序pipe、广播和join的执行步骤

```text
producer展开一个key前:
  原子核对本key的所有必需输出edge发布信用
  取得整组permit；progress key优先使用保底信用
  再按计划claim本执行段的input/output/scratch，之后才dispatch
producer完成:
  使用permit提交全部输出；共享块只复制Lease，不复制字节
  释放CPU token；输出仍在pipe/消费者/真实I/O中则保claim
consumer可运行:
  检查其所有join输入的同cohort/key和版本均ready
  已具其output/scratch容量后，原子接收整组输入并归还队列信用
  按固定key归约；consume后释放自己的输入Lease
```

permit等待不持CPU token、不持部分分支credit。广播必须整组获得所有必需分支slot；一个分支慢会背压整组，不少发一个分支。join按同cohort匹配输入，不能把两个不同模型revision或不同key拼在一起；失败输入产生显式终态唤醒join，禁止永等。

有序pipe的最低必需key始终有保留slot与bytes；max_items=1时只有它能取得permit，即使更高key的cache先命中。window>1时可在非保底信用上乱序完成，重排结果仍计E/metadata/credit，不能放无界side map。高key就绪但无permit只保有限计划描述，不先分配全部输出。unordered只放开投递顺序，不放开内存或最终科学reduce顺序。

一个process内部库函数可借同plan子arena，但不能调用Host另起一个未申报F的child process后阻塞等它。确需嵌套公开invoke时，child plan纳入父region模拟并借已分配子arena；未声明的嵌套invoke在run阶段invalid_request。模块函数直接组合是默认路线。

## 多command分配与公平性

CPU和I/O各用同一套**加权轮转发放token**：按command接纳序号循环，每轮给weight个dispatch credit，单执行段成本1；每轮未用credit不跨轮累积。只有ready且资源可行的command参与，单command同时运行不得超自己的cpu/io ceiling。公平性在**每个完整可行轮**上判定，而非用全局累计 dispatch 比率代替局部约束；当一条 command 尚未 ready/无法获得其额度时不得空转强等它。持久等待I/O不耗CPU dispatch credit；CPU压缩/hash/复制照常耗CPU credit，阻塞syscall只占I/O slot。

command内部优先当前progress cohort的可运行stage，次序为(Key,stage.id,task ordinal)，再发elastic任务。没有progress任务ready时可运行已获E的其它任务，不能占其保底内存/发布permit。该公平性按有界执行段次数，不声称不同耗时kernel得到精确相同CPU秒数；不可中断库调用必须在plan限制其规模，不能靠虚构抢占承诺硬实时。

新command的R+F以FIFO申请，当前持有输出确实不能释放时不强抢已有Lease。最老申请若可在已完成active region释放后满足，暂停向较新command增加E，等待这些region自然完成再给R+F；不得强制取消正常科学计算。请求大于其ceiling或全池物理上限立即拒绝；所有active region已结束、cache已清且仍因调用者保留结果而无法满足时返回resource_exhausted，不无限等待调用者释放自己手里的对象。等待者也计有界command/control配额。

诊断CPU/I/O只用科学ready队列发放后剩余token，受独立D和队列上限；科学持续繁忙可使诊断丢样，必须记录dropped。诊断不能占R/F/保底pipe信用或让science等待其flush。

## 异步状态与锁顺序

读流程固定：准备计费buffer→拿read slot→submit syscall→完成释放read slot→把解码/解压/hash排进compute→成功后发布Block。写流程固定：取计费Block→compute编码/压缩/hash→拿write slot→submit syscall→完成后更新writer偏移/释放Block。不得一边占CPU token一边等I/O slot，也不得在I/O worker执行不受CPU token约束的zstd/hash。

await把当前执行段转suspended并交回CPU token，完成事件只把预留continuation置ready。finalize/abort/cleanup的flush/unlink/hash也按上述计算/I/O阶段执行；最外层wait_external才可阻塞线程。pool锁、账本锁、pipe锁采用“读状态→解锁→尝试原子准入→按序提交”的步骤；没有任何锁跨用户函数、allocator或系统调用。取消按TaskId顺序标记并唤醒，不在锁内跑析构。

失败路径先停止该必需region的新提交，解除未消费permit，给所有下游失败终态，排空真实I/O/计算，关闭私有writer，最后close arena。一次释放只由claim后台CAS执行，重复回调不重复归还。允许的partial走finalize但保原Code/有效域；hard failure不seal成可用结果。

## 必须通过的实现反例

| case | 固定构型 | 必须观察 |
|---|---|---|
| S-FLOOR | 两stage一edge：不可再分的输入8、输出8、scratch8 bytes同时存活，sink消费后不保整块；控制/持久R另预留，额外payload F=24 bytes，再给23 | 24完成，23在启动前拒绝；实际峰值与held/allocated分开 |
| S-FORK | 一个8-byte只读块广播两分支后join，slot均1，一分支延迟 | 一份物理8 bytes、两份逻辑credit；join不早消费、不遗漏慢分支 |
| S-ORDER | key0磁盘慢，key1缓存快，slot=1，H仅F | key1不能占保底slot；key0到达后可推进，无无界重排 |
| S-MULTI | weight=1和2的两个持续ready command，单位执行段均1 | 每完整可行轮dispatch比例1:2，池/command tokens均不超限 |
| S-WORK | 两并发段各upper=6，父max_work=10 | 不能同时预留12；完成一段用4后退2，另段可重新准入，总实际≤10 |
| S-IO | P=1，延迟read与已ready计算、压缩/flush并发 | read等待时其它计算推进；压缩不超1 CPU；seal等真实write完成 |
| S-BARRIER | E=0、两个cohort局部贡献→一次全局Schur/barrier | c0提交已计R累加器即退F，c1可继续；全部EOF后barrier取得F执行，不等c1时占住c0唯一lane |
| S-LIFE | close arena后仍存一输出Lease、取消仍在途写 | 未claim立即退，输出/写buffer直到真正释放才退，不UAF/重复收费 |

这些是后续实现验收，不是本次文档已运行的证据。实现者必须记录plan F/R/E、每次准入/credit流转和峰值反例数据，才能确认调度器满足本篇；仅接口能编译或正常样例不挂起不够。

## S-MULTI 的证据口径与失败处理（2026-10-24 订正）

以权重 1:2、P=2 的固定 fixture 为例，一个可检轮开始前必须有 command1 至少 1 个 ready item、command2 至少 2 个 ready item，且这些 item 满足资源准入；对应连续三次 dispatch 的多重集须严格为 `{1,2,2}`。逐轮记录开始时 ready 状态、对应的 dispatch ID/command ID、额度和资源拒绝次数，保存全部可检序列以便独立复算。不得把只有部分 command ready 的轮计成违反权重，也不得将一个发生过违规的轮靠累计 1:2 抵消。

正式门禁同时要求（a）**安全/正确性**：完整可行轮的违规数为 0、每 command 不超自身并发 ceiling、不漏任务、不出现饥饿；（b）**观测充分性**：每次运行至少 100 个完整可行轮，连续 5 次运行每次都满足（a）和（b）。样本不足既不是证明公平性错误，也**不是通过证据**，应保留 `FAIL/INSUFFICIENT_COVERAGE` 的原始记录并诊断 producer 供给/并发窗口/调度行为，不削弱条件或挑一次绿的取代。窗口 P=1 时 `2P=2` 无法同时容纳 1+2 ready item，不能以该配置认证三次 dispatch 的 1:2 完整轮；仍需独立验证其可进展性。

早期冻结快照的五次中有一次仅74完整可行轮，因此该attempt保留FAIL。较新manifest在b592d88被测代码上记录multi01 42/42检查PASS，不再将早期失败作为当前结论。实际测试/fixture变化及新HEAD适用范围按IMPLEMENTATION-STATUS核对；本段规定样本/公平性标准，不手写第二份实时状态。

## 控制元数据、并发快照和跨模块进展

控制metadata同样必须先准入：plan给最大展开cohort数、每stage Task/continuation/cancel/pipe/permit节点数，包含std::function捕获、可变长ID/Issue和allocator对齐/容量。不能只按sizeof(Task)忽略堆内存；具体布局的上界由实现给出并用分配日志核验。重用节点池的物理容量只计一次，逻辑slot分别计；节点耗尽暂停展开，不在内存不足时依赖未预留new才能取消/唤醒。

scope/祖先账本的held/claimed/allocated/work_reserved和peak快照必须在同一事务时点读取；跨不同锁时点拼字段可能出现伪越额/漏计，不可拿它作资源正确证据。CPU时间以真实task执行段汇总，不取进程CPU差归某并发调用；不可取得就null。

最小进展保证的前提是：region内DAG有限无环、每段真实最大工作集已计F、最低Key的整组发布信用受保护、每个执行段有限且实际I/O最终完成或失败、cleanup有预留额度。沿拓扑选一个ready保底stage即可串行推进；分支不持部分permit等另一分支，sink交R/完成真实写后退F，stream到barrier通过已计R累加器解除跨cohort等待。这是条件性进展论证，不承诺磁盘永久不返回时的硬实时。

PIPE-CHAIN必须在同一Host真实串接三段科学模块、fork/join/有序sink和全局barrier，H=R+F、E=0、P=1/多线程、低Key磁盘慢和取消分别执行；再减到低于不可拆最小F应启动前拒绝。检查实际live allocation和信用流转，不仅看正常输入未挂起。不同region并行需要E追加额度，不能重复借唯一F。
