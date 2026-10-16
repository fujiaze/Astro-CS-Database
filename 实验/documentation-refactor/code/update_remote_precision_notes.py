"""Synchronize implementation facts without adopting a new scientific convention."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CHANGES = {
 'docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md': [
  ('真实 applied 空间场的端到端证据不能由 alpha=1 的运行替代。`a_ref=Σalpha P` 仍使用当前窗口，信息模板取 w.profile；截断总通量/空间标度的配对问题见科学问题记录', '已有真实applied空间场运行记录，不能再统称只有alpha=1证据；本轮只查阅日志/改动，未重跑。当前实现以位置alpha_star定义模板(α/α_star)P与源泊松项，窗口sum仅诊断；该约定与科学正本的完整响应总通量归一须分开，见Q35，不以实现反向改写定义'),
  ('PSF 文件仍主要写 h，平面 P 的独立持久化覆盖需核验；covariance support_note 已同步 v2，逐点 PSF 诊断已分开，不能沿用旧 v1/帧极值问题概括。空间顶点与权重当前多在 note 中且精度受格式化限制；精确因子仅对声明对角模型成立，不补全遗漏相关项', 'PSF载荷已新增planar_template、source_pixel_alpha与reference_alpha_star，读写函数均有对应处理；模板实际为位置标度约定。covariance note为v2、逐点诊断分开。完整顶点参数/支持、模型通用reader接线和外部可求值仍须核对，不能由新增数组声明独立读取全部完成'),
  ('生产 fdoc 只写 scale_applied/status，没有赋 application_id；因此已标度分支不能由 native 单帧记录推断已成功发布', '生产fdoc已在frame_applied分支生成application_id/response_id，并有真实applied记录。ID目前将标量k用std::to_string、响应用库键/路径组装；极小标度的字符串精度与完整曲线内容绑定仍有静态风险，不能把ID非空当无损身份'),
 ],
 'docs/engineering/architecture/NORMALIZE_PIPELINE.md': [
  ('空间 alpha 已按窗口求值，支持空洞、回退状态和总通量配对的合同覆盖仍需核对。', '空间alpha已按窗口求值并有真实applied运行记录。当前模板改用位置alpha_star约定，支持空洞、回退状态与科学总通量定义仍需核对，不能把新接线等同完整合同。'),
 ],
 'docs/detail/normalize/contracts.md': [
  ('单点求积正确不等于完整跨设备参考精度已验证。', '单点求积正确不等于完整跨设备参考精度已验证。当前实现还新增alpha_at_xy、reference_alpha_star及planar_template=(alpha/alpha_star)P，窗口sum只作诊断；这里报告实际接口，不替换数据合同中按完整响应域归一的科学定义。两种参考量的正式统一见Q35。'),
 ],
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/REMOTE_PRECISION_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    updated = {}
    for name, pairs in CHANGES.items():
        body = (ROOT / name).read_text(encoding='utf-8')
        for old, new in pairs:
            assert body.count(old) == 1, (name, old)
            body = body.replace(old, new)
        updated[name] = body
    with archive.open('wb') as out:
        out.write(f'Remote precision note sources; commit={revision}\n'.encode())
        for name in CHANGES:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in updated.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
