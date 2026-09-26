import pathlib
BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
P = BASE / 'prompts'
tmpl = (P / 'template.txt').read_text(encoding='utf-8')
chain = (chr(10) + chr(10) +
  '【链条定位·负责人定案】全项目是一条科学链上五个创新点、相互纠缠（最高设计 §2 已按此重排）：'
  'P1 通量积分拟合（把所有信号拟合进测光星等坐标系，信号处于同一平面，根基）；'
  'P2 跨帧绝对SNR（跨帧可用、不依赖参考帧）；'
  'P3 算子类创新（平面到 HEALPix 的通量绝对守恒 drizzle；稀疏 SNR 控制点同经球面映射上球）；'
  'P4 重建稠密SNR（基于负责人噪声信号模型，把稀疏控制点重建为稠密 SNR 场）；'
  'P5 加性天光去除（用精确 SNR 加权去天光并叠加，消费 P2/P4 与 P1 的统一平面）。'
  '你负责的模块在链上的上游输入与下游消费必须写成实验设计的一部分：'
  '至少一个用例贯穿上下游接口（上游给什么量、下游拿去做什么、量纲/精度/有效性约定），'
  'report.md 单列【链条位置】一节。')
mods = {
 'P1': ('P1通量积分拟合', '①',
        'docs/science/PHOTOMETRY.md（测光标定语义与 k_photo 正本）', 'review-05-①-科学性'),
 'P2': ('P2跨帧绝对SNR', '②',
        'docs/science/NOISE_MODEL.md（噪声信号模型正本，参考分解与逐项方差）', 'review-05-②-科学性'),
 'P3': ('P3守恒映射算子', '④',
        'docs/algorithms/DRIZZLE_GEOMETRY.md（交叠分配与通量闭合门）与 ASTROCS_DESIGN.md §2.3', 'review-05-④-科学性'),
 'P4': ('P4重建稠密SNR', '②',
        'ASTROCS_DESIGN.md §2.4（重建稠密信噪比：必须带亮度、三口径重建算子、w=SNR²/F_ref²=1/σ_F²）与 docs/science/NOISE_MODEL.md §5b、docs/science/CONTROL_WEIGHT_SNR.md §2b', 'review-05-②-科学性'),
 'P5': ('P5加性天光去除', '③',
        'docs/science/PHASE2_UPM.md 与 docs/plugins/algorithms_phase2/11_upm.md（UPM 与接缝判据正本）', 'review-05-③-科学性'),
}
MAXR = '--dangerously-skip-permissions --reasoning-effort max --context-window 393216'
for tag, (d, key, extra, rev) in mods.items():
    for n in (1, 2, 3):
        t = tmpl.replace('{mod}', d).replace('{key}', key).replace('{n}', str(n)).replace('{extra}', extra)
        t = t + chain.replace('你负责的模块', '本模块（' + tag + ' ' + d[2:] + '）')
        (P / ('research-%s-%d.txt' % (tag, n))).write_text(t, encoding='utf-8')
print('prompts regenerated:', len(mods) * 3)
