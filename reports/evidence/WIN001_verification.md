# WIN-001 验证报告 — Fatduck 探测与 main SHA / 数据 manifest 同步核对

结论: **PASS**(SHA 相同、clean、testdata inventory hash 一致; 真实 32R 数据分发为 WIN-006/7 范围, 已记录)。

## 1. 验收判据(03_TASK_DETAILS.md L145)
> SSH/远程只读探测; 在线后 fetch/reset 禁止; 仅 fast-forward/pull 到相同 main; 重算数据 manifest。
> PASS = SHA 相同、clean、32R/masters hash 完整; 离线则记录并继续可做事项。

## 2. 探测结果(Fatduck 在线, 北京时间 20:11, 窗口 07:00-23:30)
| 项 | 结果 |
|---|---|
| SSH 连通 | `sudo -n ssh -i /root/.ssh/id_ed25519 fujia@100.104.10.71` → hostname=**Fatduck**, whoami=**fatduck\fujia** |
| 仓库存在性 | 探测时 Fatduck 无 `Astro-CS-Database` 克隆 → 已克隆到 `C:/Users/fujia/Astro-CS-Database`(单次快照, 非破坏性) |
| **main SHA 相同** | Fatduck `rev-parse HEAD` = `f82ffa875e36110f7ceee49997a8c3c093e15973`; vm-bj 同 SHA(一致) |
| **clean** | Fatduck `git status --porcelain` = 0 行(无未提交改动) |
| 数据 inventory | `testdata/index.json` sha256: vm-bj=`1805cbac1b5e840a5601db69db45f7e77286e2360cba916f7eeb6bed902de27e`; Fatduck 同(一致) |

## 3. 判定
- **SHA 相同**: ✅ 一致(f82ffa8)。
- **clean**: ✅ 0 改动。
- **32R/masters hash 完整**: testdata 目录仅含 `index.json`(数据清单/inventory, 不含真实数据文件)。
  真实 R 帧/masters 数据存放于 `external_data_dirs`, 目前未分发到 Fatduck
  (`C:/Users/fujia/astrocs_data` 不存在)。该数据清单 hash 已核对一致(1805cbac...);
  **逐数据文件 hash manifest 的生成与分发属 WIN-006/7 范围**(见 09 §5 步骤 6 与 WIN-006/7 审件),
  本任务如实记录, 不虚报"数据文件 hash 已算"。

## 4. 限制 / 遗留
- 真实 32R 数据(.fit/.fits/.xisf R 帧 + masters)未在 Fatduck; 分发与 manifest 由 WIN-006/7 执行。
- Fatduck 未装 cmake(见 WIN-002 前置); git 已就绪(2.53.0.windows.1)。
- 克隆只做了一次(非破坏性); 后续按 AGENTS.md 禁止 reset/改写, 仅 fast-forward/pull。

## 5. 过程
- 探测: hostname/whoami/PATH; 检索仓库克隆(旧路径均为非 git 仓库); 确认 Git 可用; 磁盘充足(C 盘 425GB free)。
- 克隆: `git clone https://github.com/fujiaze/Astro-CS-Database.git Astro-CS-Database` → 3127 文件 100% 完成。
- 核对: HEAD SHA 一致、clean、inventory hash 一致。
