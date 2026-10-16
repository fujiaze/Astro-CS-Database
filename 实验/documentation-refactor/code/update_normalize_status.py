"""Preserve pre-update pages and synchronize static facts after remote merge."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CHANGES = {
 'docs/engineering/architecture/NORMALIZE_PIPELINE.md': [
  ('新增球面 precision 也主要经 Drizzle 局部向量与 staging 传给 writer，尚未成为注册端口上的独立模型载荷。', '球面 precision 经 Drizzle 局部向量与 staging 传给 writer，descriptor 与实际 CLI IR 已补 frame_science 输出/输入边；载体仍是 output_dir 工作文件，不等于 Runtime 已传输完整 typed 模型或完成消费者 lease。'),
  ('PSF 接线目前将全帧参数压为标量宽度，不能代表目标的逐位置像元积分模型。', 'PSF 已从逐帧 psf_params 构建空间三角模型，逐控制点求积；无三角支持时仍回退具名帧常宽度。空间 alpha 已按窗口求值，支持空洞、回退状态和总通量配对的合同覆盖仍需核对。'),
  ('typed Runtime、参考 SED 积分、空间 PSF/alpha 和共同误差的完成状态分别核对', 'typed Runtime、参考 SED 积分、空间模型的支持/误差状态、applied 空间 alpha 的真实运行和共同误差分别核对'),
 ],
 'docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md': [
  ('QR 主元门限不等于奇异值门限，背景投影后 I_F 仍以两个范数平方相减，近共线时有消去风险；主元比平方不等于谱条件数或保守上界。Jacobi 分支的负谱处理仍需单独核对', 'I_F 已改为显式残差两轮投影后取平方范数，保存响应/残差范数和 condition_estimate_method；旧范数平方相减问题不再描述该分支。QR 主元门限不等于奇异值门限，主元比平方仍不能仅凭方法名成为谱条件数或保守上界；通量残差平方与阈值的尺度、Jacobi 分支分别核对'),
  ('生产请求只赋标量 applied_scale；空间 pixel_scale 未贯通。`a_ref=Σalpha P` 使用当前窗口，而信息模板仍取 w.profile；在截断总通量/空间标度下的配对未被现有验证覆盖', '生产请求已有 pixel_alpha_at，经同一 photo_spatial_m 求值，再填每窗 pixel_alpha；优先于旧 cfg.pixel_scale 和常量。真实 applied 空间场的端到端证据不能由 alpha=1 的运行替代。`a_ref=Σalpha P` 仍使用当前窗口，信息模板取 w.profile；截断总通量/空间标度的配对问题见科学问题记录'),
  ('当前调用仍传帧级圆化中位 sigma，未贯通逐位置椭圆/旋转 PSF。相邻阶差仅作自检；每点包络和误差目前使用帧级极值，不能冒充该点真实诊断', '已消费逐帧 psf_params，PSF 库构建三角剖分并混合顶点积分响应；逐点包络/求积误差与帧摘要已经分离。当前模型无孔洞/跨无效区约束输入，凸包外/剖分失败仍回退帧常 sigma，不能由拟合成功行推定全部质量与空间支持合格；相邻阶差只作自检'),
  ('PSF 文件仍主要写 h，所需平面 P 未完整持久化；部分 support_note/representation_note 残留 v1，逐点诊断引用帧极值。精确因子仅对声明的对角平面模型精确，不补全遗漏相关项', 'PSF 文件仍主要写 h，平面 P 的独立持久化覆盖需核验；covariance support_note 已同步 v2，逐点 PSF 诊断已分开，不能沿用旧 v1/帧极值问题概括。空间顶点与权重当前多在 note 中且精度受格式化限制；精确因子仅对声明对角模型成立，不补全遗漏相关项'),
  ('重建函数只检查 sr 幂次，未完整核对基础线性单位。发布器的 hash 校验不能替代读侧调用', '重建函数已加入 expected_linear_unit，核对基础线性单位和平方指数；该函数修复不等于通用 reader 已调用它。发布器的 hash 校验不能替代读侧完整验证'),
  ('新增 canonical sidecar 不表示 Mosaic 已经消费该物理对象。', '新增 canonical sidecar 不表示 Mosaic 已经消费该物理对象。'),
 ],
 'docs/detail/normalize/contracts.md': [
  ('生产已调用 `pixel_integrated_moffat4_profile`，但帧级圆化宽度不等于真实逐位置空间 PSF。参考通量、Poisson 与 spatial alpha 仍须和积分模板配对。', '生产已调用 `pixel_integrated_moffat4_profile`，并通过 PSF 库 `build_spatial_model` / `spatial_pixel_response` 在控制点混合顶点积分响应；无空间定位时的帧常宽度仍是近似。空间 alpha 由 `PrecisionFrameRequest::pixel_alpha_at` 逐窗填入 `PlanarPatchWindow::pixel_alpha`，不是整帧常量字段。参考通量、Poisson、截断窗口与实际应用模板仍须配对。'),
  ('每控制点保存自身包络、参数、相位和积分误差，不能共享帧级极值冒充逐点记录。', '每控制点保存自身包络、参数、相位和积分误差，帧级极值只作摘要。当前逐点求积诊断已经分离；空间模型的全部顶点参数、有效支持和可独立重算精度仍须验证，不能只凭 note 中三角ID/低位权重推断持久化完整。'),
 ],
 'docs/engineering/architecture/DATA_FLOW.md': [
  ('符合 `mosaic` 输入格式', '可显式组装为 `mosaic` 输入；输出清单不自动等同完整运行配置'),
  ('容差数值与可满足性下限由测试标准规定[12]。', '比较配置与适用域由测试标准规定[12]；ULP通用下限和归约界疑点另行记录，不由该句保证所有算法满足。'),
  ('worker 数只来自 benchmark 生成的机器画像与全局预算对象。', 'worker 数由机器画像和全局预算对象解析；无有效画像使用已声明的保守基线，不能因缺画像拒绝合法输入。'),
  ('分配策略：串行 I/O 与控制面恒 1 线程；CPU 内核获得 `min(预算, 内核块提示)`；异步 I/O 流水线恒 1 专用线程；后台服务（看门狗、资源监控、进度日志）恒 1 线程加独立小预算，不入科学预算池。', '分配策略区分CPU计算、I/O和后台服务；CPU内核不超过租约与内核块提示，I/O池/队列按独立有界预算执行并纳入进程观测。当前AIO有多worker发布尾部实现，不能用“异步I/O恒1线程”的旧描述代替实际路径。'),
 ],
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/NORMALIZE_STATUS_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    # Validate every replacement before preserving or changing anything.
    updated = {}
    for name, pairs in CHANGES.items():
        body = (ROOT / name).read_text(encoding='utf-8')
        for old, new in pairs:
            assert body.count(old) == 1, (name, old)
            body = body.replace(old, new)
        updated[name] = body
    with archive.open('wb') as out:
        out.write(f'Normalize status sources; commit={revision}\n'.encode())
        for name in CHANGES:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in updated.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
