# 安全加载

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
