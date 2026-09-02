#!/usr/bin/env python3
"""DATA-003 生产 ArtifactStore（执行形态；权威文档形态 = docs/interfaces/data/
DATA-003_PRODUCTION_ARTIFACT_STORE.md + 本目录 README.md）。

冻结语义（tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-003 + 约束 A.3/A.4/A.6 +
DATA-001 manifest 合同 + DATA-002 交换资格）:
  - Runtime 启动真实 Store（ArtifactStore(root, run_id).start()，run 私有）；
    模块 `execute` 只能拿已校验 handle/reader/writer —— 写侧 Writer（stage 后
    publish 返回 Digest），读侧 bind_as_input/read_manifest 返回 ManifestRead、
    read_verified 返回 bytes；绝不把裸文件路径交给模块（R-NO-NAME-BINDING，
    13_DATA_PIPELINE_AND_ARTIFACT_STANDARD §1 "Pipeline edge 传递 ArtifactHandle，
    不是路径字符串"；storage_uri → 真实路径解析只发生在 Store 内部）；
  - 写路径 = 临时对象 → 完整校验（DATA-001 manifest validator + 内容 hash 核对）→
    sha256 → 原子 publish（fsync + rename）；成功才写 COMPLETE manifest 与
    manifest-hash sidecar；cancel/fail（未 publish / writer.release / 进程中断 /
    磁盘满）→ 无成功对象（无 COMPLETE manifest，不索引、不可消费、可恢复）；
  - 唯一 producer：同 artifact_id 二次 publish → 硬失败（DATA-001 唯一 producer）；
  - manifest 顶层 content_digest 与 manifest_digest 均为 sha256/64hex；manifest
    发布物保持严格 DATA-001 schema 形态（additionalProperties=false，不附加
    字段），manifest hash 以 sidecar `{aid}.manifest.sha256` 持久化 → 可重算
    （manifest_hash_recompute 与 sidecar 一致）。

本文件为纯 Python 执行语义（Linux 控制/轻合成节点可完整验证）；Windows 正式
DLL 交付由 DATA-003 同语义 C 接线复刻（Store 内部解析 storage_uri，句柄不跨
DLL）。科学公式/常数不改。
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import re
import sys
import time
from typing import Any, Dict, Optional

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from artifact_manifest_validator import Validator as ManifestValidator  # noqa: E402

_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_COMPLETE = "COMPLETE"


def sha256_bytes(data: bytes) -> str:
    """字节内容 sha256 hex（64 小写）。与 DATA-001 manifest content_digest 语义一致。"""
    return hashlib.sha256(data).hexdigest()


def canonical_manifest_json(doc: Dict[str, Any]) -> str:
    """manifest 规范 JSON：ensure_ascii=False、键序稳定、无多余空白。

    供 manifest hash（sidecar）与 manifest_hash_recompute 使用；json.dumps 的
    dict 键序 = 插入序，本仓库所有 manifest 构造均按 DATA-001 schema 冻结字段序
    构建 → 重算稳定（DATA-001/002 示例同样按冻结序）。
    """
    return json.dumps(doc, ensure_ascii=False, sort_keys=False,
                      separators=(",", ":"))


def utc_now_z() -> str:
    """UTC RFC3339 秒精度（YYYY-MM-DDTHH:MM:SSZ；与 manifest created_utc 同式）。"""
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


# ─────────────────────────────────────────────────────────────────────────────
# reader / writer 抽象（模块 execute 只能拿已校验 handle；无裸文件路径）
# ─────────────────────────────────────────────────────────────────────────────


class Digest:
    """已校验内容的只读摘要句柄（模块可安全持有；不含文件系统路径语义）。"""

    def __init__(self, hex_: str, size: int) -> None:
        if not isinstance(hex_, str) or not _HEX64.match(hex_):
            raise ValueError("digest hex must be 64 lowercase hex chars")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ValueError("digest size must be non-negative integer")
        self.hex = hex_
        self.size = size

    def to_manifest_digest(self) -> Dict[str, Any]:
        return {"algorithm": "sha256", "hex": self.hex}

    def __repr__(self) -> str:  # pragma: no cover - debug only
        return f"Digest(hex={self.hex[:12]}…, size={self.size})"


class ManifestRead:
    """已校验 manifest 的只读句柄（DATA-001 manifest 文档形态；storage_uri 仅身份）。

    storage_uri 只作为身份字段透传（绝不解析为文件路径语义——R-NO-NAME-BINDING）。
    """

    def __init__(self, doc: Dict[str, Any]) -> None:
        self._doc = dict(doc)

    def get(self, key: str, default: Any = None) -> Any:
        return self._doc.get(key, default)

    def artifact_id(self) -> str:
        return str(self._doc.get("artifact_id", ""))

    def content_digest_hex(self) -> str:
        cd = self._doc.get("content_digest", {})
        return str(cd.get("hex", "")) if isinstance(cd, dict) else ""

    def status(self) -> str:
        return str(self._doc.get("status", ""))

    def to_dict(self) -> Dict[str, Any]:
        return dict(self._doc)

    def __repr__(self) -> str:  # pragma: no cover - debug only
        return f"ManifestRead(artifact_id={self.artifact_id()!r})"


class Writer:
    """Store 授予的临时对象 writer（模块 execute 只经此写内容；publish 前无成功对象）。

    写路径: stage_bytes → Store 完整校验 + hash → publish 原子发布（返回 Digest）。
    cancel/fail 时 writer.release() → 临时对象随 Store.cleanup 清除，无成功对象。
    """

    def __init__(self, store: "ArtifactStore", artifact_id: str,
                 dir_: pathlib.Path) -> None:
        self._store = store
        self._artifact_id = artifact_id
        self._dir = dir_
        self._published = False
        self._released = False

    @property
    def artifact_id(self) -> str:
        return self._artifact_id

    def stage_bytes(self, data: bytes) -> None:
        """把内容写入 Store 拥有的临时对象（未发布；无成功对象）。"""
        self._ensure_writable()
        tmp = self._dir / f"{self._artifact_id}.tmp"
        with self._store.io.stage_open(tmp) as f:
            f.write(data)
            self._store.io.fsync_file(f)

    def stage_file(self, src: pathlib.Path) -> None:
        """把外部文件内容复制进 Store 临时对象（模块不直接把外部路径交给 reader）。"""
        self._ensure_writable()
        data = pathlib.Path(src).read_bytes()
        self.stage_bytes(data)

    def _ensure_writable(self) -> None:
        if self._published:
            raise RuntimeError("writer already published (unique producer)")
        if self._released:
            raise RuntimeError("writer released (cancel/fail): no success object")
        if not self._dir.is_dir():
            raise RuntimeError("writer stage dir gone (run ended/cancelled)")

    def release(self) -> None:
        """cancel/fail 路径: 释放 writer 并通知 Store 中止该写会话。

        中止后 publish/stage_manifest 一律拒绝 → 无成功对象；暂存 manifest
        立即清除，临时对象由 Store.cleanup 清理。
        """
        if self._released:
            return
        self._released = True
        self._store.abort(self._artifact_id)
        return None


# ─────────────────────────────────────────────────────────────────────────────
# Store I/O 层（spy 证明每读写经 Store；直读磁盘绕过 → 无成功对象）
# ─────────────────────────────────────────────────────────────────────────────


class StoreIO:
    """真实 Store 磁盘 I/O 后端。

    全部真实文件打开都经本层（stage_open/read_bytes/atomic_publish），供 spy
    包装计数。storage_uri → 真实路径的解析只发生在本层内部（DATA-001 冻结语义：
    "解析 storage_uri 为真实文件系统路径只允许发生在 Store 实现内部"）。
    """

    def stage_open(self, tmp_path: pathlib.Path) -> Any:
        return open(tmp_path, "wb")

    def read_bytes(self, path: pathlib.Path) -> bytes:
        return pathlib.Path(path).read_bytes()

    def fsync_file(self, f: Any) -> None:
        f.flush()
        os.fsync(f.fileno())

    def atomic_publish(self, tmp_path: pathlib.Path, final_path: pathlib.Path) -> None:
        # 原子 rename（同文件系统内）；publish 前 fsync 已完成
        os.replace(tmp_path, final_path)


class SpyStoreIO(StoreIO):
    """spy Store I/O：记录每次真实读/写经 Store 的字节事件（验收 spy 证明）。

    模块读/写全部经 Store 方法 → 事件被 spy 记录；绕过 Store 直读磁盘的内容
    不在 Store 校验/索引内 → 无成功对象。
    """

    def __init__(self) -> None:
        self.reads: list[Dict[str, Any]] = []
        self.writes: list[Dict[str, Any]] = []
        self.publishes: list[Dict[str, Any]] = []

    def stage_open(self, tmp_path: pathlib.Path) -> Any:
        self.writes.append({"kind": "stage_open", "path": str(tmp_path),
                            "utc": utc_now_z()})
        return super().stage_open(tmp_path)

    def read_bytes(self, path: pathlib.Path) -> bytes:
        self.reads.append({"kind": "read_bytes", "path": str(path),
                           "utc": utc_now_z()})
        return super().read_bytes(path)

    def atomic_publish(self, tmp_path: pathlib.Path, final_path: pathlib.Path) -> None:
        self.publishes.append({"kind": "atomic_publish",
                               "from": str(tmp_path), "to": str(final_path),
                               "utc": utc_now_z()})
        return super().atomic_publish(tmp_path, final_path)


# 磁盘满/进程中断故障注入后端（负测用；真实执行不注入）
class FailingIO(StoreIO):
    """在指定阶段抛 OSError(ENOSPC) 的故障注入后端（负测：磁盘满/中断）。"""

    def __init__(self, fail_on: str = "stage_open") -> None:
        self.fail_on = fail_on

    def stage_open(self, tmp_path: pathlib.Path) -> Any:
        if self.fail_on == "stage_open":
            raise OSError(28, "No space left on device")
        return super().stage_open(tmp_path)

    def atomic_publish(self, tmp_path: pathlib.Path, final_path: pathlib.Path) -> None:
        if self.fail_on == "atomic_publish":
            raise OSError(28, "No space left on device")
        return super().atomic_publish(tmp_path, final_path)


class InterruptIO(StoreIO):
    """模拟进程中断: publish 过程中（内容 rename 前）抛 KeyboardInterrupt/SystemExit。

    结果: 内容/临时 manifest 已 fsync 但未 rename → 无 COMPLETE manifest；新
    Store 实例 start() 不索引 → 可恢复。
    """

    def atomic_publish(self, tmp_path: pathlib.Path, final_path: pathlib.Path) -> None:
        raise KeyboardInterrupt("process interrupted mid-publish")


# ─────────────────────────────────────────────────────────────────────────────
# 生产 ArtifactStore
# ─────────────────────────────────────────────────────────────────────────────


class ArtifactStore:
    """DATA-003 生产 ArtifactStore：真实目录 + 原子发布 + 唯一 producer + 校验读。

    约定:
      - store_root 下每 run 独立目录（RT-002 run 私有 Store）：
        {root}/runs/{run_id}/objects/{artifact_id}              = 已发布内容
        {root}/runs/{run_id}/stage/                             = 临时对象（未发布）
        {root}/runs/{run_id}/manifests/{artifact_id}.manifest.json = COMPLETE manifest
        {root}/runs/{run_id}/manifests/{artifact_id}.manifest.sha256 = manifest hash sidecar
      - 模块 execute 的读写全部经本 Store；storage_uri 解析只在本 Store 内。
      - content digest = 发布内容字节 sha256（DATA-001 manifest.content_digest）；
        manifest hash = manifest 规范 JSON 的 sha256（sidecar，可重算）。
    """

    def __init__(self, store_root: pathlib.Path, run_id: str,
                 io: Optional[StoreIO] = None) -> None:
        if not isinstance(run_id, str) or not run_id:
            raise ValueError("run_id required non-empty")
        self.run_id = run_id
        self.store_root = pathlib.Path(store_root)
        self.io = io if io is not None else StoreIO()
        self._objects: Dict[str, pathlib.Path] = {}
        self._manifests: Dict[str, Dict[str, Any]] = {}
        self._manifest_digests: Dict[str, str] = {}
        self._run_dir = self.store_root / "runs" / run_id
        self._stage_dir = self._run_dir / "stage"
        self._objects_dir = self._run_dir / "objects"
        self._manifests_dir = self._run_dir / "manifests"
        self._aborted: set[str] = set()

    # ── Runtime 启动 ──
    def start(self) -> "ArtifactStore":
        """初始化真实 Store 目录结构（run 私有；恢复时只索引成功对象）。

        恢复语义（RT-007/RT-002）: 只索引 内容 + COMPLETE manifest + hash sidecar
        三者齐全的已发布对象；无 COMPLETE manifest 的残留（中断/磁盘满/cancel）
        = 无成功对象，不索引、不允许消费（可恢复：新 run/新 Store 重发）。
        """
        self._stage_dir.mkdir(parents=True, exist_ok=True)
        self._objects_dir.mkdir(parents=True, exist_ok=True)
        self._manifests_dir.mkdir(parents=True, exist_ok=True)
        for mf in sorted(self._manifests_dir.glob("*.manifest.json")):
            try:
                doc = json.loads(mf.read_text(encoding="utf-8"))
            except Exception:
                continue  # 损坏 manifest 不是成功对象
            if not isinstance(doc, dict) or doc.get("status") != _COMPLETE:
                continue  # cancel/fail 留下的 manifest 不是成功对象
            aid = doc.get("artifact_id")
            if not isinstance(aid, str) or not aid:
                continue
            obj = self._objects_dir / aid
            sha = self._manifests_dir / f"{aid}.manifest.sha256"
            if not obj.is_file() or not sha.is_file():
                continue  # 缺内容或缺 sidecar → 非完整成功对象
            self._objects[aid] = obj
            self._manifests[aid] = doc
            self._manifest_digests[aid] = sha.read_text(encoding="utf-8").strip()
        return self

    # ── 取消/中止（cancel/fail → 无成功对象） ──
    def abort(self, artifact_id: str) -> None:
        """中止一次写会话（writer.release 调用）。

        cancel/fail 后该 artifact 的暂存 manifest 不再可发布；publish 拒绝。
        已成功发布的成功对象（_objects 内）不撤销（唯一 producer 已完成）。
        """
        if artifact_id in self._objects:
            return  # 已成功发布的对象不撤销
        self._aborted.add(artifact_id)
        # 清除暂存 manifest（内存态），临时对象留待 cleanup 清理
        self._manifests.pop(artifact_id, None)
        return None

    # ── 写路径（模块 execute 只经此） ──
    def new_writer(self, artifact_id: str) -> Writer:
        """模块 execute 唯一写入口：Store 授予临时 writer。

        同 artifact_id 已发布（唯一 producer）→ 直接拒绝再写。
        """
        if not isinstance(artifact_id, str) or not artifact_id:
            raise ValueError("artifact_id required non-empty")
        if artifact_id in self._objects or artifact_id in self._manifests:
            raise RuntimeError(
                f"duplicate artifact id (unique producer): {artifact_id}")
        if artifact_id in self._aborted:
            raise RuntimeError(
                f"artifact {artifact_id} aborted (cancel/fail): no success object")
        return Writer(self, artifact_id, self._stage_dir)

    def stage_manifest(self, artifact_id: str,
                       manifest_doc: Dict[str, Any]) -> ManifestRead:
        """模块把完整 DATA-001 manifest 交给 Store 暂存（publish 时原子落盘）。

        本方法只暂存 + 完整校验（DATA-001 schema 层全拒: 缺字段/NaN/重复
        producer/未知 type/非法 hash/storage_uri 裸路径）；真正发布仍以原子
        publish 落盘，cancel/fail 不会产生 COMPLETE manifest。
        """
        if artifact_id in self._objects or artifact_id in self._manifests:
            raise RuntimeError(
                f"duplicate artifact id (unique producer): {artifact_id}")
        if artifact_id in self._aborted:
            raise RuntimeError(
                f"artifact {artifact_id} aborted (cancel/fail): no success object")
        if not isinstance(manifest_doc, dict):
            raise ValueError("manifest must be a JSON object")
        ok, errs = ManifestValidator().validate_doc(manifest_doc)
        if not ok:
            raise ValueError("manifest invalid: " + "; ".join(errs))
        if manifest_doc.get("artifact_id") != artifact_id:
            raise ValueError(
                f"manifest artifact_id {manifest_doc.get('artifact_id')!r} "
                f"!= writer artifact_id {artifact_id!r}")
        if manifest_doc.get("status") != _COMPLETE:
            raise ValueError(
                f"publish requires status=COMPLETE, got "
                f"{manifest_doc.get('status')!r} (cancel/fail → 无成功对象)")
        self._manifests[artifact_id] = dict(manifest_doc)
        return ManifestRead(dict(manifest_doc))

    def publish(self, artifact_id: str) -> Digest:
        """原子发布暂存对象 + manifest + hash sidecar（唯一 producer）。

        顺序: 内容已在 stage_bytes 时 fsync；此处计算内容 sha256 → 与 manifest
        content_digest 核对（完整校验，篡改/错误 schema 拒绝）→ size 核对 →
        manifest 规范 JSON hash → fsync 目录 → 原子 rename 内容 → 原子 rename
        manifest（= 完成标记）→ 原子写 hash sidecar。cancel/fail（未调用 publish
        或 publish 前废弃）→ 无 COMPLETE manifest。
        """
        if artifact_id not in self._manifests:
            raise RuntimeError(
                f"publish before stage_manifest: {artifact_id} "
                f"(cancel/fail → 无成功对象)")
        if artifact_id in self._aborted:
            raise RuntimeError(
                f"publish aborted artifact {artifact_id} "
                f"(cancel/fail → 无成功对象)")
        tmp_obj = self._stage_dir / f"{artifact_id}.tmp"
        if not tmp_obj.is_file():
            raise RuntimeError(
                f"no staged content for {artifact_id} "
                f"(cancel/fail → 无成功对象)")
        doc = dict(self._manifests[artifact_id])
        # 完整校验（发布前最后一次全检；内容 hash 与 manifest 声明一致）
        raw = self.io.read_bytes(tmp_obj)
        content_hex = hashlib.sha256(raw).hexdigest()
        cd = doc.get("content_digest", {})
        if not isinstance(cd, dict) or cd.get("algorithm") != "sha256":
            raise ValueError("manifest content_digest must be sha256")
        decl = cd.get("hex")
        if not isinstance(decl, str) or not _HEX64.match(decl):
            raise ValueError("manifest content_digest.hex must be 64 hex chars")
        if decl != content_hex:
            raise ValueError(
                f"content hash mismatch: declared {decl}, computed {content_hex} "
                f"(tamper/错误 schema 拒绝)")
        size = doc.get("size")
        if size != len(raw):
            raise ValueError(
                f"manifest size {size} != staged content {len(raw)} "
                f"(错误 schema 拒绝)")

        # manifest hash = 规范 JSON sha256（sidecar 持久化；可重算）
        manifest_json = canonical_manifest_json(doc)
        manifest_hex = hashlib.sha256(manifest_json.encode("utf-8")).hexdigest()

        final_obj = self._objects_dir / artifact_id
        final_mf = self._manifests_dir / f"{artifact_id}.manifest.json"
        tmp_mf = self._stage_dir / f"{artifact_id}.manifest.json.tmp"
        with open(tmp_mf, "w", encoding="utf-8") as f:
            f.write(manifest_json)
            f.flush()
            os.fsync(f.fileno())
        self._fsync_dir(self._objects_dir)
        self._fsync_dir(self._manifests_dir)
        # 原子 publish: 先内容后 manifest（manifest rename = 完成标记）
        self.io.atomic_publish(tmp_obj, final_obj)
        self._fsync_dir(self._objects_dir)
        self.io.atomic_publish(tmp_mf, final_mf)
        self._fsync_dir(self._manifests_dir)
        # hash sidecar（唯一 producer 语义下与 manifest 同生命周期）
        sha_path = self._manifests_dir / f"{artifact_id}.manifest.sha256"
        tmp_sha = self._stage_dir / f"{artifact_id}.manifest.sha256.tmp"
        with open(tmp_sha, "w", encoding="utf-8") as f:
            f.write(manifest_hex)
            f.flush()
            os.fsync(f.fileno())
        self.io.atomic_publish(tmp_sha, sha_path)

        self._objects[artifact_id] = final_obj
        self._manifests[artifact_id] = doc
        self._manifest_digests[artifact_id] = manifest_hex
        return Digest(content_hex, len(raw))

    # ── 读路径（消费前必须经 Store 校验；模块只拿 handle/reader） ──
    def get_digest(self, artifact_id: str) -> Optional[Digest]:
        """已发布对象的内容 digest（不存在/无成功对象 → None）。"""
        doc = self._manifests.get(artifact_id)
        if doc is None:
            return None
        cd = doc.get("content_digest", {})
        if not isinstance(cd, dict):
            return None
        hex_ = cd.get("hex")
        if not isinstance(hex_, str) or not _HEX64.match(hex_):
            return None
        size = doc.get("size")
        return Digest(hex_, int(size) if isinstance(size, int) else 0)

    def read_manifest(self, artifact_id: str) -> Optional[ManifestRead]:
        """已发布 COMPLETE manifest（模块只经此读 manifest；不解析路径）。"""
        doc = self._manifests.get(artifact_id)
        if doc is None:
            return None
        return ManifestRead(dict(doc))

    def bind_as_input(self, artifact_id: str, expected_type_id: str,
                      expected_status: str = _COMPLETE) -> ManifestRead:
        """消费前绑定检查: 存在 + COMPLETE + type_id 匹配（跨 Phase 磁盘交换资格）。

        只有经本 Store 校验的已发布 COMPLETE manifest 才允许绑定为输入；
        绕过 Store 直读磁盘目录 → artifact 不在 Store 索引 → 失败（无成功对象）。
        """
        doc = self._manifests.get(artifact_id)
        if doc is None:
            raise ValueError(
                f"bind: artifact {artifact_id!r} not published in Store "
                f"(绕过 Store/未发布 → 拒绝)")
        if doc.get("status") != expected_status:
            raise ValueError(
                f"bind: artifact {artifact_id!r} status "
                f"{doc.get('status')!r} != {expected_status!r} "
                f"(非 COMPLETE 不是成功对象)")
        tid = doc.get("type_id")
        if tid != expected_type_id:
            raise ValueError(
                f"bind: artifact {artifact_id!r} type_id {tid!r} != expected "
                f"{expected_type_id!r} (role/schema 不符拒绝)")
        return ManifestRead(dict(doc))

    def read_verified(self, artifact_id: str, expected_type_id: str) -> bytes:
        """消费内容: 绑定校验通过后只经 Store 读字节并重算 hash 与 manifest 一致。

        hash 不匹配（磁盘篡改）→ 拒绝（篡改 → 硬失败）。
        """
        self.bind_as_input(artifact_id, expected_type_id)
        obj = self._objects.get(artifact_id)
        if obj is None or not obj.is_file():
            raise ValueError(f"artifact {artifact_id!r} object missing in Store")
        data = self.io.read_bytes(obj)
        doc = self._manifests[artifact_id]
        cd = doc.get("content_digest", {})
        decl = cd.get("hex") if isinstance(cd, dict) else None
        if not isinstance(decl, str) or hashlib.sha256(data).hexdigest() != decl:
            raise ValueError(
                f"artifact {artifact_id!r} content hash mismatch at consume "
                f"(篡改/损坏 → 拒绝)")
        return data

    # ── manifest hash 重算 ──
    @staticmethod
    def manifest_hash_recompute(manifest_doc: Dict[str, Any]) -> str:
        """按 manifest 规范 JSON 重算 manifest hash（64 hex）。

        与发布时写入 sidecar 的 hash 一致（可重算验收）。manifest_doc 为发布
        形态（不含内部字段）。
        """
        doc = dict(manifest_doc)
        doc.pop("manifest_digest", None)
        return hashlib.sha256(
            canonical_manifest_json(doc).encode("utf-8")).hexdigest()

    def manifest_digest_hex(self, artifact_id: str) -> Optional[str]:
        """已发布 manifest 的 hash sidecar hex（供可重算对照）。"""
        return self._manifest_digests.get(artifact_id)

    def manifest_digest_hex_from_disk(self, artifact_id: str) -> Optional[str]:
        """直接读磁盘 hash sidecar（校验 sidecar 原子落盘）。"""
        sha_path = self._manifests_dir / f"{artifact_id}.manifest.sha256"
        if not sha_path.is_file():
            return None
        v = sha_path.read_text(encoding="utf-8").strip()
        return v if _HEX64.match(v) else None

    # ── 查询/清理 ──
    def ids(self) -> list[str]:
        return sorted(self._manifests.keys())

    def manifest_count(self) -> int:
        return len(self._manifests)

    def object_count(self) -> int:
        return len(self._objects)

    def published_objects(self) -> list[str]:
        return sorted(self._objects.keys())

    def run_dir(self) -> pathlib.Path:
        return self._run_dir

    def objects_dir(self) -> pathlib.Path:
        return self._objects_dir

    def cleanup(self) -> None:
        """run 结束清理临时 stage（cancel/fail 后无成功对象残留）。"""
        if self._stage_dir.is_dir():
            for p in self._stage_dir.iterdir():
                try:
                    if p.is_file():
                        p.unlink()
                except OSError:
                    pass
        return None

    @staticmethod
    def _fsync_dir(d: pathlib.Path) -> None:
        """目录 fsync（POSIX；Windows 无目录句柄 fsync 语义时静默跳过）。"""
        try:
            fd = os.open(str(d), os.O_RDONLY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
        except OSError:
            pass


# ─────────────────────────────────────────────────────────────────────────────
# 高层完整写路径: 写临时对象 → 完整校验 → hash → 原子 publish
# （Runtime 启动真实 Store 后，模块 execute 经 store.new_writer/stage_manifest/
# publish 三件套；cancel/fail 时不调用 publish + store.cleanup() → 无成功对象）
# ─────────────────────────────────────────────────────────────────────────────


def publish_object(store: ArtifactStore, artifact_id: str, data: bytes,
                   manifest_doc: Dict[str, Any]) -> Digest:
    """完整写路径: 临时写 → 完整校验 → hash → 原子 publish（一站式）。

    任一步失败/取消 → 抛异常；无 COMPLETE manifest、无成功对象（调用方可捕获后
    调 store.cleanup()）。
    """
    writer = store.new_writer(artifact_id)
    writer.stage_bytes(data)
    store.stage_manifest(artifact_id, manifest_doc)
    return store.publish(artifact_id)
