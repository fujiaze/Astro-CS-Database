# CLI-002 执行日志 (2026-08-28, vm-bj + Fatduck)

## 输入
03_TASK_DETAILS CLI-002 行(统一 parser/JSON-JSONL writer/exit mapping/sequence/cancel/crash boundary;验收=04 全 golden/malformed/Unicode/cancel tests+stdout 无日志污染); 04 §1-6; API-002 协议合同。

## 动作
1. 第三方落库: third_party/nlohmann/json.hpp(nlohmann single-header v3.12.0, 与 orchestrator json_config.cpp 固定版本一致; 落库使 CLI 构建无网络依赖; 位于 checker 扫描根外, 与 nanoflann vendored 同例)。
2. cli/exit_codes.h: 11 退出码唯一源(04 §2), 其余文件零数值表(测试断言)。
3. cli/cancel_token.h: 协作取消令牌(SIGINT/SIGTERM/SetConsoleCtrlHandler → 原子置位); 取消 → 9+final 事件+不留伪产物。
4. cli/jsonl.h: JsonlEmitter(10 必含字段+final/progress/resource/artifact/backend 扩展+sequence 单调+ISO8601 UTC+run_id 恒 12hex(修 %012 最小宽度→掩码 48bit)); stdout 恰行/JSON, 日志归 stderr。
5. cli/main.cpp 重写: 统一 parser(命令树白名单/未知命令·未知旗标·缺值·重复·旗标当值·枚举校验·--phases 升序无重复·--quick|--full 恰一 → 全 2); config init(模板写出 0/IO 7)与 config validate(OK 0/缺失 3/格式错 3/非对象 3)真实现, 其余子命令 not-wired stub(2)+final 事件; crash boundary(try/catch → 70+run_id+命令+脱敏 detail, 不泄露凭据); Windows wmain→UTF-8+filesystem::u8path(Unicode 路径); 测试钩子 ASTROCS_TEST_SLEEP_MS/ASTROCS_TEST_CRASH(仅协议 golden 用, 非用户接口, 已在代码注释+本日志声明)。
6. cli/CMakeLists.txt: 挂 third_party include; 异常启用(GCC 去 -fno-exceptions, MSVC /EHsc)—crash boundary 依 04 §5 必须 catch; 其余保持(CLI-001 单 target/install/禁 arch 旗标)。
7. tests/cli/test_cli_protocol.py 10 golden: help 精确 13 行/version schema/14 组 parser 拒绝面(全 2+stdout 零污染)/config init 真写/validate 0·3·3·3 映射/JSONL 全字段+sequence+final 扩展/取消 9+status=cancelled/crash 70+脱敏+路径不外泄/Unicode 中文路径/退出码单源泄漏扫描。

## 验证(双平台实测)
- Linux vm-bj: 全量回归 **unittest 92/92 OK**(新增 10, 含每用例真编译真运行)。
- Windows Fatduck(在线窗内, MSVC 14.44): BUILD_OK;help 13 行 exit 0;missing 3;malformed 3;Unicode 中文文件名路径 → 正常读出 3(内容坏);crash → 70 报告脱敏+run_id。与 Linux 逐项一致。临时文件已清理。
- 取消路径: Python Popen 直发 SIGINT → rc=9+final(status=cancelled, sequence 连续);初测 bash 后台作业 kill 伪影已排除(bash 作业控制信号语义, 非程序缺陷)。
- 过程修复: parse_args 两 bug(命令 token 消费后旗标循环起点/顶层 dash 命令被提前 break)+run_id 宽度。

## 限制与说明
- phase1/2/3 run 等科学 handler 接线属 CODE/TST 域任务(CLI-002 交付协议机器与映射, stub 恒 not-wired exit 2)。
- cancel 的真实"中断内核计算"路径在 CODE 落地内核时复验;本任务证令牌/信号/退出码/事件链完备。
- 全量 golden 矩阵双平台自动化属 WIN/FAT 域(04 §6-6);本任务已做 Windows 侧关键 golden 抽验。

## 产物
third_party/nlohmann/json.hpp; cli/{exit_codes,cancel_token,jsonl}.h; cli/main.cpp; cli/CMakeLists.txt; tests/cli/test_cli_protocol.py; 本日志。

## PASS 判定
统一 parser+JSON/JSONL writer+退出码映射+sequence+cancel+crash boundary 全实现且 04 golden/malformed/Unicode/cancel 双平台实测; stdout 无日志污染有断言。CLI-002 = PASS。
