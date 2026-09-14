# V9 片3 · **P0**（登记面存在但真判定零载体）

### V9-N-07（P0·机制①最纯形态）**5 道非豁免门在 CI 里只跑 `--selftest`**：真实判定模式需要交付物参数，而 CI 从不传 ⇒ 交付二进制 ISA 泄漏 / 生产可达性 / 生产图一致性**在 CI 中没有实体门**
- **五门（全在 `fast`+`linux-main`+`windows-main`、`waivable=false`）**：`PRODUCTION-GRAPH`=`check_pipeline_graph.py --selftest`、`ISA-LEAK-SELFTEST`=`check_isa_leak.py --selftest`、`SERIAL-HEAVY-SELFTEST`=`check_serial_heavy.py --selftest`、`PROD-REACH-SELFTEST`=`check_prod_reachability.py --selftest`、`LOG-CONTRACT-SELFCHECK`=`check_log_contract.py --selfcheck`。
- **读码直证**：`check_isa_leak.main` 真实模式需 `--binary/--avx2-lib/--avx512-lib`，缺 `--binary` 时打印 `ISA_LEAK_FAIL: --binary required` 并 `return 1`——**而 CI 从不传，因为它跑的是 `--selftest`**；`check_prod_reachability.main` 真实模式要 production binary + `compile_commands.json`，找不到即 `exit 2`；`--selftest` 只在 tempfile/内存假字符串里自证扫描器逻辑（`selftest()` 内四段**写死的 objdump 文本**）。
- **后果（三条宪章红线无门）**：§10.2/§10.3 **交付二进制 ISA 泄漏**、**生产可达性**、§17 **生产图一致性**。⇒ **产品怎么回归都不会让这 5 道"不可豁免门"变红**，与 `V11-N-05`（ABI 协商门自证）、`V15-N-06`（恒真 kernel 白名单）**同属"门检自己的副本"**，但本条覆盖面最大。
- **免重报说明**：`SERIAL-HEAVY` 尚可（真面另有 `NO-SERIAL-HEAVY` 承载）⇒ **其余 4 门的真面在登记面完全无载体**，V9 已如实区分。
- **建议**：①CI 增传交付物参数（构建后跑真模式，缺产物即 `FAIL` 而非 skip）；②或把这 5 门改名 `*-SELFCHECK` 并另立真门（现有命名**主动误导**）；③配门：`waivable=false` 的门若其 command 含 `--selftest/--selfcheck` ⇒ 红（`E11`）。
- **related** `V11-N-05`、`V15-N-06`、`M8-G-001`、簇 1 机制①③、`E11`（新建议门）、`A-38`
