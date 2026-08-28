#!/usr/bin/env python3
"""V5 审阅胶囊生成器: 每个 main commit 一个 capsule。
用法: python3 tools/make_capsule.py <task_id> <commit_sha> <verdict_note>
输出: artifacts/prerelease_v5/capsules/<task_id>_<commit12>.zip
"""
import json, os, subprocess, sys, hashlib, zipfile, datetime

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_BYTES = 2_000_000  # 单文件上限, 超出只登记 hash

def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True).stdout

def main():
    task_id, commit, note = sys.argv[1], sys.argv[2], (sys.argv[3] if len(sys.argv) > 3 else "")
    commit = commit.strip()
    c12 = commit[:12]
    parent = git("rev-parse", f"{commit}^").strip()
    subject = git("log", "-1", "--format=%s", commit).strip()
    files = git("diff-tree", "--no-commit-id", "--name-only", "-r", commit).split()
    patch = git("diff-tree", "-p", "--no-commit-id", commit)
    outdir = os.path.join(REPO, "artifacts", "prerelease_v5", "capsules")
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
    main()
