// astrocs 退出码唯一源 (API-002 §2 / 04 §2) — CLI-002
// 本文件是 11 个退出码在仓库内的唯一定义处;其他文件只 include, 不得重定义数值表。
#pragma once

namespace astrocs {

enum ExitCode {
    OK            = 0,   // 成功, 且所有请求门禁通过
    ARGS          = 2,   // CLI 参数或配置错误
    INPUT         = 3,   // 输入缺失、格式或 hash 错误
    SCIENCE       = 4,   // 科学验证/数值不变量失败
    BACKEND       = 5,   // backend ABI、签名、CPU 特征或加载失败
    COMPUTE       = 6,   // 计算执行失败
    IO            = 7,   // I/O 失败
    INTEGRITY     = 8,   // 输出完整性/验证失败
    CANCELLED     = 9,   // 用户取消或超时
    RESOURCE      = 10,  // 资源利用率或内存增长门禁失败
    INTERNAL      = 70,  // 未分类内部软件错误; 必须生成 crash report
};

}  // namespace astrocs
