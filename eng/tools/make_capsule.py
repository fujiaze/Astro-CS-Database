#!/usr/bin/env python3
"""V5 审阅胶囊生成器: 每个 main commit 一个 capsule。
用法: python3 eng/tools/make_capsule.py <task_id> <commit_sha> <verdict_note>
输出: capsules/<task_id>_<commit12>.zip（旧证据树）

退役 (RETIRED) —— 本工具属旧世代(V5 控制包)逐提交审阅胶囊生成器, 已退役:
  1. 审阅胶囊是旧世代控制包(V5/REV-002)的交付形态（历史版本控制包全部作废）,
     且胶囊产物只能落旧证据树 capsules/(该树已整体删除)
     —— 新世代不归档 (docs/ASTROCS_DESIGN.md §0 权威链、§12 版本信息下线);
  2. 实测: 无参调用即未捕获 IndexError; 带参调用虽 rc=0, 但把 zip 写回**已退役的**
     capsules/ —— 属 ENGINEERING_SPEC.md §8 禁止的「静默坏掉/僵尸入口」;
  3. 活动替代: 无。逐提交复核由 git 历史自身承担(git show / git log), 正式证据按 §7 落位;
  4. 文件保留原因: eng/tests/cli/test_iso_acr_gpu_isolation.py::test_04 仍静态扫描本文件路径,
     故**不删文件**(AGENTS.md §5 禁止删本体)。
  复原命令 (内容未丢): git 历史
  退役后行为: 任意调用打印 MAKE_CAPSULE_RETIRED 说明并 exit 2 (fail-closed, 不伪装绿)。
  登记见 docs/ci/01_CHECKS.md §2.2。
"""
import json, os, subprocess, sys, hashlib, zipfile, datetime

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAX_BYTES = 2_000_000  # 单文件上限, 超出只登记 hash

def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True).stdout

RETIRED_NOTICE = (
    "MAKE_CAPSULE_RETIRED: 本工具（旧世代 V5 逐提交审阅胶囊生成器）已退役。\n"
    "  依据: docs/ASTROCS_DESIGN.md §0（权威链：旧世代控制包产物不构成判据）"
    "（历史版本控制包全部作废；不归档不保留）；"
    "ENGINEERING_SPEC.md §8（不允许静默坏掉）。\n"
    "  输入/输出已不存在: 输出目录（旧证据树 capsules/）已删除。\n"
    "  复原命令: git 历史\n"
    "  登记: docs/ci/01_CHECKS.md §2.2"
)


def main():
    """退役后入口：显式失败（fail-closed，禁止 traceback 崩溃，不伪装绿）。"""
    print(RETIRED_NOTICE, file=sys.stderr)
    return 2


def legacy_main():
    """退役保留实现（复用方法：按复原命令取回旧世代控制包后可直接复跑）。"""
    task_id, commit, note = sys.argv[1], sys.argv[2], (sys.argv[3] if len(sys.argv) > 3 else "")
    commit = commit.strip()
    c12 = commit[:12]
    parent = git("rev-parse", f"{commit}^").strip()
    subject = git("log", "-1", "--format=%s", commit).strip()
    files = git("diff-tree", "--no-commit-id", "--name-only", "-r", commit).split()
    patch = git("diff-tree", "-p", "--no-commit-id", commit)
    # 陈旧根级历史证据目录已清零；本工具已退役
    # （见文件头 RETIRED），常量与文件头声明的当前证据树一致。
    outdir = os.path.join(REPO, "artifacts", "evidence", "prerelease-v5", "capsules")
    os.makedirs(outdir, exist_ok=True)
    zpath = os.path.join(outdir, f"{task_id}_{c12}.zip")
    capsule = {
        "task_id": task_id, "commit": commit, "parent": parent, "subject": subject,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "host": "vm-bj Linux amd64", "branch": "main", "verdict_note": note,
        "changed_files": files,
    }
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("CAPSULE.json", json.dumps(capsule, ensure_ascii=False, indent=1))
        z.writestr("commit.patch", patch)
        z.writestr("changed_files.txt", "\n".join(files) + "\n")
        hashes = [("CAPSULE.json", hashlib.sha256(json.dumps(capsule, ensure_ascii=False, indent=1).encode()).hexdigest()),
                  ("commit.patch", hashlib.sha256(patch.encode()).hexdigest())]
        for f in files:
            full = os.path.join(REPO, f)
            if not os.path.isfile(full):
                continue  # 本 commit 内删除的文件
            data = open(full, "rb").read()
            name = f"files/{f}"
            hashes.append((name, hashlib.sha256(data).hexdigest()))
            if len(data) <= MAX_BYTES:
                z.writestr(name, data)
            else:
                z.writestr(name + ".TRUNCATED_REGISTERED_ONLY", "")
        sums = "\n".join(f"{h}  {n}" for n, h in hashes)
        z.writestr("SHA256SUMS", sums + "\n" + hashlib.sha256(sums.encode()).hexdigest() + "  SHA256SUMS\n")
    print(zpath, os.path.getsize(zpath))

if __name__ == "__main__":
    sys.exit(main())
