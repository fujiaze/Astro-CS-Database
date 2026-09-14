# 前台自证条目（producer=FD）· F_TEST_GAP × P1

> 取证手段：当场 `sed`/`grep`/`strings` 只读复验 L28c 与 E3 的回传，未修改任何被扫描文件。

### FD-F-001 「禁复制漂移」的守卫是用 `strstr` 查源文件名 —— 它测的是文本，不是行为
- **ID**：FD-F-001 ｜ **类别**：F_TEST_GAP ｜ **优先级**：**P1** ｜ producer=FD（前台自证；14 域与四补轴均已收工，无人承接）
- **位置**：`tests/unit/cpu_provider_test.cpp::check_shared`（当场读到 `CHECK(s.find("baseline_kernels_impl.inc") != std::string::npos);` 与 `CHECK(s.find("backend_table.inc") != std::string::npos);`）；注册面 `tests/unit/CMakeLists.txt::cpu_provider`（`add_executable(cpu_provider_test …)` + `add_test(NAME cpu_provider …)`，并在同文件的目标清单里）
- **问题说明**：该用例的"共享纪律"断言 = 打开 `avx2_backend.cpp`/`avx512_backend.cpp`/`avx_backend.cpp` 三个源文件、`fread` 进 4096 字节缓冲、再 `std::string::find` 查两个 `.inc` **文件名字符串是否出现过**。
  它**不加载、不调用、不比对任何一份表的内容**。⇒ 只要那行 `#include` 还在，无论 ①表被复制成第二份、②`.inc` 内容与头注释漂移、
  ③变体实际编译进不同的表、④甚至 include 写在一个永不生效的 `#if 0` 里，**本用例全绿**。更具体：它 `fread` 上限 4096 字节，**include 行若在文件 4 KB 之后即读不到**（当前 4 个 backend 文件里 `baseline_backend.cpp:27`、`avx_backend.cpp:22`、`avx2:26`、`avx512:27` 都远小于 4096 而恰好通过）。
- **为什么这条与 FD-G-002/M8-F-002 不重复**：M8-F-002 是"断言不进退出码"，FD-G-002 是"红被基线豁免"，本条是**断言对象错位**——门在跑、退出码也接、也没有被豁免，
  但它检查的谓词（"文件名出现过"）**与被守护的不变量（"表只有一份、不漂移"）之间没有逻辑蕴含关系**。这是簇 1 的**第三种失守机制**，前两种都不能覆盖它。
- **依据条款**：宪章 §12.3-4（测试须能证伪）、§13.1/§13.2（验证层须覆盖被声明的不变量）；根 AGENTS.md「科学定义=算法=接口=代码=测试」
- **建议处置**：①把该断言改为**行为级**：分别加载 baseline/avx2/avx512 三变体，断言其 `kKernels` 表长度、`kernel_id` 序列、`precision`/`determinism` 逐列相等（同仓 `lib/backend_host/backend_table.inc` 已提供唯一源，比对成本极低）；
  ②或至少把它降级为"源结构烟测"并在名字里写清，不得再充当"共享纪律"的证据（现它被 `docs` 与注释当作禁漂移的守卫引用）；③机器门：任何 `CHECK(s.find(…))` 形式作用于**读入的源文件文本**且断言对象是文件名/路径字面量者，登记为可疑断言（可与 L28b 的 R-6b 同批实现）
- **取证口径**：`grep "#[^#]*include.*backend_table"` 命中 4 个编译单元（证明 .inc 确在编译面）；`sed` 直读 `cpu_provider_test.cpp:65-80`；`strings` 见 FD 系列另一条（交付单元内 11 处 ALG-0NN）
- **置信度**：高（用例正文与注册面均当场读取）｜ **related**：**L28c-D-001**（同一条 `.inc` 携带未登记 ALG-008/009 且已由前台实测进交付二进制）、**L28b-D-005**（消费侧 `baseline_provider.cpp:488-493` 对 `sci_contract_id` **只判非空**）、M8-F-002、FD-G-002、簇 1

### FD-F-002 同机制第二实例（由前台当场实测补入，与 FD-F-001 合为「子串断言守卫」家族）
- **ID**：FD-F-002 ｜ **类别**：F_TEST_GAP ｜ **优先级**：**P1**（与 FD-F-001 同判据；两条**不并档**，一为 provider 表、一为资源归一化声明，删除任一条另一条仍成立）
- **位置**：`cli/resource_recorder.h:243`（生产者）＋ `tests/unit/mon001_recorder_test.cpp:98`（守卫）
- **证据摘录**（前台 `grep -n` 当场原样输出）：
  > `cli/resource_recorder.h:243` …`"\"normalized_cpu_100pct_all_allocated_cores\":true,\"stages\":[",`
  > `tests/unit/mon001_recorder_test.cpp:98` …`CHECK(js.find("\"normalized_cpu_100pct_all_allocated_cores\":true") != std::string::npos);`
- **问题说明**：生产者把 `normalized_cpu_100pct_all_allocated_cores: true` 作为**字面量硬写进输出 JSON**（无任何计算路径可使其为 `false`）；
  守卫侧则用 `js.find(子串)` 断言"这句话出现在输出里"。⇒ **该字段既不表达事实、也无可证伪性**，但它会被下游（人、门、报告）读成"CPU 百分比已按全部已分配核归一化"。
  而 E2 实测的真相是：**85%/90% 那条门吃的是 1 核口径**（`resource_recorder.h:101 = (d_cpu_seconds/interval_)*100`，不经 `utilization_value`），
  已分配容量在控制节点为 **16 核**，历史 177 条有值样本里 **146 条落在 100–199 档（≈1 核）**、按容量口径**过 85% 仅 6 条**而现行代码全判达标。
  ⇒ **这个字面量正是 M5a-G-002（P0）的自我声明面，而它被一个子串断言"验证"过。**
- **两例合起来的机制名（写进簇 1 作第四种失守机制）**：**「子串断言守卫」** —— 用 `find(字面量)` 对**文本产物**（源文件正文 / 生成的 JSON）做断言，
  与被守护的不变量之间无逻辑蕴含关系。它与 M8-F-002（断言不进退出码）、FD-G-002（红被基线豁免）、M5b-G-01（门验错对象）都不同，**且最难识别，因为它确有 CHECK、确有 ctest、确有 PASS**。
- **建议机器门（可当天入、零假阳）**：任何 `CHECK(*.find("` / `if (*str.find(` / `assert("… : true" in …)` 形态，**且其参数含 JSON 键值对字面量或文件名/路径字面量**者，登记为可疑断言并要求改写为值断言或结构断言。
- **路径订正**：E2 回报里写的 `resource_recorder.h` 未带目录，真身在 **`cli/`**（`lib/backend_host/` 下无此文件）；属 PATH_MISMATCH，**只改锚不撤证据**。
- **置信度**：高（生产者与守卫两处均当场 `grep -n` 取原文）｜ **related**：**FD-F-001**（同机制第一实例）、**M5a-G-002**（P0，本条是其声明面）、M8-F-002、FD-G-002、M5b-G-01、簇 1 第四机制