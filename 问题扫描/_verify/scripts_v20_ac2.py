
import re
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
def at(ln, sub):
    ok = sub in ma[ln-1]
    print('%5d %-40s %s' % (ln, sub[:40], 'OK' if ok else 'DRIFT -> %s' % [i+1 for i,l in enumerate(ma) if sub in l][:3]))
for ln,sub in [(1121,'p1_has'),(1134,'p1_int'),(1158,''),(4186,'frame_slots'),(4187,'sample_mask_offset'),(4786,'ASTROCS_REJECT_PROFILE'),(2662,'int snr_max_sources'),(2666,'sci_cfg.gain_e_per_adu'),(2968,'precision_mode'),(4845,'input_lights'),(4857,'cosmetic'),(4912,'p.min_workers'),(4914,'p.max_workers'),(3587,'__workers'),(3743,'std::max(1, doc.value("__workers"'),(5160,'cfg2["__workers"]'),(3729,'P2UpmBuildConfig uc'),(1462,'k_use'),(1551,'ac_correct_frame'),(2347,'ipv'),(3534,'filter_passband'),(3313,'filter_passband')]:
    at(ln,sub)
