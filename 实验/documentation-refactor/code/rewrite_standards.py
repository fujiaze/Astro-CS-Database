"""Retain reviewed implementation standards and write consistent prose."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/standards/CODE.md': r'''# 代码标准

本标准将[系统架构](../architecture/ARCHITECTURE.md)落实为代码规则。科学定义由 science 拥有，模块接口由[公共 API](../api/PUBLIC_API.md)及 detail 说明，注释写法见[注释标准](COMMENT.md)。

## 接口与错误

生产 C++ 使用 C++17、RAII 和窄接口；Windows 正式工具链为 MSVC，Linux为GCC/Clang，具体配置见[构建节点](../build/BUILD_NODES.md)。公开头自包含，C ABI 使用稳定POD/句柄、版本握手及显式所有权；STL和异常不穿过外部ABI。

每个指针说明借用/拥有、长度、可空、别名和有效期。分配前检查尺寸与乘法溢出，错误路径释放所有已取得资源。模块返回Result或C状态，不自行退出；成功、可恢复科学状态和硬错误按[错误模型](ERROR_MODEL.md)解释，错误日志不能替代返回值。

配置从显式请求/运行上下文取得，科学对象不藏在可变全局状态。默认值、可选能力和替代路径有明确合同与来源；未知误差不填零，负线性信号与有限真零合法。正式写盘经AIO，完成性与持久化遵循[发布合同](../contracts/ATOMIC_PUBLISH.md)。

## 数值与资源

每个科学字段说明单位、坐标原点、归一、精度、有效域及下游用途。共享算法单一实现；独立oracle验证其语义。快路径有支持域、等价证据和域外明确处置，不能把静默切换不等价模型称fallback。

像元热点不逐像元分配、写文件、日志或取时钟。缓存有容量、身份、失效、线程和寿命合同；scratch归调用/线程/对象所有。模块使用Runtime租约，禁止私建长期池、读取全局科学配置或无限队列。外部等待有超时、取消与错误传播；数学求解停止条件不以超时替代。

## 命名、格式与兼容

产品显示名为ACSD，全称Astro Celestial Sphere Database。实际头目录、命名空间、符号、schema、模块ID、CLI键、构建target、环境变量和已发布产物名按兼容合同保持；显示名整理不顺手修改机器身份。历史实验、原始证据及它们的文件名保留原样，正式正文只保留仍有接口意义的版本标识。

类型采用PascalCase，函数snake_case，C++成员snake_case_；既有C ABI命名保持。include顺序为本模块、项目公共头、标准库。first-party按根.clang-format/.editorconfig维护，第三方不批量格式化。注释和正文的历史流水迁往过程记录，真实引用/ID迁移同步消费者，不能以机器字符串存在永久冻结旧文档。

## 编译器诊断

本项目源码不引入无解释的新告警，不用`/w`、`/W0`或全域屏蔽遮盖问题。MSVC对ISO C/POSIX名称的C4996咨询诊断可由项目配置的`_CRT_SECURE_NO_WARNINGS`、`_CRT_NONSTDC_NO_WARNINGS`处理；这不免除真实缓冲、编码和错误检查。

vendored头可在引入点用SYSTEM或push/pop窄隔离其诊断，说明来源与范围，不扩散到first-party。实际编译结果、平台差异和未解决告警随构建记录，文档存在不代表双平台已零告警。
''',
 'docs/engineering/standards/COMMENT.md': r'''# 源码注释

注释解释科学依据、不变量、所有权、线程安全和非显然性能选择。算法推导在科学正本，接口规则在公开头及[模块文档](../../detail/00_INDEX.md)，过程历史在实验和Git记录。

对公式说明变量、单位、适用域及专题链接；对保守几何说明允许多候选而禁止漏候选；对借用数组说明有效期、异步复制和释放方；对并发说明shared/thread-local/reduction及运算序；对数值常量说明数学来源或标定输入，不能只写“经验值”。

避免复述语句的“初始化变量”“遍历数组”“返回成功”。一般保留简短理由，长推导迁入正本并引用结论。临时workaround说明触发条件、实际限制和可追踪来源，不能以缺issue编号阻止必要修复。

任务编号、控制包命令、审核轮次和“以后Task再做”等过程叙事不作为现行算法说明。API/schema/科学模型版本有实际兼容意义时保留；实验、失败日志和封存证据保留当时标识。不要凭字面V/R前缀批量删除版本常量或代码。

修改代码时同步附近过时注释，检查其描述与真实调用一致。源码写“非阻塞”“完整”“线程安全”“已验证”等承诺必须有对应机制和适用域；未具备时明确说明。注释卫生检查只能辅助定位，不能用PASS标签代替科学、寿命或并发审阅。
''',
 'docs/engineering/standards/CONCURRENCY.md': r'''# 并发与执行资源

并发由阶段Runtime与统一预算管理，组件分工见[数据流](../architecture/DATA_FLOW.md)和[调度合同](../contracts/SCHEDULER.md)。模块不私建长期池，不读取另一套全局线程配置。

## 预算与状态

亲和性、平台配额和运行策略形成预算，实际授予租约约束本次计算。帧级与帧内并行共享预算，嵌套执行不得重复授予或超额；不能把“嵌套”一词一概禁止后又要求多轴并行。OpenMP设置在受控调用边界注入，不由算法任意改全局默认值。

每个并行入口说明parallel region、共享只读/可变状态、thread-local scratch、归约对象、运算序、并发调用与释放限制。共享cache和计数器通过明确锁、atomic或线程本地聚合，不能只在头注释写threadsafe。取消时停止派新工作，等待已借用对象的消费者完成再回收。

## 数值结果与确定性

固定输入、配置、种子、类型、浮点环境与归约次序，尽量保持重复执行确定性。跨worker、ISA和优化配置按事先声明的科学容差验证；独立任务按输入下标回填可固定结果对应，但不自动保证所有库和编译器下逐位相同。

科学正确性不由调度API名称决定。显式OpenMP/宿主executor是现有计算入口，future/async若使用也必须消费同一预算、说明寿命和异常；不能创建隐藏线程或无界任务链。实际允许实现以工程接口为准，不靠一个语法禁词代替预算核验。

## 异步I/O

预取、压缩和写出可隐藏延迟，队列同时约束容量、在飞字节、背压、取消、超时、错误传播和收尾。正式I/O等待可以背压；开发快照超预算按[诊断规范](DIAGNOSTIC_PROBES.md)丢弃并计数。异步任务持有数组/句柄所有权或显式租约，禁止临时指针跨回收点。

验证实际在飞线程和预算、重复调用、取消/异常、队列拥堵、共享缓存及释放竞态，分别报告数值差异和资源读数。CPU利用率是观测，不构成产品执行审批门。
''',
 'docs/engineering/standards/CACHE.md': r'''# 缓存与科学暂存

缓存复用已定义结果；正式cache/spill属于科学执行的数据依赖，与[开发快照](../architecture/DEV_MODE.md)分开。缓存关闭或失效后仍按同一科学定义重算，不从快照副本取数。

## 必备合同

每个缓存明确容量和在飞字节、输入/配置/算法身份、失效、线程与所有权、生命周期、命中/重算/失败状态。键包含使数值结果改变的全部条件；路径可作定位，但路径或指针相同不足以认证内容。TTL只限制时效，不能替代版本/内容身份。

| 缓存 | 当前定位与需核对事项 |
|---|---|
| UPM dense文件 | model/source hash、目标阶、格式和容量；打开时检查来源，拒绝陈旧或损坏内容 |
| Gaia查询 | gaia_client.c的方向/半径/星等窗、类型/文件数及版本比较；客户端绑定、TTL和内存预算分别核对 |
| Gaia解压块 | client共享预算、文件/块身份、淘汰、mmap及在途借用 |
| Drizzle目标几何 | spherical_overlap.h的线程私有LRU；每run的NSIDE/模型绑定和clear需核对 |
| Export tile/负缓存 | p3_resample.cpp的输入产品、层级/tile、容量、缺块状态和跨运行失效 |
| 浏览器tile/显示缓存 | 工具/UI消费者自己的容量与寿命；不冒充科学输入缓存 |

该表是定位，不固定各实现的全部容量常量；实际值、配置与内存画像由模块说明和运行记录给出。有限缓存不等于总驻留有界，还要计解码scratch、在飞I/O和外借对象。

## 失效与并发

变更输入内容、模型、标度、WCS、参考或序列化版本时按对应依赖失效。单写多读、线程私有或互斥的方案由具体对象说明，命中路径返回借用view或独立副本要明确。destroy与在途读串行化；淘汰只回收无消费者的数据。

冷/热路径对同一身份给出相同科学值，比较保留有效域和缺项；损坏缓存明确报错或按已定义状态重算，不能静默当正常命中。数值地板、模型选择或迭代次数不能由缓存是否命中偷偷改变。

## 验证与边界

测试同身份重复、每类依赖变化、容量/淘汰、负缓存、错误恢复与并发释放；记录命中、解压、实际读字节和内存峰值。ACSD Gaia仅本地读取，旧“外部请求每组一次、两级落盘网络缓存”不适用当前模块；本地读取次数不等同外部网络请求。原文保存于[来源档案](../../../实验/documentation-refactor/docs/STANDARDS_SOURCE.txt)。
''',
 'docs/engineering/standards/COMPATIBILITY.md': r'''# 兼容性

兼容性覆盖配置、数据、模型、公开API/ABI和已发布产品。依据[最高设计](../../ACSD_DESIGN.md)，科学正确性与可解释身份优先；机器形状可读不表示物理语义等价。

新增字段可以加性演进，但未知字段是否接受由具体版本合同决定。改变单位、坐标原点、估计量、状态或默认值时同步正本、schema、writer/reader及消费者；不可透明兼容时明确版本与adapter，不悄悄把旧值按新语义解释。版本变更和发布按[发行合同](../build/RELEASE.md)执行。

UPM持久化按明确frames列表绑定系数，检查帧ID唯一、类型、系数行数和模型来源；容器遍历顺序不能决定frame_id。生产排异扩展入口与兼容薄包装分别维护，是否可达沿实际调用核对。畸形、悬空模型和单位冲突明确报错，合法signal但precision unavailable保留对应能力。

同一配置键的登记、默认与生产消费者一致，具体规则见[配置合同](../contracts/CONFIG.md)。格式/API/schema版本具有兼容意义时保留；历史轮次只在证据中解释，不能凭标签决定产品可用。接口改动以具体消费者和正负例复核，不新增研发审批表。
''',
 'docs/engineering/standards/OPTIMIZATION.md': r'''# 性能优化

优化从实际profile出发，保持科学定义、精度与有效域。预算和测量口径见[性能模型](../resources/PERFORMANCE_MODEL.md)，CPU能力及后端见[后端架构](../resources/cpu/BACKEND.md)。

固定机器、输入、配置、二进制/构建身份与工作量，区分计算、I/O、锁、分配和串行段，按真实墙钟占比选目标。候选包括空间索引替代全对比较、scratch复用、重复tile读取、层级重建与浏览器映射缓存；这些只是待实测方向，不能据清单声称瓶颈已确认。

候选实现有相同科学对象的独立reference/oracle，检查信号、几何、PSF、噪声/控制与能力状态，再比较耗时、内存、读写和散度。至少重复并报告样本数与分位数，快照和探针自身开销另列。优化失败与无安全收益的尝试也保存，不只留最快一次。

不要为提高利用率改变精度、掩膜、迭代停止或模型；有意改变算法时按科学问题单独研究。快路径域外明确拒绝或走已定义的等价路径，不能静默降质。收益和回退随目标数据域解释，不用吞吐改善替代科学验证。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/STANDARDS_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Standards source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
