# AstroCS_CP0.zip 独立核验报告 (自检, 供审核人交叉验证)

包: /home/lighthouse/astrocs_audit_v2/cp0_out/AstroCS_CP0.zip
sha256(cp0 zip): c79f50d7cdf8e9348d96b516cdead13bf94b2c649612a0591c4240fcb84f8e82
解包后: SHA256SUMS 覆盖 18 文件, `sha256sum -c` exit 0. 包不改, 字节不变, 故本报告放包外. 生成时刻: 2026-08-27.

| 审核项 | 结果 | 证据 |
|---|---|---|
| ZIP 完整性 | OK | 解包成功, 18 文件, SHA256SUMS 全通过 |
| MANIFEST 一致 | OK | package_audit.py 生成; files=18, total_bytes 与 sum 一致, 无 set 不匹配 |
| SHA256SUMS 一致 | OK | 覆盖除自身外全部文件; sha256sum -c exit 0 |
| HEAD=main=origin/main | OK | identity.json: 三 SHA=535e73879662…, rev_parse_equal=true, remote 无凭据 |
| 外部 worktree 变化 | OK | EXTERNAL_WORKTREE_CHANGES.md: AGENTS.md(.M) + 3 untracked, 无来源不明修改 |
| 32R 11+11+10 | OK | data_manifest.csv: Red=32, panel1=11/panel2=11/panel3=10 |
| hash 唯一/无缺帧 | OK | 35/35 唯一 hash; 无重复、无缺 |
| 3 masters | OK | master_bias(64849152) / master_dark(64820480) / master_flat(64824576), hash 已记 |
| SUMMARY vs CSV 计数 | OK | task {NOT_STARTED:59,PASS:3}; finding {P0:3,P1:7,P2:16,P3:3}; test{}; build{} 全 MATCH |
| 禁入文件 | OK | 无 .fits/.xisf/.exe/.dll/.so/.a/.lib/.o/.obj/.pdb/.bundle/.tar/.tgz; 无 build/builds/out/cache/temp/.git |
| 绝对路径/凭据泄露 | OK | 扫描 CSV/JSON/MD/TXT: 0 处 F:/, C:/, /home/, /tmp/, http://user:pass@, token/password/secret |
| 严格停 CP0 | OK | PASS 仅 [ID-001,ID-002,ID-003]; 其余 59 均 NOT_STARTED; 状态仅 NOT_STARTED/PASS |
| 未 commit/push/建分支 | OK | git: 仅 main; HEAD=origin/main=535e738… 未见新增 commit (by 本审计); status 仅外部变化 |

注: 数据不出包 — data_manifest/LARGE_ARTIFACT_MANIFEST 仅记录相对路径/大小/hash, 原始 FITS/XISF 仍在 Fatduck; 包内无大产物.
结论: 满足审核人清单; 但最终验收由审核人裁决, 当前状态 **CP0_AWAITING_EXTERNAL_REVIEW**.
