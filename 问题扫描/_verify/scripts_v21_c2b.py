
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
targets = ['n_contrib','vote_entries','n_iter','n_cand','write_ops','read_ops','alloc_balance','valid_pixels','total_signal_avg','snr_rows','rejected_lo','rejected_hi','read_fail','n_grid','bad_res','bad_exp','skipped_degenerate','n_nan_mag','n_drop','n_coef','completed_units','combo_count','callback_calls','leaf_ops','sparse_volume_bytes','bitmap_volume_bytes','full_volume_bytes','padding_bytes','working_bytes','valid_total','snr_samples']
loc = collections.defaultdict(list)
for f in files:
    try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        for t in targets:
            if re.search(r'\b'+t+r'\b', L):
                loc[t].append((f,i,L.strip()[:110]))
for t in targets:
    ls = loc[t]
    ws = [x for x in ls if re.search(r'\b'+t+r'\b\s*(=|\+\+|\+=|--|-=)', x[2]) and '==' not in x[2]]
    rs = [x for x in ls if x not in ws]
    print('###', t, 'total=%d W=%d R=%d'%(len(ls),len(ws),len(rs)))
    for f,i,s in ls[:14]: print('   ', f+':'+str(i), s)
