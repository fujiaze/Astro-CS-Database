"""Preserve GUI sources and distinguish requirements from existing components."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/gui/GUI_DESIGN.md': r'''# Windows GUI 设计

GUI 是 Windows Qt6/MSVC 独立桌面前端，通过阶段 JSON、子进程和 stdout JSONL 调用 normalize、mosaic、export；科学计算由命令行内核完成。上位边界见[最高设计](../../ACSD_DESIGN.md)，配置见[配置合同](../contracts/CONFIG.md)，事件与取消见[CLI 协议](../contracts/CLI_PROTOCOL.md)。

## 组件与实现状态

界面包含配置、分组、运行和产物四页。配置编辑器、分组规则、JSON 生成器、运行控制器和事件解析器相互解耦。运行控制器管理工作目录、环境、子进程和队列；浏览组件只读产品。完整 GUI 是产品目标，当前已有的是[HiPS 浏览组件](../../detail/infrastructure/hips_browser.md)，不能由此认定分组、运行编排或安装包已经实现。规划中的根 gui/ 独立工程在本轮工作树尚不存在。

配置写盘前检查类型、取值和引用路径，保留用户配置来源与可写位置；安装目录不可写时明确处理，不能静默丢失。机器画像由 benchmark 生成，GUI 展示画像和失配状态，按钮启动真实命令，不手写性能结论。

## 扫描、规则与校准匹配

递归发现 FITS，逐帧索引路径、文件名、目录、原始头、大小和修改时间；不为分组复制像素。增量扫描识别新增与变化，损坏文件单列。头字段保留原键、原值、单位与解释来源，缺失和冲突显示待处理，不能凭名称猜物理量。

| 维度 | 首选头字段 | 解释责任 |
|---|---|---|
| 帧类型 | IMAGETYP | 按明确设备映射识别 LIGHT/DARK/FLAT/BIAS；未知值保留 |
| 滤镜 | FILTER | 对应实际响应库条目，名称相同不证明响应相同 |
| 曝光 | EXPTIME | 秒；别名须有单位依据 |
| 温度 | CCD-TEMP | 实测温度与 SET-TEMP 设定值分别记录 |
| 仪器 | INSTRUME、CAMERA | TELESCOP 是望远镜身份，不能无条件代替相机 |
| 增益/偏移 | GAIN、OFFSET | 设备设置与电子/ADU转换增益分开解释 |

在头归类之上，用 Qt QRegularExpression 规则细分会话、日期、目标或设备。每条规则保存作用对象（路径、文件名、目录或头字段）、表达式、命名捕获到维度的映射、顺序和启停状态。先匹配规则确定对应维度，未匹配使用明确默认会话；路径分隔符规范化后再应用规则。编辑器提供样例与捕获预览，语法错误定位后拒绝应用。模板可编辑和另存。

匹配校准帧时展示设备、增益/偏移、曝光、温度、滤镜及会话依据，容差由配置合同提供。BIAS 的设备设置、DARK 的曝光/温度和 FLAT 的响应/会话适配只是候选索引，不能凭匹配分数证明校准科学成立。缺失、超容差及多个候选显式展示，手动改派保留来源。校准用 flat 的响应适配与 mosaic 允许异构响应输入是不同问题，不能把后者变成跨滤镜拒绝条件。

树表展示帧类型、滤镜与会话，支持拖拽改派、合并、拆分、忽略和锁定。重分组保留锁定组及手动覆盖，自动结果与覆盖分别保存。同一输入索引、配置、规则顺序、启停和手动覆盖产生相同块划分与路径指派；大小和修改时间只支持扫描缓存，不认证科学内容身份。

## 配置生成与执行

每块提供可编辑名称、亮场、实际校准输入、响应引用、独立 output_dir 与 WCS 等块级配置，字段形状以 normalize 合同和真实 parser 为准。master_* 只能引用对应 master，不能把一组原始校准帧路径直接当 master；如需生成 master，必须走已定义内核入口，GUI 不自行合成。

JSON 预览与保存使用相同模型，检查块间输出冲突。运行三命令可独立执行，完整队列显式把上一步产品路径与身份填入下一步配置，不假定输出 JSON 就是下一命令完整配置。预检界面复现当前 CLI 的诊断和兼容确认行为；-force 不绕过损坏 JSON、缺文件或不可计算几何。目标设计不新增研发审批门。

## 运行呈现

解析器按 UTF-8 行组装事件，处理分块读取、未知可选字段、坏行、run_id 与 sequence；记录原始流和进程退出。事件键以协议与 schema 为准，不在 GUI 另创 fraction、cpu_util、mem_rss 或 log kind。progress 展示已完成量与总量；resource 展示已实际提供的进程观测；backend 展示实际选择；artifact 登记路径与完整性信息；final 表示收尾。资源线程数不能直接标作忙碌 worker。日志通道与运行事件分开，stderr 不按 JSONL 强行解析。

尚无事件的能力显示未知或未提供，不能推造节点进度与资源读数。事件中断或缺 final 时标 interrupted，最后一次观测带时间与陈旧状态保留供诊断，不以清零伪装实时测量，也不把最后进度当完成。artifact 事件只是登记，打开产品仍按完成状态、清单与完整性验证。

取消请求通过正式宿主通道发送；当前组件是否完成该接线必须实测。只有退出与收尾证据支持取消时记 cancelled；超时强杀、通道不可用、非取消错误分别记录，不能用按钮点击替代成功取消。已发布的独立产品按原子发布合同保留，暂存和不完整对象隔离。

## 产物与部署验证

产物页复用 Qt6/OpenGL 浏览组件，提供球面与平面查看、元数据与溯源。显示拉伸只作用显示缓冲，测量读取原科学值；框选导出生成 export 的 crop 配置。组件的已有查看能力与完整 GUI 集成分别验收。

目标部署为独立 Qt 工程，使用 windeployqt 收集依赖、Windows 安装器并列交付命令行与 GUI。实际构建与材料范围见[发布说明](../build/RELEASE.md)，不将规划目录和安装器当现成产品。程序位置按安装布局解析，开发期允许显式覆盖并报告实际路径。

验证包含分组回放、字段缺失/冲突、规则错误与锁定、配置不可写、JSONL 分块/坏行/断流、取消失败、重复输出路径、错误码及显示不改原值。本轮仅文档与源码静态核对，未构建 GUI 或回放事件。旧任务编号、完整匹配表和事件草案保存于[来源档案](../../../实验/documentation-refactor/docs/GUI_SOURCE.txt)。
''',
 'docs/detail/infrastructure/hips_browser.md': r'''# HiPS 浏览组件

模块目录为 lib/infrastructure/hips_browser/，现有实现 healpix_browser_qt 位于其同名子目录。它是[Windows GUI](../../engineering/gui/GUI_DESIGN.md)可复用的 Qt6/OpenGL 浏览组件，独立构建，不进入科学命令计算链。产品 GUI 的发布目标由最高设计决定，不能用旧“Alpha 不含 GUI”限制当前设计。

## 输入、输出与数据流

输入为只读 HiPS 产品及元数据，科学 signal 与诊断辅助层按各自语义显示；文件格式能力不意味着 mosaic 应发布辅助科学通道。输出为屏幕图像、导航坐标、显示状态与诊断，显示变换不回写科学数组或改变测量值。产品和错误合同见[HiPS I/O](../../engineering/contracts/HIPS_IO.md)及[AIO](aio.md)。

读取/解码 tile → 缓存 → 球面到视口采样 → 浮点显示缓冲 → 显示变换 → Qt/OpenGL 绘制。STF 编辑应复用已有浮点视口，避免重做解码和几何采样；视点或产品变化才更新对应上游。GL 和界面对象在所属线程使用，后台任务只交付有明确寿命的数据，过期结果按代次丢弃。

## 真实入口与所有权

子目录 CMakeLists.txt 是独立工程；app/main.cpp、app/browser_cli.cpp 为各自入口，app/main_window.* 连接界面，widgets/ 维护视图。core/browser_backend.*、hips_browser_backend.*、hips_sky_view.*、gl_renderer.* 与 stf_engine.* 实现读取、采样与显示。其构建复用共享算法源，不能因此称为第二份独立科学 oracle。

core/stf_engine.h 的 DisplayTransformState 保存模式、锁定、黑白点、中间调、曲线、压缩和 generation。UI 编辑状态，renderer 消费；异步快照不越过持有对象寿命。Qt parent-child 只管理相应 QObject，原生缓冲与后台任务仍需各自所有权和退出等待。

## 显示有效域与实现限制

显示的目标是按有效覆盖/有限信号选样，合法负值和零值不能仅因符号被当无覆盖。不同 auto/global/view 路径需分别核对。当前 STFEngine::auto_stretch 实际按 data[i] > no_data_value 过滤，头文件默认阈值为零，再 sort 取分位与中位数；它不是旧正文所称的 finite && support>0、nth_element 和完整 MAD 守卫链。

因此该入口存在排除合法非正值、未显式排除正无穷的静态风险。是否由调用者预过滤、哪些显示路径可达，必须沿实际调用与输入验证；本轮登记问题，不修改显示算法。空样本与近零范围的默认参数也不能解释成观测成功。

signal 拉伸与 support 等诊断层显示分别定义；显示参数不是科学校准参数。坏产品、缺 tile、读取失败和不支持能力明确显示状态，不用伪图像冒充完整覆盖。坐标由共享 HEALPix 实现提供，屏幕坐标、像元基址及天球角单位在交互接口明确。

## 验证

用已知像素与坐标核对读取、导航、STF 参数、显示重绘缓存、陈旧任务丢弃、退出等待和源值不变；覆盖全负、含零、全无效、正无穷与极动态范围。真实渲染、截图和 Windows 集成分别提供证据。本轮未运行浏览器；旧 CLI 开关、性能和可选发行声明完整保存在[来源档案](../../../实验/documentation-refactor/docs/GUI_SOURCE.txt)，不能据原表宣称当前可达或已验证。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/GUI_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'GUI source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
