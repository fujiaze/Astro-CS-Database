#!/usr/bin/env python3
"""IO-003 原子 HiPS/manifest 输出发布器（执行形态）— lib/infrastructure/aio/io/hips_output_store.py

冻结语义（tasks/03_RUNTIME_DATA_IO_TASKS.md IO-003 +
docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md + 约束 A.3/A.4/A.6 +
DATA-003 production_store 原子语义 + DATA-004 provenance sidecar 语义）:

  1. 唯一输出目标：每个输出落在唯一用户路径或 run ID 目录
     {output_root}/runs/{run_id}/{user_path}（run 私有；两个不同 run 绝不共享
     一个目标路径）。目录层级 user_path 段（run_id/各组件/文件名）只允许
     [A-Za-z0-9._-]，含 '..'/'.'/空段/反斜杠/'/'内嵌 → 拒绝（路径穿越拒绝）。
  2. 发布流水线：临时写（stage 目录内）→ 关闭 → **文件数据 fsync** →
     fitsverify（tile FITS 结构 + DATASUM，lib/infrastructure/aio/io/fits_verify.py
     与 IO-001 fits_core 同算法）→ sha256 → 原子 rename → 目录 fsync（尽力，
     见第 8 条）→ 完成 manifest（`COMPLETE` 为唯一完成标记）。
     任一步失败（校验不过/中断/取消/磁盘满/权限拒绝）→ 无 COMPLETE manifest、
     无成功对象；可恢复（cleanup 或新 run 重发）。
  3. 默认不覆盖：目标已存在 → 拒绝；显式 overwrite=True 才允许替换，且替换序是
     **先写暂存 → 成功后原子让位替换**：旧的成功产品不删除，只被一次 rename(2)
     原子挪到 products/.replaced-*；换入失败则原样搬回；连搬回也失败则保留让位
     副本并把它的路径报出（禁静默丢数据）。中断窗口无完成标记，可恢复。
  4. 文件权限拒绝：任何写入路径组件含符号链接、或最终目标权限不可写/被降权
     （目录或文件），一律拒绝（先检查后写；不静默放宽）。发布产物目录/文件
     权限收紧为 0o750/0o640（阶段隔离 + 用户路径不泄露）。
  5. tree hash 可重算：完成 manifest 记录 tree_hash = sha256(规范 JSON：
     {path, size, sha256} 稳定排序）；同内容重算一致，任何文件改动即变化。
     **这是内容一致性判据，不是落盘判据**（边界见第 8 条与 verify_tree_hash）。
  6. cancel/fail → 无成功对象：InterruptIO 故障注入/手动 abort → 无 COMPLETE
     manifest；Store.start() 恢复只索引"内容 + COMPLETE manifest"齐全对象。
  7. 并发不同 run 不互相覆盖：run_id 目录隔离 + 原子 rename（同文件系统）。
  8. 落盘口径（本条是**边界声明**，不是能力声明）：
     本模块只对**文件数据**做真 fsync（StoreIO.write_bytes 内的 os.fsync(fd)）。
     **目录项**的 fsync 由 _fsync_dir() 执行，而它对任何 OSError 一律静默吞掉
     ⇒ 目录 fsync 失败在本模块内**不留任何痕迹**；本模块**没有** kDurable /
     kNotDurable 那样的持久化三态，也不声称提供它。
     仓内真正的落盘口径在 lib/infrastructure/aio/product_io/include/astro/aio/
     atomic_publish.h（PublishDurability kDurable/kNotDurable；判据 = rename 已生效
     + 目录 fsync 成功）与 atomic_publish.cpp:152-168 的 confirm_dir_durability()
     （目录 fsync 真失败 ⇒ fail-closed 上报 kErrIo；不得把「无失败证据」当成
     「已确认落盘」）。本模块**未**接入该口径 —— 如实登记为缺口：不以「更强的验证」
     冒充，也不自行发明第三套判据。

本文件为纯 Python 执行语义（Linux 控制/轻合成节点可完整验证）；Windows 正式
DLL 交付由 IO-003 同语义 C 接线复刻（同一发布状态机；manifest/tree hash 公式
不变）。科学公式/常数不改；DATA-001 manifest schema 冻结字段不改。
"""
from __future__ import annotations

import errno
import hashlib
import json
import os
import pathlib
import re
import shutil
import time
from typing import Any, Dict, Iterable, List, Optional, Tuple

_SAFE_SEG = re.compile(r"^[A-Za-z0-9._-]+$")
_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_COMPLETE = "COMPLETE"
_RUN_RE = re.compile(r"^[A-Za-z0-9._-]+$")
_MANIFEST_NAME = "manifest.json"
# 允许发布的文件名（含默认产物）
_FNAME_RE = re.compile(
    r"^[A-Za-z0-9._-]+\.fits$|^properties$|^Moc\.fits$|^coverage\.index\.json$")


