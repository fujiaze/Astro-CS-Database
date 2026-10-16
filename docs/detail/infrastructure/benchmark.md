# Benchmark 实现

`acsd benchmark`进入`lib/infrastructure/cli/commands.cpp`，调用`backend_host::generate_profile_v2`生成机器画像，再经`benchmark_profile_verdict`及`save_cpu_profile_atomic`处理。字段和测量原则见[benchmark合同](../../engineering/resources/BENCHMARK.md)，路由见[CPU后端](../../engineering/resources/cpu/BACKEND.md)。

输入是实际CPU/OS/ISA、软件/二进制身份、kernel与worker/block候选；输出cpu_profile只控制执行参数，不能改变响应、参考源、精度模型或科学值域。生成器的测量是否完整对应每个profile字段，沿profile_gen.cpp、bench_harness.cpp与实际kernel调用核对。

CLI先校验组装不变量，失败不写完整画像；写入优先安装目录，失败后明确报告原因及用户级回落路径。当前命令stdout为`<画像路径> <verdict>`，诊断走stderr；路径可能含空格，消费者不能按首个空格截断路径。保存失败与画像判定失败返回各自错误，不能把文件存在当成功。

科学命令运行时，commands.cpp解析画像路径、调用validate_cpu_profile，再进入cli_select_backend及内存预算解析。校验、失效回落与实际后续消费者是否一致仍需逐路验证，不能只看前面的warning就认为无效画像一定未再消费。当前run_manifest基础字段未写所用profile身份，见[清单合同](../../engineering/contracts/MANIFEST_VERIFY.md)。

画像的repeats、kernels、output等内部参数不自动构成CLI旗标；当前benchmark公开命令表没有这些选项。具体schema、默认值和取值通道从真实消费者核对。无画像使用明确的保守baseline及预算，不因没有画像拒绝合法科学输入。

验证固定输入下的误差、吞吐散度、冷/热、画像篡改、机器/软件/provider失配、不可写目录、原子保存和实际路由。旧文中的已完成测试声明保存在[来源档案](../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)，本轮未执行benchmark。
