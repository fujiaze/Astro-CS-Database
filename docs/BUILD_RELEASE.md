# AstroCS 构建与发布 (V19)

## 发布流程

```text
1. toolchain.ps1 check
2. toolchain.ps1 build (全部模块)
3. 科学/质量 gate 全过
4. toolchain.ps1 review -Topic <主题> 生成审核包
5. 验证审核包 SHA256SUMS
6. commit + push (最小任务粒度)
```

## V19 发布状态

```text
PRE_RELEASE_FOUNDATION_READY       (V19 目标)
FINAL_REAL_DATA_VALIDATION=PENDING_V20
PRE_RELEASE_CANDIDATE_READY        (V20 后才签)
```

## 交付物

```text
AstroCS_Review_PreReleaseFoundation_V19.zip
  README.md / SHA256SUMS.txt
  reports/ (13 份) / evidence/ / self_review/ (round0-6)
  source/ (full first-party after.zip + manifest)
  docs_snapshot/ (全部 pre-release 文档)
```

不携带 BASS 大图/build/vendor。