class PublishError(ValueError):
    """发布失败（路径穿越/权限/已存在/校验失败/中断）——一律无成功对象。"""


class PathTraversalError(PublishError):
    """路径穿越拒绝。"""


class PermissionError_(PublishError):
    """文件权限/符号链接拒绝。"""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def utc_now_z() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def canonical_json(doc: Dict[str, Any]) -> str:
    """规范 JSON：键序稳定（构造方按冻结序）、紧凑、无多余空白。"""
    return json.dumps(doc, ensure_ascii=False, sort_keys=False,
                      separators=(",", ":"))


def tree_hash(entries: Iterable[Dict[str, Any]]) -> str:
    """tree hash：条目按 (rel_path, size, sha256) 稳定排序后做 sha256。

    条目形如 {"path": rel, "size": int, "sha256": hex64}；与发布时写入
    manifest.tree 的条目完全同构 → 可重算验收（同内容 → 同 hash）。
    """
    norm = []
    for e in entries:
        rel = e.get("path")
        if not isinstance(rel, str) or not rel:
            raise PublishError("tree entry path required non-empty")
        size = e.get("size")
        sha = e.get("sha256")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise PublishError(f"tree entry size invalid for {rel!r}")
        if not isinstance(sha, str) or not _HEX64.match(sha):
            raise PublishError(f"tree entry sha256 invalid for {rel!r}")
        norm.append((rel, int(size), sha))
    norm.sort(key=lambda t: (t[0], t[1], t[2]))
    payload = json.dumps(norm, ensure_ascii=False, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def recompute_tree_hash(manifest_doc: Dict[str, Any]) -> Optional[str]:
    """从完成 manifest 的 tree 条目重算 tree hash（可重算验收）。

    与 manifest.tree_hash 一致 → 同内容重算一致；缺 tree/tree_hash 字段返回 None。
    """
    entries = manifest_doc.get("tree")
    if not isinstance(entries, list):
        return None
    return tree_hash(entries)


def validate_relative_path(user_path: str, what: str = "user_path") -> str:
    """校验用户路径词法：段只允许 [A-Za-z0-9._-]，禁 '..'/'.'/空段/反斜杠。

    返回规范化相对路径（'/' 连接，无首尾 '/'）；违规 → PathTraversalError。
    本函数不做文件系统访问（词法层拒绝穿越）。
    """
    if not isinstance(user_path, str) or not user_path:
        raise PathTraversalError(f"{what} required non-empty")
    if "\\" in user_path:
        raise PathTraversalError(
            f"{what} backslash rejected (Windows 分隔符不得进入用户路径): "
            f"{user_path!r}")
    up = user_path
    if up.startswith("/") or up.endswith("/"):
        raise PathTraversalError(f"{what} must be relative: {user_path!r}")
    segs = up.split("/")
    for seg in segs:
        if seg in ("", ".", ".."):
            raise PathTraversalError(
                f"{what} traversal/empty segment rejected: {user_path!r}")
        if not _SAFE_SEG.match(seg):
            raise PathTraversalError(
                f"{what} illegal character in segment {seg!r}: {user_path!r}")
    return "/".join(segs)


def validate_run_id(run_id: str) -> str:
    if not isinstance(run_id, str) or not run_id:
        raise PublishError("run_id required non-empty")
    if not _RUN_RE.match(run_id):
        raise PublishError(f"run_id illegal: {run_id!r}")
    return run_id


def _check_no_symlink(path: pathlib.Path) -> None:
    """路径上任何已存在组件为符号链接 → 权限拒绝（防逃逸 stage/目标）。"""
    cur = pathlib.Path(path.anchor) if path.is_absolute() else pathlib.Path(".")
    parts = path.parts if path.is_absolute() else path.parts
    probe = pathlib.Path(path.anchor) if path.is_absolute() else pathlib.Path()
    for part in parts:
        if not part or part == path.anchor:
            continue
        probe = probe / part
        try:
            if probe.is_symlink():
                raise PermissionError_(
                    f"symlink component rejected: {probe} "
                    f"(文件权限/路径穿越拒绝)")
        except OSError:
            raise PermissionError_(f"stat failed on {probe}")
    _ = cur


def _fsync_dir(d: pathlib.Path) -> None:
    try:
        fd = os.open(str(d), os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError:
        pass


def _raise_perm(exc: OSError, what: str) -> None:
    """把底层 OSError 映射为 PermissionError_（权限拒绝语义）。

    EACCES/EPERM/EROFS/EISDIR = 权限拒绝 → PermissionError_；其余 OSError
    （ENOSPC 等）保持原异常由上层归类（无成功对象语义同样成立）。
    """
    if exc.errno in (errno.EACCES, errno.EPERM, errno.EROFS, errno.EISDIR):
        raise PermissionError_(f"{what}: {exc.strerror or exc}")
    raise exc


class StoreIO:
    """磁盘 I/O 后端（可被 spy / 故障注入后端替换；语义同 DATA-003 StoreIO）。

    权限拒绝（EACCES/EPERM/EROFS/EISDIR）统一映射为 PermissionError_
    （验收：文件权限拒绝）。其余 OSError（ENOSPC 等）原样抛出 → 上层按
    "无成功对象" 语义处理（无 COMPLETE manifest）。
    """

    def mkdir(self, p: pathlib.Path, mode: int = 0o750) -> None:
        try:
            p.mkdir(parents=True, exist_ok=True)
        except OSError as exc:
            _raise_perm(exc, f"mkdir {p}")
        try:
            os.chmod(p, mode)
        except OSError:
            pass

    def write_bytes(self, p: pathlib.Path, data: bytes, mode: int = 0o640) -> None:
        try:
            fd = os.open(str(p), os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
        except OSError as exc:
            _raise_perm(exc, f"open {p}")
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(data)
                f.flush()
                os.fsync(f.fileno())
        except OSError as exc:
            try:
                p.unlink()
            except OSError:
                pass
            _raise_perm(exc, f"write {p}")
        except BaseException:
            try:
                p.unlink()
            except OSError:
                pass
            raise

    def copy_file(self, src: pathlib.Path, dst: pathlib.Path) -> None:
        try:
            shutil.copyfile(src, dst)
        except OSError as exc:
            _raise_perm(exc, f"copy {dst}")
        try:
            fd = os.open(str(dst), os.O_RDONLY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
        except OSError:
            pass

    def atomic_rename(self, tmp: pathlib.Path, final: pathlib.Path) -> None:
        try:
            os.replace(tmp, final)
        except OSError as exc:
            _raise_perm(exc, f"rename {tmp} -> {final}")

    def unlink(self, p: pathlib.Path) -> None:
        try:
            p.unlink()
        except OSError as exc:
            _raise_perm(exc, f"unlink {p}")

    def remove_tree(self, p: pathlib.Path) -> None:
        try:
            if p.is_dir() and not p.is_symlink():
                shutil.rmtree(p)
            elif p.is_file() or p.is_symlink():
                p.unlink()
        except OSError as exc:
            _raise_perm(exc, f"remove {p}")


class SpyStoreIO(StoreIO):
    """spy：记录真实磁盘事件（写/拷贝/rename/删除）——验收证据。"""

    def __init__(self) -> None:
        self.events: List[Dict[str, Any]] = []

    def _log(self, kind: str, **kw: Any) -> None:
        rec = {"kind": kind, "utc": utc_now_z()}
        rec.update(kw)
        self.events.append(rec)

    def write_bytes(self, p: pathlib.Path, data: bytes, mode: int = 0o640) -> None:
        self._log("write_bytes", path=str(p), bytes=len(data))
        return super().write_bytes(p, data, mode)

    def copy_file(self, src: pathlib.Path, dst: pathlib.Path) -> None:
        self._log("copy_file", src=str(src), dst=str(dst))
        return super().copy_file(src, dst)

    def atomic_rename(self, tmp: pathlib.Path, final: pathlib.Path) -> None:
        self._log("atomic_rename", from_=str(tmp), to=str(final))
        return super().atomic_rename(tmp, final)

    def unlink(self, p: pathlib.Path) -> None:
        self._log("unlink", path=str(p))
        return super().unlink(p)

    def remove_tree(self, p: pathlib.Path) -> None:
        self._log("remove_tree", path=str(p))
        return super().remove_tree(p)


class InterruptIO(StoreIO):
    """中断故障注入：在指定原子 rename 前抛 KeyboardInterrupt（模拟进程中断）。

    结果：无完成 manifest、无成功对象（可恢复）。fail_on ∈ {"rename_tiles",
    "rename_manifest", "write"}。
    """

    def __init__(self, fail_on: str = "rename_manifest") -> None:
        self.fail_on = fail_on
        self.renames: List[str] = []

    def write_bytes(self, p: pathlib.Path, data: bytes, mode: int = 0o640) -> None:
        if self.fail_on == "write":
            raise KeyboardInterrupt("process interrupted mid-stage-write")
        return super().write_bytes(p, data, mode)

    def atomic_rename(self, tmp: pathlib.Path, final: pathlib.Path) -> None:
        if self.fail_on == "rename_manifest" and final.name == _MANIFEST_NAME:
            raise KeyboardInterrupt("process interrupted mid-publish (manifest)")
        if self.fail_on == "rename_tiles" and final.name != _MANIFEST_NAME:
            raise KeyboardInterrupt("process interrupted mid-publish (tile)")
        return super().atomic_rename(tmp, final)


class ReadOnlyIO(StoreIO):
    """权限故障注入：写/rename 一律权限拒绝（= 真实 StoreIO 对 EACCES 的映射结果）。

    真实磁盘权限拒绝时 StoreIO 把 EACCES 映射为 PermissionError_；本注入后端
    直接以同语义抛 PermissionError_（验收：文件权限拒绝 → PermissionError_）。
    """

    def write_bytes(self, p: pathlib.Path, data: bytes, mode: int = 0o640) -> None:
        raise PermissionError_(f"read-only fault injection: write {p}")

    def copy_file(self, src: pathlib.Path, dst: pathlib.Path) -> None:
        raise PermissionError_(f"read-only fault injection: copy {dst}")

    def atomic_rename(self, tmp: pathlib.Path, final: pathlib.Path) -> None:
        raise PermissionError_(f"read-only fault injection: rename {final}")


# ─────────────────────────────────────────────────────────────────────────────
# 原子 HiPS/manifest 输出发布器
# ─────────────────────────────────────────────────────────────────────────────


class HipsOutputStore:
    """IO-003 原子产物发布器：唯一 run 目录 + 临时写→fitsverify→sha256→原子 rename→完成 manifest。

    布局:
      {output_root}/runs/{run_id}/stage/            = 临时写区（未发布；可恢复清理）
      {output_root}/runs/{run_id}/products/{user_path} = 已发布目标（唯一用户路径）
      {output_root}/runs/{run_id}/products/{user_path}/manifest.json = 完成 manifest

    manifest（完成标记，status=COMPLETE）:
      manifest_schema: acsd.hips-output-manifest/v1
      manifest_version: 1
      run_id / user_path / product（目录型产物名）
      tree: [{path, size, sha256} ...]（全部发布文件；稳定排序）
      tree_hash: sha256(规范 tree)  ← 可重算验收
      fitsverify: 逐 tile DATASUM/结构校验通过记录
      created_utc / publisher: acsd.hips-output/v1
    """

    def __init__(self, output_root: pathlib.Path, run_id: str,
                 io: Optional[StoreIO] = None,
                 verify_checksum: bool = False) -> None:
        self.output_root = pathlib.Path(output_root)
        self.run_id = validate_run_id(run_id)
        self.io = io if io is not None else StoreIO()
        self.verify_checksum = verify_checksum
        self._run_dir = self.output_root / "runs" / self.run_id
        self._stage_dir = self._run_dir / "stage"
        self._products_dir = self._run_dir / "products"
        # 已发布（恢复后）索引: rel_path → {size, sha256}
        self._published: Dict[str, Dict[str, Any]] = {}
        # 覆盖写让位副本的进程内计数（同 run 内让位名不重名）
        self._retire_seq = 0

    # ── 生命周期 ──
    def start(self) -> "HipsOutputStore":
        """建立 run 私有目录（0o750）；恢复只索引含 COMPLETE manifest 的对象。"""
        self.io.mkdir(self._run_dir)
        self.io.mkdir(self._stage_dir)
        self.io.mkdir(self._products_dir)
        # 恢复：products/ 下每目录若有 manifest.json(status=COMPLETE) 即成功对象
        if self._products_dir.is_dir():
            for child in sorted(self._products_dir.iterdir()):
                if not child.is_dir() or child.name.startswith("."):
                    continue
                mf = child / _MANIFEST_NAME
                if not mf.is_file():
                    continue  # 无完成 manifest → 非成功对象（中断残留不索引）
                try:
                    doc = json.loads(mf.read_text(encoding="utf-8"))
                except Exception:
                    continue
                if not isinstance(doc, dict) or doc.get("status") != _COMPLETE:
                    continue
                rel = str(child.relative_to(self._products_dir))
                entries = doc.get("tree")
                if not isinstance(entries, list):
                    continue
                ok = True
                for e in entries:
                    p = child / e["path"]
                    if not p.is_file():
                        ok = False
                        break
                if not ok:
                    continue
                self._published[rel] = {"size": -1, "sha256": ""}
        return self

    def published_products(self) -> List[str]:
        return sorted(self._published.keys())

    def run_dir(self) -> pathlib.Path:
        return self._run_dir

    def stage_dir(self) -> pathlib.Path:
        return self._stage_dir

    def products_dir(self) -> pathlib.Path:
        return self._products_dir

    # ── 目标解析（唯一用户路径；路径穿越/权限拒绝在发布器内集中） ──
    def _target_dir(self, user_path: str) -> pathlib.Path:
        """解析唯一目标目录：{products}/{user_path}（词法已验 + 无符号链）。"""
        rel = validate_relative_path(user_path)
        _check_no_symlink(self._products_dir)
        target = self._products_dir
        for seg in rel.split("/"):
            target = target / seg
            _check_no_symlink(target.parent)
        if target == self._products_dir:
            raise PathTraversalError("user_path must name a product directory")
        if self._products_dir not in target.parents and target != self._products_dir:
            raise PathTraversalError(
                f"user_path escapes products root: {user_path!r}")
        return target

    def _stage_file(self, rel_path: str) -> pathlib.Path:
        """临时写路径（stage 私有区；文件名词法已由发布器控制）。

        定址按**完整相对路径**而非文件名。HiPS 目录产物在同一子产品下含多个分辨率
        阶次（NorderK/DirD/NpixN.fits），同名 NpixN.fits 会合法地出现在不同 NorderK
        下 —— NESTED 下像素 0..3 的各阶祖先都折叠回 0，故 Norder1/Dir0/Npix0.fits 与
        Norder2/Dir0/Npix0.fits 同名是正常形态。按文件名定址会让两个不同的产品文件
        落进同一暂存路径，第二个撞上 publish_directory 的 "stage path already exists"
        而使这一类合法多阶产品根本无法发布。故对完整相对路径取 sha256 定址；尾部仍
        保留文件名仅为可读性（不参与定址）。摘要宽度保持 16 hex 不变（下游
        eng/tests/io/test_hips_output_contract.py:298 按该宽度构造预置符号链接），
        本单只改定址**输入**，不改宽度。
        """
        seg = rel_path.rsplit("/", 1)[-1] if "/" in rel_path else rel_path
        if not _FNAME_RE.match(seg):
            raise PublishError(f"publish filename illegal: {seg!r}")
        key = hashlib.sha256(rel_path.encode("utf-8")).hexdigest()[:16]
        return self._stage_dir / f"{key}.{seg}"

    # ── 完整发布（文件集合 → 原子目录产物） ──
    def publish_directory(self, user_path: str, files: Iterable[Tuple[str, bytes]],
                          *, overwrite: bool = False,
                          producer: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """一次性发布目录型产物（HiPS tiles + properties…）。

        files: (相对文件名, 内容字节) 迭代；每个文件:
          临时写 → 关闭(fsync) → fitsverify（.fits 后缀；结构+DATASUM）→ sha256。
        全部成功 → 原子 rename 每个文件 → 最后原子写 manifest.json(COMPLETE)
        = 完成标记。默认不覆盖；overwrite=True 先清理旧目标（中断窗口可恢复）。
        返回完成 manifest 文档。
        """
        # ── 词法/存在性预检（先检后写） ──
        rel = validate_relative_path(user_path)
        target = self._target_dir(rel)
        target_exists = target.exists() or target.is_symlink()
        is_success = target_exists and self.has_complete_manifest(rel)
        if is_success and not overwrite:
            raise PublishError(
                f"target exists (overwrite=0): {rel} "
                f"(默认不覆盖，显式 overwrite 才可)")
        # 目标区此刻**一个字节都不动**：清残 / 让位必须等暂存全部成功之后才做
        # （见下面的 retire 段与 _retire_success_target）—— 先毁旧再暂新时，暂存或
        # 写入中途失败会把旧的正确产物提前毁掉且不可恢复。

        # ── 文件清单词法校验（先检后写） ──
        file_list = [(validate_relative_path(fn, "filename"), data)
                     for fn, data in files]
        if not file_list:
            raise PublishError("publish_directory requires at least one file")
        fnames = {fn for fn, _ in file_list}
        if "properties" not in fnames:
            raise PublishError("HiPS 目录产物缺 properties (必填)")
        for fn, _ in file_list:
            self._validate_product_filename(fn)

        # ── stage：临时写 + 关闭 + fitsverify + sha256 ──
        # staged 每项的 fitsverify = 该文件**实际**过了 fitsverify 的结果（None = 没验）。
        # 不得用 `fn.endswith(".fits")` 当「验过」的标记：Moc.fits 也是 .fits 后缀却被
        # 下一行的分支显式排除（IO-001 支持域外），用它会让完成 manifest 的
        # fitsverify.tile_count 把未校验的 Moc.fits 计成已校验。
        staged: List[Dict[str, Any]] = []
        try:
            for fn, data in file_list:
                tmp = self._stage_file(fn)
                # 临时文件组件不得为符号链接/已存在（O_EXCL 语义前置检查）
                _check_no_symlink(self._stage_dir)
                _check_no_symlink(tmp)
                if tmp.exists() or tmp.is_symlink():
                    raise PermissionError_(
                        f"stage path already exists (拒绝符号链接/覆盖 stage): {tmp}")
                self.io.write_bytes(tmp, data, mode=0o640)
                # fitsverify：tile 科学平面 FITS（NpixN.fits）必须过结构+DATASUM；
                # Moc.fits 为 BINTABLE 扩展（IO-001 支持域外），只做 sha256 + 原子落盘
                # （与 IO-002 读端 "MOC optional hint" 一致：缺失/损坏不阻塞发布）。
                verified = None
                if fn.endswith(".fits") and not fn.endswith("Moc.fits"):
                    verified = self._verify_fits(tmp, fn)
                sha = sha256_bytes(data)
                staged.append({"tmp": tmp, "rel": fn, "size": len(data),
                               "sha256": sha, "fits": fn.endswith(".fits"),
                               "fitsverify": verified})
        except BaseException:
            self._cleanup_staged(staged)
            raise

        # ── 暂存全部成功，此刻才动目标区（先写暂存 → 成功后原子替换）──────────
        # 旧的正确产品**不删除**，而是先一次 rename(2) 原子让位到 products/.replaced-*
        # （同目录 ⇒ 不跨文件系统）。让位失败 ⇒ 目标根本没被碰过、旧产品原样在位；
        # 换入失败 ⇒ 原样搬回；连搬回也失败 ⇒ 保留让位副本并把它的路径报出，
        # 绝不静默删除（禁静默丢数据）。
        backup: Optional[pathlib.Path] = None
        try:
            if target_exists:
                if is_success:
                    backup = self._retire_success_target(target, rel)
                else:
                    # 中断/cancel 残留（目录存在但无 COMPLETE manifest）= 非成功对象
                    # （DATA-003 语义：cancel/fail → 无成功对象，可恢复）→ 清残后重发。
                    if target.is_dir() and not target.is_symlink():
                        self.io.remove_tree(target)
                    else:
                        self.io.unlink(target)
        except BaseException:
            # 让位/清残失败 ⇒ 新树尚未换入、目标保持原状（旧产品未丢）。但此时已写好的
            # 暂存必须收干净，否则同 run 的下一次发布会撞 "stage path already exists"
            # 而把可恢复失败变成不可重发。
            self._cleanup_staged(staged)
            raise

        try:
            # ── 目标目录就绪 ──
            try:
                _check_no_symlink(target.parent)
                self.io.mkdir(target, mode=0o750)
            except OSError as exc:
                self._cleanup_staged(staged)
                raise PermissionError_(f"target dir create failed: {exc}")

            # ── 原子 rename 全部文件（先建 manifest 前文件；顺序无关紧要） ──
            try:
                for rec in sorted(staged, key=lambda r: r["rel"]):
                    final = target / rec["rel"]
                    if final.parent != target:
                        # NorderK/DirD/… 子目录
                        self.io.mkdir(final.parent, mode=0o750)
                    self.io.atomic_rename(rec["tmp"], final)
                _fsync_dir(target)
            except BaseException:
                # 中断/失败：目标区已半写 → 无完成 manifest（非成功对象；可恢复）
                self._cleanup_staged(staged)
                raise

            # ── 完成 manifest（唯一完成标记；原子 rename 落盘） ──
            tree_entries = sorted(
                [{"path": r["rel"], "size": r["size"], "sha256": r["sha256"]}
                 for r in staged],
                key=lambda e: (e["path"], e["size"], e["sha256"]))
            # fitsverify 三项**全部由实际校验记录导出**，不写字面量：
            # performed = 至少有一个 tile 真的过了 fitsverify；
            # tile_count = 真的过了 fitsverify 的 tile 数（Moc.fits 不计——它按上面
            #   的分支就没验）；checksum = 当且仅当每个已验 tile 都带 DATASUM 卡并
            #   通过该卡校验时才是 "datasum"，否则如实记 "none"。
            # 期望量来自 _verify_fits 返回的校验事实（校验器自身的输出字段），
            # 不是把这里的定义式重写一遍与它自己比较。
            vtiles = [r for r in staged if r["fitsverify"] is not None]
            datasum_all = bool(vtiles) and all(
                r["fitsverify"]["datasum"] for r in vtiles)
            doc: Dict[str, Any] = {
                "manifest_schema": "acsd.hips-output-manifest/v1",
                "manifest_version": 1,
                "status": _COMPLETE,
                "run_id": self.run_id,
                "user_path": rel,
                "product": rel.rsplit("/", 1)[-1],
                "publisher": "acsd.hips-output/v1",
                "tree": tree_entries,
                "tree_hash": tree_hash(tree_entries),
                "fitsverify": {"performed": bool(vtiles),
                               "checksum": "datasum" if datasum_all else "none",
                               "tile_count": len(vtiles)},
                "created_utc": utc_now_z(),
            }
            if producer:
                doc["producer"] = dict(producer)
            manifest_json = canonical_json(doc)
            tmp_mf = (self._stage_dir /
                      f"manifest.{self.run_id}.{rel.replace('/', '_')}.json.tmp")
            try:
                self.io.write_bytes(tmp_mf, manifest_json.encode("utf-8"),
                                    mode=0o640)
                self.io.atomic_rename(tmp_mf, target / _MANIFEST_NAME)
                _fsync_dir(target)
            except BaseException:
                try:
                    tmp_mf.unlink()
                except OSError:
                    pass
                raise
        except BaseException as exc:
            # 换入失败 ⇒ 把让位的旧产物搬回（半写目标区先撤掉，否则 rename onto
            # 非空目录必败）。搬不回就抛带让位路径的错误，绝不静默删旧产物。
            self._restore_retired(backup, target, exc)
            raise

        # ── 成功：新树已在位且带 COMPLETE manifest，旧让位副本此时才可回收 ──
        self._discard_retired(backup)
        # 成功对象登记
        self._published[rel] = {"size": -1, "sha256": doc["tree_hash"]}
        return doc

    # ── 覆盖写：旧产物的原子让位 / 回滚 / 回收 ──
    # 覆盖写序 = 先写暂存 → 成功后原子替换。旧的正确产品**永不先被删或截断**：
    # 让位是一次 rename(2)（同目录 ⇒ 不跨文件系统）；失败发生在换入之前时目标未被动。
    def _retired_name(self, rel: str) -> pathlib.Path:
        """让位副本路径：products/.replaced-<run>-<16hex>。

        点前缀是刻意的：start() 的恢复扫描按 `child.name.startswith(".")` 跳过
        products 子项，故让位副本永不被当成产品索引 —— 即使回滚失败把它留在盘上，
        也不会伪造出一个「成功对象」。
        """
        self._retire_seq += 1
        token = hashlib.sha256(
            f"{rel}|{self.run_id}|{os.getpid()}|{self._retire_seq}|"
            f"{time.time_ns()}".encode("utf-8")).hexdigest()[:16]
        return self._products_dir / f".replaced-{self.run_id}-{token}"

    def _retire_success_target(self, target: pathlib.Path,
                               rel: str) -> pathlib.Path:
        """把旧的**成功对象**原子让位到 products/.replaced-* 并返回让位路径。

        失败（让位 rename 报错，或让位后目录状态不符预期）⇒ 目标仍保持旧产物原状，
        抛 PublishError 报出；此时新树尚未换入，旧产品**没有**任何丢失。
        """
        backup = self._retired_name(rel)
        try:
            self.io.atomic_rename(target, backup)
        except Exception as exc:
            raise PublishError(
                f"覆盖发布失败：旧产物让位失败，目标未被改动，旧产物仍在 {target}: "
                f"{exc}") from exc
        if target.exists() or target.is_symlink() or not backup.exists():
            # 让位没真正生效（后端语义与预期不符）⇒ 搬回去，保持不变式
            try:
                self.io.atomic_rename(backup, target)
            except Exception:
                pass
            raise PublishError(
                f"覆盖发布失败：旧产物让位未生效，目标未被改动，旧产物仍在 {target}")
        return backup

    def _restore_retired(self, backup: Optional[pathlib.Path],
                         target: pathlib.Path, cause: BaseException) -> None:
        """换入失败 ⇒ 把让位的旧产物搬回；搬不回就抛带让位路径的错误。

        只在 backup 存在时动作（没有让位就没有可回滚的旧产物）。绝不删让位副本。
        """
        if backup is None:
            return
        try:
            if target.is_dir() and not target.is_symlink():
                self.io.remove_tree(target)
            elif target.exists() or target.is_symlink():
                self.io.unlink(target)
        except Exception:
            pass
        try:
            self.io.atomic_rename(backup, target)
        except Exception as rex:
            raise PublishError(
                f"覆盖发布失败且旧产物回滚失败：旧产物仍完整保存在 {backup}，"
                f"请人工恢复（按禁静默丢数据纪律不得自动删除）。"
                f"回滚错误={rex}; 原始错误={cause}") from rex

    def _discard_retired(self, backup: Optional[pathlib.Path]) -> None:
        """成功发布后回收让位副本；回收失败只留痕，不回滚已完成的发布。"""
        if backup is None:
            return
        try:
            self.io.remove_tree(backup)
        except Exception as exc:
            import sys as _sys
            print(f"[hips-output-store] warning: retired product cleanup failed "
                  f"({backup}): {exc}", file=_sys.stderr)

    # ── 校验助手 ──
    def _verify_fits(self, path: pathlib.Path, rel: str) -> Dict[str, Any]:
        """fitsverify 一步：结构 + DATASUM（与 fits_core 同算法）。失败抛 PublishError。

        返回**实际做了什么**的事实，供完成 manifest 如实记账：
          datasum  = 该文件带 DATASUM 卡且已通过该卡校验
          checksum = 这次调用真的做了 CHECKSUM 卡校验（受 verify_checksum 开关）
        注意：DATASUM 的校验**不受** verify_checksum 开关约束（它只管 CHECKSUM 卡），
        故 datasum 的真值来源是校验器返回的 header，而不是本函数的开关。
        """
        try:
            from fits_verify import FitsVerifyError, verify_fits_file
        except ImportError:
            import sys
            sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
            from fits_verify import FitsVerifyError, verify_fits_file  # noqa: F811
        try:
            hdr = verify_fits_file(str(path), verify_checksum=self.verify_checksum)
        except FitsVerifyError as exc:
            raise PublishError(f"fitsverify 拒绝 {rel}: {exc}")
        except OSError as exc:
            raise PublishError(f"fitsverify 读取失败 {rel}: {exc}")
        return {"datasum": hdr.datasum is not None,
                "checksum": bool(self.verify_checksum and
                                 hdr.checksum is not None)}

    @staticmethod
    def _validate_product_filename(fn: str) -> None:
        """HiPS 产物文件名约束：properties | Moc.fits | NorderK/DirD/NpixN.fits |

        coverage.index.json（**数据集级**覆盖索引；仅根层，IO-003 §3.2 白名单 P-178 扩项）。
        """
        if fn == "properties":
            return
        if fn == "Moc.fits":
            return
        # 数据集级覆盖索引（HIPS_STORAGE_FORM_CONTRACT §4.2 / A5：位于归档之外、与产品同级）。
        # 只认根层的这一个名（带路径或改名 = 新形态，须先改合同再实现）。
        if fn == "coverage.index.json":
            return
        segs = fn.split("/")
        if len(segs) == 3 and segs[0].startswith("Norder") and \
                segs[0][6:].isdigit() and segs[1].startswith("Dir") and \
                segs[1][3:].isdigit() and segs[2].startswith("Npix") and \
                segs[2][4:-5].isdigit() and segs[2].endswith(".fits"):
            return
        raise PublishError(f"非 HiPS 目录产物文件名: {fn!r} "
                           f"(仅 properties/Moc.fits/NorderK/DirD/NpixN.fits/coverage.index.json)")

    # ── 清理/查询 ──
    def _cleanup_staged(self, staged: List[Dict[str, Any]]) -> None:
        for rec in staged:
            tmp = rec.get("tmp")
            if tmp is not None:
                try:
                    tmp.unlink()
                except OSError:
                    pass

    def cleanup(self) -> None:
        """清除本 run 临时 stage（cancel/fail 后无残留）。"""
        if self._stage_dir.is_dir():
            for p in self._stage_dir.iterdir():
                try:
                    if p.is_file():
                        p.unlink()
                except OSError:
                    pass

    def has_complete_manifest(self, user_path: str) -> bool:
        rel = validate_relative_path(user_path)
        mf = self._products_dir / rel / _MANIFEST_NAME
        if not mf.is_file():
            return False
        try:
            doc = json.loads(mf.read_text(encoding="utf-8"))
        except Exception:
            return False
        return isinstance(doc, dict) and doc.get("status") == _COMPLETE

    def read_complete_manifest(self, user_path: str) -> Optional[Dict[str, Any]]:
        rel = validate_relative_path(user_path)
        mf = self._products_dir / rel / _MANIFEST_NAME
        if not mf.is_file():
            return None
        try:
            doc = json.loads(mf.read_text(encoding="utf-8"))
        except Exception:
            return None
        if not isinstance(doc, dict) or doc.get("status") != _COMPLETE:
            return None
        return doc

    def read_product_file(self, user_path: str, fn: str) -> Optional[bytes]:
        """已发布产物文件内容（sha256 与 manifest 核对；篡改拒绝）。

        能证明: (a) 该产品有 COMPLETE manifest（可消费）；(b) 按路径读到的字节
        与 manifest.tree 声明的 sha256 一致（事后篡改可检出）。

        **不能证明**: 数据或目录项已落盘。读回走的是同一文件描述符语义的页缓存，
        写方刚写完的那份数据在页缓存里原样可见 ⇒ 缺失的落盘同步在本函数里
        **结构上无法被发现**。本函数通过 ≠ 数据已落盘。落盘口径见模块 docstring 第 8 条。
        """
        rel = validate_relative_path(user_path)
        if not self.has_complete_manifest(rel):
            return None
        doc = self.read_complete_manifest(rel)
        assert doc is not None
        want = None
        for e in doc.get("tree", []):
            if e.get("path") == fn:
                want = e
                break
        if want is None:
            return None
        p = self._products_dir / rel / fn
        if not p.is_file():
            return None
        data = p.read_bytes()
        if sha256_bytes(data) != want.get("sha256"):
            raise PublishError(f"product {rel}/{fn} hash mismatch (篡改拒绝)")
        return data

    def verify_tree_hash(self, user_path: str) -> bool:
        """tree hash 可重算验收：按当前文件系统视图重算 == manifest.tree_hash。

        能证明: 产品树上每个 manifest 登记的文件此刻都存在，且其当前字节的 sha256
        与登记值一致（树内容与清单自洽）。

        **不能证明**: 落盘。"按磁盘当前文件重算"这句里的"磁盘"只是文件系统视图 ——
        读回命中的是写方自己刚写进页缓存的同一份数据，缺失的文件 fsync 或目录 fsync
        在这里都是不可见的（本模块的目录 fsync 见 _fsync_dir，连失败都不报）。
        真正的落盘判据在 atomic_publish.h 的 kDurable/kNotDurable，见模块 docstring 第 8 条。
        """
        rel = validate_relative_path(user_path)
        doc = self.read_complete_manifest(rel)
        if doc is None:
            return False
        base = self._products_dir / rel
        entries = []
        for e in doc.get("tree", []):
            p = base / e["path"]
            if not p.is_file():
                return False
            entries.append({"path": e["path"], "size": e["size"],
                            "sha256": sha256_file(p)})
        return tree_hash(entries) == doc.get("tree_hash")
