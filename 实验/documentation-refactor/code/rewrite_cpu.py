"""Preserve reviewed CPU/loader references and separate execution families."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/resources/BENCHMARK.md': r'''# Benchmark 与机器画像

benchmark评价数学等价实现的执行成本，产生机器绑定画像，不替用户选择科学模型。资源统计见[性能模型](PERFORMANCE_MODEL.md)，实际入口见[benchmark细节](../../detail/infrastructure/benchmark.md)，字段归属见[配置合同](../contracts/CONFIG.md)。

采用可比的优化构建，记录CPU/OS、编译器、ISA、二进制与依赖哈希、worker预算、数据规模、热身、重复次数与负载条件。吞吐与数值误差成对报告，多轮稳健统计不取单次最快值。测量开销、初始化、I/O与计算分别计时，失败和无收益候选也保存。

`acsd benchmark`的正式输出是cpu_profile：优先发行布局的安装位置，不可写或构建树场景显式回落到用户可写目录。开发profile实验的脚本、命令、输入和轻量读数保存在所属实验；不能把所有画像固定写到仓库run/，也不能因此禁止本地实验留档。

画像绑定机器、软件、provider/内核及调优条件，失配明确报告并按已定义基线运行。生成、结构/身份校验、实际路由使用和真实科学结果分别核验；画像自报PASS不等于产品已科学验收。
''',
 'docs/detail/infrastructure/benchmark.md': r'''# Benchmark 实现

`acsd benchmark`进入`lib/infrastructure/cli/commands.cpp`，调用`backend_host::generate_profile_v2`生成机器画像，再经`benchmark_profile_verdict`及`save_cpu_profile_atomic`处理。字段和测量原则见[benchmark合同](../../engineering/resources/BENCHMARK.md)，路由见[CPU后端](../../engineering/resources/cpu/BACKEND.md)。

输入是实际CPU/OS/ISA、软件/二进制身份、kernel与worker/block候选；输出cpu_profile只控制执行参数，不能改变响应、参考源、精度模型或科学值域。生成器的测量是否完整对应每个profile字段，沿profile_gen.cpp、bench_harness.cpp与实际kernel调用核对。

CLI先校验组装不变量，失败不写完整画像；写入优先安装目录，失败后明确报告原因及用户级回落路径。当前命令stdout为`<画像路径> <verdict>`，诊断走stderr；路径可能含空格，消费者不能按首个空格截断路径。保存失败与画像判定失败返回各自错误，不能把文件存在当成功。

科学命令运行时，commands.cpp解析画像路径、调用validate_cpu_profile，再进入cli_select_backend及内存预算解析。校验、失效回落与实际后续消费者是否一致仍需逐路验证，不能只看前面的warning就认为无效画像一定未再消费。当前run_manifest基础字段未写所用profile身份，见[清单合同](../../engineering/contracts/MANIFEST_VERIFY.md)。

画像的repeats、kernels、output等内部参数不自动构成CLI旗标；当前benchmark公开命令表没有这些选项。具体schema、默认值和取值通道从真实消费者核对。无画像使用明确的保守baseline及预算，不因没有画像拒绝合法科学输入。

验证固定输入下的误差、吞吐散度、冷/热、画像篡改、机器/软件/provider失配、不可写目录、原子保存和实际路由。旧文中的已完成测试声明保存在[来源档案](../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)，本轮未执行benchmark。
''',
 'docs/engineering/resources/cpu/BACKEND.md': r'''# CPU 后端架构

CPU后端提供同一数学kernel的可选实现，通过能力和测量选择。[ISA变体](ISA_VARIANTS.md)维护编译隔离，[能力探测](CAPABILITY_PROBE.md)解释硬件/OS状态，[安全加载](../../api/abi/SECURE_LOADER.md)解释模块加载。科学定义与单位仍由science拥有。

## 两族接口

| 族 | 文件/入口 | 消费者与清单 |
|---|---|---|
| backend | acsd_cpu_*，acsd_backend_get_api_v1 | backend_host/backend_loader.cpp与cpu_routing；backends.manifest.json |
| provider | acsd_cpuprov_*，acsd_provider_query_v1 | module_loader及provider查询；providers.manifest.json |

二者有不同ABI、能力位与消费者，不能混装或按相同名字互换。生产命令构建内adapter与独立DSO也分别核对。host通过kernel_id查询能力，不能把全进程切成一个含糊“AVX2模式”。

## 加载与执行

基础程序和握手门面按baseline编译，只有计算TU带高级ISA旗标。执行前检查实际CPU特征、OS寄存器保存状态、所需子集、合法库路径、文件哈希、ABI和自检；硬件支持不等于OS可安全执行，安全可执行也不等于当前机器最快。线程配额属于资源预算，和ISA能力分开。

跨外部边界使用版本化C ABI、POD/span、host回调及明确分配器。struct_size/abi_version不符拒绝，不猜布局；异常在内部转换，backend不私建长期池。kernel描述记录数学身份、算法/实现版本、dtype、别名、对齐、取消与并发。

## 选择、失败与证据

启动前可选变体不可用或无收益，记录原因并选合法baseline；计算中失败按阶段错误处理，不静默换后端继续混合结果。画像与机器/provider不符时失效并报告，后续真实消费者还要拒绝陈旧值。baseline自身失败不能继续伪成功。

每kernel比较独立科学参考及baseline，记录误差、负值/近零/极值和实际适用域，再比较吞吐。共享源只减少重复实现，不是独立oracle，也不证明FMA或编译器差异始终在固定容差内。安装声明、可加载、自检和科学执行分别报告；旧测试路径与历史读数在[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。
''',
 'docs/engineering/resources/cpu/CAPABILITY_PROBE.md': r'''# CPU 能力探测

能力层区分硬件特征hw与OS可安全执行集合os_safe，不负责核心数预算或profile写盘。实际实现为`lib/infrastructure/benchmark/cpu/common/src/capability_detect.c`，公开头在同目录树`include/acsd/cpu/capability_v1.h`。

## 探测与判定

先查询CPUID，再在OSXSAVE允许时执行XGETBV，记录XCR0；高级kernel调用之前核对required特征是否属于os_safe。不能让高级ISA污染检测/握手自身。provider需求按其真实编译/指令与独立位词表解释，不把backend和provider的整数掩码互代。

当前classify把SSE/BMI等按相应硬件位处理，AVX家族要求OSXSAVE及XMM/YMM状态，AVX-512分支还有整组子集和opmask/ZMM条件。准确逻辑以cap_classify的嵌套条件核对；单列0xE0不等于完整前提，也不能把项目整组策略当所有AVX-512硬件的普适定义。具体位值保持公开头，不在本页另列第二套数字表。

## 接口与输出

acsd_cap_detect_v1采集，acsd_cap_classify_v1可接受合成证据做判定，acsd_cap_os_safe_satisfies_v1检查required。POD前两字段struct_size/abi_version，错误码与该头对应，不跨边界托管分配。schema为common/schemas/cpu_capability.schema.json，包含CPU身份、CPUID、OS状态、hw/os_safe及判断；序列化稳定性与真实不同主机输出分别验证。

亲和性、cgroup/Job Object、可用核心及线程租约由Runtime资源层测量。本层的能力记录不证明当前模块已加载或执行该指令，也不证明profile参数已生效。

## 验证范围

合成CPUID/XCR0矩阵覆盖缺AVX、缺FMA、缺OS保存、缺子集和非法位，再与真实平台采集比较；合成输入应通过实际判定函数，独立期望需来自一手硬件规范。两平台同schema、实际探测与真实kernel运行分别报告。旧测试目录和矩阵是设计/历史记录，须先取得载体才能称已执行，原文保存于[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。
''',
 'docs/engineering/resources/cpu/AVX2_PROVIDER.md': r'''# AVX2/FMA provider

本页说明provider族AVX2实现，依据[后端架构](BACKEND.md)与[ISA隔离](ISA_VARIANTS.md)。实际位置为`lib/infrastructure/benchmark/cpu/avx2/`，查询入口是acsd_provider_query_v1，不是backend族的get_api。

## 注册与调用

当前注册calibration-pixel-transform与hips-bulk-transform两个kernel，与baseline用相同kernel_id/科学身份。其他kernel由host查表后选baseline，provider表外调用返回unsupported；provider自身不偷偷执行另一模型。

query握手核对ABI、必要host服务和能力，acsd_cpu_avx2_cap_gate使用capability层要求AVX/AVX2/FMA及OS状态。门面TU按baseline编译，计算TU avx2_kernels.cpp才带高级ISA旗标；旧“整provider TU带-mavx2”的叙述已过时。能力失败时返回不支持，尚不能据此保证加载前全平台任何路径绝无非法指令。

## 数值合同

calibration逐元素计算`(a-b-k c)d`；hips-bulk按定义的四邻点系数组合。FMA改变中间舍入，编译器、相消、非有限值与有效域需实测；没有跨线程归约不等于任意编译配置下结果逐位相同。

既有baseline比较量为`|a-b|/max(1,|b|)≤2e-4`，这是带绝对量级地板的混合尺度指标，不是所有非零b的纯相对误差。baseline与科学reference各在同阈值内，并不能推出variant与reference也必在同一个阈值内；疑点列[Q26](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。本轮保留原比较参数，不推导新界或修改kernel。

## 内存、预算与验证

参数POD与baseline公开头复用，输入输出别名、容量、dtype、对齐与所有权按该接口核对。host授预算并控制在飞工作，串行回落必须保持数学定义。DSO句柄覆盖函数指针所有调用，再按协议shutdown/unload。

验证真实/模拟能力拒绝、ABI、表外kernel、门面无高级指令、计算面实际发射、非首元素失败、极值/相消、预算1/N和独立reference。老20%/28%等增益只对应原机器/批次，不当本轮性能结论；原文和测试意图在[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。
''',
 'docs/engineering/resources/cpu/ISA_VARIANTS.md': r'''# ISA 变体与逐内核选路

变体服务同一科学kernel，在受控域保持数值语义后按实测性能选择。逐kernel路由和两族入口见[CPU后端](BACKEND.md)，实际构建见[构建图](../../build/BUILD_GRAPH.md)。

## 两族与注册

backend族acsd_cpu_*使用acsd_backend_get_api_v1及backends.manifest.json；provider族acsd_cpuprov_*使用acsd_provider_query_v1及providers.manifest.json。各有ABI、能力位与消费者，构建可生成的DSO不等于当前发行推荐或运行实际选择。

目前AVX2 provider只登记calibration-pixel-transform和hips-bulk-transform；其他kernel按host选择合法baseline。AVX、AVX-512与整数/位扩展是否有收益取决于机器、编译器、数据与内核，不能从指令集包含关系推导吞吐严格支配。旧SHIP/NOT_SHIPPED及10%经验带是历史批次决策，不是普适性能定理。

## 编译隔离

两族均拆门面TU和计算TU：门面承载query/get_api、自检入口和表，按baseline编译；计算面通过唯一跨TU桥调用，局部设置ISA旗标。backend桥为acsd_variant_kernel_dispatch_v1，provider桥为acsd_cpuprov_kernel_range_v1。

根CMake给OBJECT计算目标配置GCC/Clang的-mavx2/-mfma或具体AVX-512子集，MSVC用对应/arch及浮点收缩策略。不能把旗标挂到整个主CLI、baseline或门面；开启LTO/链接优化后还需产物级检查，源码隔离不是最终指令隔离证明。

## 能力声明与产物核对

manifest记录实际文件hash、ABI、required_features、compiler、局部flags及kernel能力。编译许可面、实际发射面和运行required集合分别核对，包含隐式FMA等依赖。GCC/MSVC与两族掩码来自各自位表，不能复制旧928/992数字当通用安全判定。

反汇编检查计算面具有所声明档位的实际使用，query/get_api/能力门面能在baseline机器安全到达。动态导出白名单与Windows导出桥差异分别登记；多一个可解析符号和ABI语义变化不是同一问题。

## 测量与数值

固定输入、编译、预算、机器与统计协议比较，热身/冷读、重复中位数/散度及baseline取值策略如实保存。共享impl减少复制，不替代独立科学oracle。FMA/相消/尾部长度、别名和不同worker下的数值与身份都需验证，容差按[AVX2说明](AVX2_PROVIDER.md)和被估计量解释。

整数索引存在不代表BMI/POPCNT可加速，原12kernel的位操作扫描只支持其输入与实现集合。未来热点改变后重新测量，不能把“本次无收益”冻结成永久不提供能力。

## 原始证据与当前限制

旧文详细的ISA-001/002/003读数、R-60过程、目标迁移及cpuprov_avx512发射FMA却漏声明的记录完整保存于[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。其中既有风险须按当前CMake、清单生成器与真实二进制重核，不能因重写正文就宣布已修复；本轮未编译、反汇编或运行kernel。
''',
 'docs/engineering/api/abi/SECURE_LOADER.md': r'''# 安全加载

loader消费host解析的授权单元，检查路径、文件、ABI和模块身份后返回句柄。基础类型见[C ABI](ABI.md)，两族CPU接口及回落政策见[后端架构](../../resources/cpu/BACKEND.md)。特定文件加载失败不搜索其他目录，host选择合法baseline是另一层路由。

## 当前Linux路径

`lib/infrastructure/pipeline/module_loader/secure_loader.h/.c`实现host侧接口：

1. 入参为授权绝对路径，realpath后核对canonical与allowed_root。
2. 检查ELF64/x86-64及声明的SHA-256；未登记摘要的兼容分支与完整授权分支分别解释。
3. dlopen使用明确文件及RTLD_NOW/LOCAL。
4. 查询所需module/provider符号，执行host ABI握手。
5. describe核对结构、module/build/version与授权记录。
6. 成功返回拥有句柄，失败清空out并返回稳定状态/detail。

路径文本检查不能单独证明无TOCTOU；文件校验与dlopen间的替换窗口、依赖库实际解析路径和初始化行为需要专门验证。顶层库使用绝对路径不等于所有传递依赖都已脱离LD_LIBRARY_PATH影响。加载器不应执行未经能力核对的高级指令，CPU能力接线与本路径核验分开。

## 平台与所有权

当前secure_loader.c的Windows分支返回unsupported。Windows目标是Unicode路径、受限LoadLibraryExW搜索、PE/AMD64、摘要和ABI/描述校验，不能把目标步骤列成已实现。另一个backend_loader是否支持Windows应沿该族单独核对。

host传allocator与回调，loader输出字符串和句柄按其release接口回收；在途函数指针/对象完成后才卸载。外部异常不穿边界，内部错误转换为对应状态。模块生命周期不因dlopen成功就宣称已完成validate/plan/execute。

## 错误与验证

路径、hash、架构、符号、握手、descriptor和I/O错误用secure_loader.h的detail区分，不直接当CLI退出码。错误文本去敏，由调用方统一日志，loader不私写文件。摘要只检查已授权期望与当前文件一致，不证明期望清单本身可信。

验证canonical内外、链接逃逸、校验后替换、缺符号、错误ABI/build、传递依赖劫持、句柄释放及真实noop/provider；记录平台和实际调用者。旧无脚本矩阵、P/N/L标签与安全强断言完整保留于[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)，本轮未加载动态库或执行注入。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/CPU_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'CPU source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
