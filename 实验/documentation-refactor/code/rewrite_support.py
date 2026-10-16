"""Rewrite shared-library and operator troubleshooting documentation."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/detail/common/shared_libraries.md': r'''# 共享基础库

lib/algorithms/shared/ 提供跨阶段复用的几何、内存摘要和标量类型原语，不持有文件通道或业务管线。[依赖标准](../../engineering/standards/DEPENDENCY.md)规定单一实现，[数值标准](../../engineering/standards/NUMERIC.md)规定对象精度，[Runtime](../../engineering/contracts/RUNTIME.md)规定生产资源和寿命。

## 入口与边界

| 公开头 | 入口与职责 |
|---|---|
| healpix/healpix_core.h | acsd::healpix 的 ang2pix_nest、pix2ang_nest、局部位交错、FITS排列、父子层级、query_disc、neighbors |
| crypto/sha256.h | acsd::crypto::sha256_hex 和增量 Sha256，对调用者提供的内存计算摘要 |
| include/astro_scalar.h | AstroScalarType、标量特征与分发支持 |
| include/precision_context.h | 现存 PrecisionContext 单例的标量选择兼容接口 |

HEALPix/SHA 分别有 .cpp 实现，不是纯头库。几何坐标 ra/dec 用度，query_disc 半径用角秒；NESTED 层级差与实际 order 分开。内部像元零基中心、HiPS FITS 排列与独立参照见[映射科学](../../science/algorithms/export/HEALPIX_MAPPING.md)。非法 nside 抛 invalid_argument，pix2ang 的越界 ipix 按现头约定返回零坐标，不能把该哨兵当合法定位成功；层级操作的溢出/范围前提逐函数核对，不将某函数的检查推广到所有 inline。

## 摘要与身份

Sha256 只负责字节摘要，不负责定义科学帧身份。frame_id 的载荷、规范化、截断、排序与缺项必须在实际生产者解释，不能从 SHA 工具存在推断全链统一。

当前 coverage/src/sampler.cpp 的 p2_frame_id 自行组装所选 properties、signal/support 的原生 float 字节和可选 SNR Catalogue，再截取摘要前16个十六进制字符。它不等于完整 tree_hash，也不自动覆盖唯一 frame_science 模型的全部变化；日期字段、原生字节序、目录读取上限和可选面错误语义仍需逐项核对。输入 occurrence 身份也不同于内容摘要，来源和重复输入不能只由截断摘要消歧。

Sha256 实例由调用方持有，不并发修改同一实例，输入内存至少在 update 期间有效。final_hex 为终结调用；当前实现终结后 update 静默返回，再 final_hex 返回64个零字符，调用者不能把它当再次计算成功。文件摘要走 AIO，不在共享算法库自行打开文件。摘要一致不证明来源真实或科学正确。

## 精度、寿命与并发

PrecisionContext 是现有全局兼容机制，默认 FP32，普通字段读写无同步，约定仅启动阶段写一次；跨 DLL 仍须显式传模式，不能以头内单例证明所有动态库共享一个实例。原 orchestrator 有启动设置点，AIO 通过 aio_set_precision_mode 接收；独立三命令的实际路由须沿真实消费者核对。

目标生产模型把精度作为显式对象/调用合同，不能据旧全局开关覆盖稠密FP32、WCS/模型FP64等各对象归属，也不能在运行中无锁切换。几何的无状态调用与返回vector等调用方对象分开管理，错误跨ABI按边界转换，不能让内部C++异常穿C面。

验证覆盖独立几何/排列真值、极区/面缝/非法阶、SHA标准向量与分块、重复终结误用、身份变化面和不同精度/动态库接线。共享库测试只证明所调用原语，原头百万点声明与旧无测试登记属于历史证据；本轮未执行。完整原文与文献保存在[来源档案](../../../实验/documentation-refactor/docs/SUPPORT_SOURCE.txt)。
''',
 'docs/detail/infrastructure/troubleshooting.md': r'''# 排障手册

先确认命令、运行身份和实际输出，再沿错误向上游定位。退出码和错误域由[错误模型](../../engineering/standards/ERROR_MODEL.md)定义，日志与事件由[日志合同](../../engineering/contracts/LOG_AND_ERROR.md)和[CLI协议](../../engineering/contracts/CLI_PROTOCOL.md)解释；本页不新增科学回退或拒绝条件。

## 定位顺序

保存完整命令、配置、构建、输入身份、stdout JSONL、stderr、实际日志和退出码。正式输出按配置 output_dir 查找，不能假设目标 logs/ 或 log_artifacts 已在每条现有路径写出。核对 run_id、frame 与节点，再看相同运行的清单、完成状态和产物；不要把上次残留当本次成功。

从最先失败的节点追到公共入口、真实调用者和消费者，按同一输入建立最小复现。产品几何从产品读取；缺原始 WCS 头不是 IPV 求解失败的证据。输入缺失、计算无效、I/O失败、精度不可用和取消分别解释。恢复按[原子发布](../../engineering/contracts/ATOMIC_PUBLISH.md)处理，保留已发布独立产品，失败暂存隔离；不要先删证据再重跑。

## 常见症状

| 症状 | 首先检查 | 正本/处理方向 |
|---|---|---|
| 启动 DLL/后端失败 | 实际平台、架构、构建族、依赖与加载错误；不盲改 PATH | [CPU后端](../../engineering/resources/cpu/BACKEND.md)、[构建](../../engineering/build/BUILD_GRAPH.md) |
| 块/对象缺失 | 前序是否实际发布、run/frame/type/revision、消费者寿命 | [Runtime](../../engineering/contracts/RUNTIME.md) |
| 校准无变化/全零 | 原始值、单位、BSCALE/BZERO、母版身份/尺寸、实际应用分支 | [校准](../../science/calibration/CALIBRATION.md)、[FITS I/O](../../engineering/contracts/FITS_IO.md) |
| WCS失败或残差异常 | IPV实际候选/匹配/解、初值和同帧几何，分别检查solver候选与科学源 | [Normalize管线](../normalize/pipeline.md) |
| PSF/测光不可用 | 独立科学源、响应、拟合支持/状态、尺度是否恰好一次应用 | [Normalize合同](../normalize/contracts.md) |
| 参考精度 unavailable | 参考定义、gain/噪声/协方差、支持与原因；合法signal仍可保留 | [噪声与精度](../../science/noise_snr/NOISE_SNR.md) |
| 权重无效/为零 | 估计量单位、同域支持、传递因子T和实际接受样本 | [集成](../../science/integration/INTEGRATION.md)；不回退support/等权伪造精度 |
| 接缝/背景阶跃 | 同一天区、真实足迹、有效重叠图、规范、乘性/响应/PSF失配 | [UPM](../../science/sky/UPM.md) |
| 排异配置错误 | 真实枚举、样本资格与每像素路由，区分声明/实现/历史模式 | [排异](../../science/integration/REJECTION.md) |
| HiPS缺tile/黑洞 | 清单与实际叶集合、形态/索引、有效性、写盘/校验错误 | [存储形态](storage_form.md)；缺覆盖不填零 |
| Drizzle全无效/极区漏候选 | WCS/SIP、有效域、候选完备性和实际曲边算子 | [Drizzle](../../science/drizzle/DRIZZLE.md) |
| Export坐标/边界异常 | 输入层级、原点、WCS/裁剪、核与覆盖，不能凭往返自证 | [Export管线](../export/pipeline.md) |
| FITS损坏/写出中断 | 原始文件结构、大小/权限/磁盘、独立读取与发布完整性 | [AIO](aio.md)、[原子发布](../../engineering/contracts/ATOMIC_PUBLISH.md) |
| Gaia重复查询/缓存陈旧 | 本地数据身份、键、实际调用次数、共享缓存范围 | [星表客户端](gaia_xpsd_client.md)、[缓存](../../engineering/standards/CACHE.md) |
| 性能下降/超时 | 当前预算、输入规模、队列、worker租约、I/O与实际采样 | [性能模型](../../engineering/resources/PERFORMANCE_MODEL.md)；不以质量/资源诊断引入审批门 |

## 辅助工具及局限

eng/tools/acsd_diagnose.py 接受一个存在目录，可递归扫描日志与若干产物，--json 指定报告输出，--timeout 指定其外部命令超时。参数并不要求目录名为 logs；不存在才返回2。将报告写到明确的实验/诊断位置，避免覆盖原日志。默认不读取大FITS，工具摘要不能代替实际产品验证。

当前 ERROR_MAP 仍包含 exit_code=2→DLL错误、旧 rejection_cli、旧环境开关及过时科学回退建议；它按文本启发式归类，存在假阳/漏检。诊断候选原因与规范错误码不同，先核对原始错误和当前代码，不照抄自动建议。该工具扫描有限额且可能截断大日志，零命中只表示所扫范围未命中；本轮未运行工具或修改它。

Windows环境检查和构建按[构建节点](../../engineering/build/BUILD_NODES.md)选择真实工具链，不对Qt/MSVC程序一概添加MinGW路径。诊断开关只有[ACSD_DEV_MODE](../../engineering/architecture/DEV_MODE.md)，旧私有开关提示不能作为正式使用方式。日志分享先去除凭据与敏感路径，保留可复现身份。

旧20行症状表、命令和阶段标签完整保存于[来源档案](../../../实验/documentation-refactor/docs/SUPPORT_SOURCE.txt)，不以旧表声称相应字段、回归载体或回退当前有效。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/SUPPORT_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Support source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
