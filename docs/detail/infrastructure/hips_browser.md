# HiPS 浏览组件

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
