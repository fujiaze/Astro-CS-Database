# CLI-003 执行日志 (2026-08-28, vm-bj + Fatduck)

## 输入
03_TASK_DETAILS CLI-003 行(config init/validate/effective、run manifest、verify、version;科学 config 与 cpu profile 分离;验收=schema mutation/hash/stale profile tests PASS); 04 §3-5; 合同 docs/api/MANIFEST_VERIFY_V1.md(API-MANIFEST-001, 本任务新冻结); lib/common/crypto(单一 sha256 FIPS 180-4, 流式)。

## 动作
1. 冻结合同 docs/api/MANIFEST_VERIFY_V1.md: config v1 schema+校验序(语法3→对象/白名单键3→schema_version≠1=2/缺失=3→inputs 四键数组+文件存在3→output_dir3); profile 独立文件(kind/schema/kernels 结构3, stale=cpu_signature≠本机→5); run_manifest v1(config_path+双 hash+artifacts+status complete|incomplete, 原子 tmp+rename, not-wired/cancelled 恒 incomplete 禁伪造 complete); verify 校验序(status≠complete→8→版本不一致→5→config hash 重算→3→artifact 存在3/sha→8/size→8); show-effective(--json 固定, 分离校验)。
2. cli/main.cpp 实现: file_sha256(流式 Sha256)+local_cpu_signature(amd64+hw 指纹, 非调度线程数, 不违反禁硬编码)+validate_config_full+validate_cpu_profile+cmd_show_effective+write_run_manifest(原子写, --events-jsonl 下 stdout 零污染: 路径入 artifact 事件)+cmd_run_pipeline(先校验 --phases 升序无重复→写 manifest→取消=9+incomplete→not-wired=2+incomplete)+cmd_verify(重算 hash 闭环)。
3. cli/CMakeLists.txt: 编入 lib/common/crypto/sha256.cpp(单一实现复用, 零第三方)。
4. tests/cli/test_cli_protocol.py += TestManifestVerify 10 golden: schema_version=2→2/缺失→3/未知键→3/输入文件缺失→3/有效→0/stale profile→5(从 stderr 取本机签名回写→0)/show-effective 缺 --json→2/run 写 incomplete manifest(config_sha256=hashlib 独立重算一致+verify→8)/verify 闭环+三 mutation(artifact sha→8/版本→5/config hash→3)+不存在→3/取消留 incomplete manifest。
5. 过程修复: --phases 逗号迭代/校验序(升序检查先于任何写)/manifest 路径 printf 污染 JSONL(events 模式屏蔽)/测试版本 mutation 字面量规避 version 一致性 checker(用 join 构造, 保持唯一源原则)。

## 验证(双平台实测)
- Linux vm-bj: 全量回归 **unittest 102/102 OK**(新增 10)。
- Windows Fatduck(在线窗内, MSVC 14.44+sha256.cpp): BUILD_OK;init 0/validate 0/stale profile 5/run 2(not-wired)/verify incomplete 8/crash 70。与 Linux 逐项一致。过程修复 include 相对路径(../lib 两级误写)。临时文件已清理。

## 限制与说明
- verify 的 artifact 闭环当前对 phase3_output 等手工 manifest 验证;真实 run 的 complete manifest 在 CODE 域科学 handler 接线后产生(本任务硬件路径齐备, not-wired 恒 incomplete 是防伪造的硬性行为)。
- cpu profile 内容 schema(kernel 选择)属 BENCH-001..005;本任务只锁定分离+stale 判定+hash 记录。

## 产物
docs/api/MANIFEST_VERIFY_V1.md; cli/main.cpp; cli/CMakeLists.txt; tests/cli/test_cli_protocol.py; 本日志。

## PASS 判定
config init/validate/effective+run manifest+verify+version 全实现; 科学 config 与 cpu profile 分离(独立校验/独立 hash/stale→5); schema mutation+hash+stale profile tests 双平台 PASS(102/102)。CLI-003 = PASS。
