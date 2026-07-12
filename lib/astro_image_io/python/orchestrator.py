"""
管线编排器 (orchestrator)
功能: 组装各阶段 handler 为 PipelineStageHandlerC，供 PipelineEngine 注册
用途: 在 plate_solve 后插入 PSF 拟合步骤，读取 star_det 块（星点坐标）和 data 块（像素），
      调用 DynamicPSF 拟合，结果写入 psf 块供 photometric_calib 复用
"""

from __future__ import annotations

import logging
import os
import sys

import numpy as np

# 添加模块路径（dynamic_psf）
_HERE = os.path.dirname(os.path.abspath(__file__))
_LIB_DIR = os.path.normpath(os.path.join(_HERE, "..", ".."))
for module_dir in ["dynamic_psf/python"]:
    p = os.path.join(_LIB_DIR, module_dir)
    if p not in sys.path:
        sys.path.insert(0, p)

from astro_image_io import PipelineFramePy, PipelineStageHandlerC

logger = logging.getLogger(__name__)


def _write_error(err_buf, err_cap, msg):
    """写入错误信息到缓冲区"""
    if err_buf and err_cap > 0:
        b = msg.encode('utf-8')[:err_cap - 1]
        err_buf[:len(b)] = b


def make_psf_fit_handler(dll_path=None):
    """创建PSF拟合handler
    返回: PipelineStageHandlerC 包装的handler

    用法:
        handler = make_psf_fit_handler()
        engine.register(STAGE_PHOTOMETRIC, handler)  # 或自定义阶段
    """
    def handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        try:
            frame = PipelineFramePy.from_c_ptr(c_frame_ptr)

            # 读取星点坐标
            star_det = frame.get_block_data("star_det")
            if star_det is None:
                # 如果没有star_det块，跳过PSF拟合
                logger.info("star_det 块不存在，跳过 PSF 拟合")
                return 0

            # 读取像素数据
            pixels = frame.get_block_data("data")
            if pixels is None:
                _write_error(err_buf, err_cap, "data块不存在")
                return -1

            logger.info("PSF 拟合开始: %d 颗星, 图像 shape=%s",
                        star_det.shape[0], pixels.shape)

            # float32 → uint16 转换（DynamicPSF要求uint16输入）
            image_u16 = np.clip(pixels, 0, 65535).astype(np.uint16)

            # 提取坐标
            cx_list = star_det[:, 0].tolist()
            cy_list = star_det[:, 1].tolist()

            # 调用PSF拟合
            from dynamic_psf import DynamicPSF
            results = DynamicPSF.fit_batch(image_u16, cx_list, cy_list, dll_path=dll_path)

            # 构造psf数组 FLOAT64[N, 6]
            n = len(results)
            psf_array = np.zeros((n, 6), dtype=np.float64)
            for i, r in enumerate(results):
                psf_array[i, 0] = float(r.status)
                psf_array[i, 1] = float(r.B)
                psf_array[i, 2] = float(r.flux)
                psf_array[i, 3] = float(r.cx)
                psf_array[i, 4] = float(r.cy)
                # fwhm = (fwhm_x + fwhm_y) / 2
                psf_array[i, 5] = float((r.fwhm_x + r.fwhm_y) / 2.0)

            # 写入psf块
            frame.add_block("psf", psf_array, description="PSF拟合结果")

            n_ok = int(np.sum(psf_array[:, 0] == 0))
            logger.info("PSF 拟合完成: %d/%d 成功", n_ok, n)
            return 0
        except Exception as e:
            logger.error("PSF 拟合异常: %s", e, exc_info=True)
            _write_error(err_buf, err_cap, str(e))
            return -1

    return PipelineStageHandlerC(handler)


# ============================================================================
# Orchestrator 类 - 管线编排器
# ============================================================================

import ctypes
import importlib.util
import time


class _HandlerExtractor:
    """伪 engine - 用于从 register_*_handler 函数提取 handler

    register_*_handler 函数期望接收一个有 register(stage, handler, params) 方法的对象。
    此类捕获注册的 handler 供 Orchestrator 直接调用（不通过 PipelineEngine）。
    """
    def __init__(self):
        self.handler = None
        self._keep_alive = []

    def register(self, stage, handler, params=None):
        self.handler = handler
        self._keep_alive.append(handler)


def _load_module_from_path(module_name, file_path):
    """从文件路径加载 Python 模块（避免多个 pipeline_adapter.py 命名冲突）"""
    if module_name in sys.modules:
        return sys.modules[module_name]
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod


class Orchestrator:
    """管线编排器 - 串联校准→解析→PSF→光度→drizzle全链路

    设计决策:
        PSF拟合不是标准PipelineStage（引擎只有5个标准阶段）。
        本类不使用 PipelineEngine.run_single，而是手动逐阶段调用 handler，
        这样可以在 PLATESOLVE 后灵活插入 PSF 拟合步骤。

    用法:
        orch = Orchestrator(
            calib_params=CalibrateParams(...),
            solve_params=PlateSolveParams(),
            photo_params=PhotometricParams(),
            drizzle_params=DrizzleParams(output_dir="./output"),
            log_dir="./logs",
        )
        ok = orch.run_single("image.fits", output_dir="./output")
    """

    def __init__(self, calib_params=None, solve_params=None,
                 photo_params=None, drizzle_params=None, log_dir=None):
        """初始化编排器

        参数:
            calib_params: CalibrateParams (None=跳过校准)
            solve_params: PlateSolveParams (None=跳过解析)
            photo_params: PhotometricParams (None=跳过光度校准)
            drizzle_params: DrizzleParams (None=跳过drizzle)
            log_dir: 日志目录 (None=不导出调试)
        """
        self.calib_params = calib_params
        self.solve_params = solve_params
        self.photo_params = photo_params
        self.drizzle_params = drizzle_params
        self.log_dir = log_dir

        self._lib_dir = os.path.normpath(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
        )

        self._setup_paths()

        self._handlers = []  # 持有所有 handler 引用防止 GC
        self._calib_handler = None
        self._solve_handler = None
        self._photo_handler = None
        self._drizzle_handler = None

        # 提取各阶段 handler (通过伪 engine)
        if calib_params is not None:
            self._calib_handler = self._extract_handler(
                "calibration/python/pipeline_adapter.py",
                "_orch_calib_adapter",
                "register_calibrate_handler",
                calib_params,
            )

        if solve_params is not None:
            self._solve_handler = self._extract_handler(
                "plate_solve/python/pipeline_adapter.py",
                "_orch_solve_adapter",
                "register_platesolve_handler",
                solve_params,
            )

        if photo_params is not None:
            self._photo_handler = self._extract_handler(
                "photometric_calib/flux_calibrator/python/pipeline_adapter.py",
                "_orch_photo_adapter",
                "register_photometric_handler",
                photo_params,
            )

        if drizzle_params is not None:
            self._drizzle_handler = self._extract_handler(
                "healpix_db/healpix_drizzle/pipeline_adapter.py",
                "_orch_drizzle_adapter",
                "register_drizzle_handler",
                drizzle_params,
            )

        # PSF 拟合 handler (始终创建，仅在 plate_solve 后调用)
        self._psf_handler = make_psf_fit_handler()
        self._handlers.append(self._psf_handler)

    def _setup_paths(self):
        """添加各模块路径到 sys.path"""
        module_paths = [
            "astro_image_io/python",
            "calibration/python",
            "plate_solve/python",
            "plate_solve/archive/vector_method/python/python",
            "photometric_calib/flux_calibrator/python",
            "healpix_db/healpix_drizzle",
            "dynamic_psf/python",
            "star_detector/python",
        ]
        for module_dir in module_paths:
            p = os.path.join(self._lib_dir, module_dir)
            if os.path.isdir(p) and p not in sys.path:
                sys.path.insert(0, p)

    def _extract_handler(self, adapter_rel_path, module_name, register_func, params):
        """从适配器模块提取 handler

        参数:
            adapter_rel_path: 适配器文件相对 lib 目录的路径
            module_name: 加载到 sys.modules 的唯一模块名
            register_func: 适配器中的注册函数名
            params: 阶段参数

        返回: PipelineStageHandlerC 实例
        """
        adapter_path = os.path.join(self._lib_dir, adapter_rel_path)
        if not os.path.isfile(adapter_path):
            raise FileNotFoundError(f"适配器文件不存在: {adapter_path}")
        mod = _load_module_from_path(module_name, adapter_path)
        extractor = _HandlerExtractor()
        getattr(mod, register_func)(extractor, params)
        if extractor.handler is None:
            raise RuntimeError(f"注册函数 {register_func} 未设置 handler")
        self._handlers.append(extractor.handler)
        return extractor.handler

    def _read_fits_to_frame(self, fits_path):
        """读FITS → PipelineFramePy

        返回: PipelineFramePy (含 header KV块 + data FLOAT32块)
        """
        from astro_image_io import ImageReader

        reader = ImageReader()
        img = reader.read(fits_path)

        frame = PipelineFramePy()

        meta = img.metadata
        wcs = meta.wcs
        cal = meta.calibration
        obs = meta.observation

        # --- header KV 块 ---
        frame.kv_set("header", "SOURCE_PATH", fits_path)

        if obs:
            frame.kv_set("header", "OBJECT", obs.object_name or "")
            if obs.date_obs:
                frame.kv_set("header", "DATE-OBS", obs.date_obs)
            if obs.focallen is not None:
                frame.kv_set_double("header", "FOCALLEN", obs.focallen)
            if obs.xpixsz is not None:
                frame.kv_set_double("header", "XPIXSZ", obs.xpixsz)

        if cal:
            frame.kv_set_double("header", "EXPTIME", cal.exptime)
            frame.kv_set("header", "FILTER", cal.filter_name)
            frame.kv_set_double("header", "GAIN", cal.gain)
            if cal.ccd_temp is not None:
                frame.kv_set_double("header", "CCD_TEMP", cal.ccd_temp)
            if cal.frame_type:
                frame.kv_set("header", "FRAME_TYPE", cal.frame_type)

        # 从 FITS 关键字读取 OBJCTRA/OBJCTDEC 等
        for kw in img.keywords:
            name = kw.name.upper()
            if name in ("OBJCTRA", "OBJCTDEC", "RA", "DEC", "SITELAT", "SITELONG"):
                frame.kv_set("header", name, kw.value)

        # 如果没有 OBJCTRA/DEC 但有 WCS，用 CRVAL 作为初始指向 (度数字符串)
        if wcs and wcs.has_wcs:
            if not frame.kv_get("header", "OBJCTRA"):
                frame.kv_set("header", "OBJCTRA", str(wcs.crval1))
            if not frame.kv_get("header", "OBJCTDEC"):
                frame.kv_set("header", "OBJCTDEC", str(wcs.crval2))
            # 写入 WCS 关键字
            frame.kv_set("header", "CTYPE1", wcs.ctype1)
            frame.kv_set("header", "CTYPE2", wcs.ctype2)
            frame.kv_set_double("header", "CRVAL1", wcs.crval1)
            frame.kv_set_double("header", "CRVAL2", wcs.crval2)
            frame.kv_set_double("header", "CRPIX1", wcs.crpix1)
            frame.kv_set_double("header", "CRPIX2", wcs.crpix2)
            frame.kv_set_double("header", "CD1_1", wcs.cd1_1)
            frame.kv_set_double("header", "CD1_2", wcs.cd1_2)
            frame.kv_set_double("header", "CD2_1", wcs.cd2_1)
            frame.kv_set_double("header", "CD2_2", wcs.cd2_2)

        # --- data 块 ---
        pixels = img.data  # numpy float32 [H, W]
        frame.add_block("data", pixels, description="原始像素")

        # 关闭 ImageData (pixels 已被 add_block 拷贝到 C 端)
        img.close()

        return frame

    def _print_block_status(self, frame, stage_name):
        """打印当前所有块的状态"""
        blocks = frame.list_blocks()
        print(f"  [{stage_name}] 块数量={len(blocks)}:")
        type_names = {
            0: "FLOAT32", 1: "FLOAT64", 2: "INT32",
            3: "INT64", 4: "STRING", 5: "KV", 6: "RAW",
        }
        for name in blocks:
            info = frame.get_block_info(name)
            if info:
                type_name = type_names.get(info["type"], f"TYPE_{info['type']}")
                dims = info["dims"]
                print(f"    - {name}: type={type_name}, dims={dims}, count={info['count']}")
            else:
                print(f"    - {name}: (无法获取信息)")

    def _cleanup_frame(self, frame):
        """显式清空所有块"""
        blocks = frame.list_blocks()
        for name in blocks:
            try:
                frame.remove_block(name)
            except Exception:
                pass

    def _call_handler(self, handler, frame, stage_name):
        """调用 handler 并处理错误

        返回: True=成功, False=失败
        """
        err_buf = ctypes.create_string_buffer(512)
        ret = handler(frame.c_frame, None, err_buf, 512)
        if ret != 0:
            err_msg = err_buf.value.decode("utf-8", errors="replace").strip()
            if not err_msg:
                err_msg = "(无错误详情，请查看模块日志)"
            print(f"  [错误] {stage_name} 失败 (code={ret}): {err_msg}")
            return False
        return True

    def _export_debug(self, frame, stage_name):
        """导出调试信息到日志目录"""
        if self.log_dir is None:
            return
        try:
            os.makedirs(self.log_dir, exist_ok=True)
            xml_path = os.path.join(self.log_dir, f"frame_after_{stage_name}.xml")
            frame.export_all_xml(xml_path)
            print(f"  [调试] 已导出: {xml_path}")
        except Exception as e:
            print(f"  [调试] 导出失败: {e}")

    def _get_block_names(self, frame):
        """获取当前所有块名列表"""
        try:
            return list(frame.list_blocks())
        except Exception:
            return []

    def _find_output_ahpx(self, frame, output_dir):
        """从 frame 的 SOURCE_PATH 推导 .ahpx 输出路径并验证存在"""
        try:
            source_path = frame.kv_get("header", "SOURCE_PATH")
            if source_path:
                basename = os.path.splitext(os.path.basename(source_path))[0]
            else:
                basename = "drizzle_output"
            ahpx_path = os.path.join(output_dir, basename + ".ahpx")
            if os.path.isfile(ahpx_path):
                return ahpx_path
        except Exception:
            pass
        return None

    def run_single(self, fits_path, output_dir):
        """单帧端到端处理

        参数:
            fits_path: 输入 FITS 文件路径
            output_dir: 输出目录

        返回: dict 含:
            success: bool - 是否成功
            timings: dict - 各阶段耗时(秒), key: read_fits/calibrate/platesolve/psf_fit/photometric/drizzle
            blocks: dict - 各阶段后的块名列表, key: after_read/after_calibrate/...
            output_files: list - 输出文件路径(.ahpx)
            error: str - 错误信息(None=无错误)
        """
        result = {
            "success": False,
            "timings": {},
            "blocks": {},
            "output_files": [],
            "error": None,
            "photo_stats": {},  # PHOTOMETRIC 后的 photo_stats KV 值
            "wcs": {},          # PLATESOLVE 后的 WCS 关键字段
        }

        print("=" * 60)
        print(f"编排器开始处理: {fits_path}")
        print(f"输出目录: {output_dir}")
        print("=" * 60)

        # 1. 读FITS → frame
        t0 = time.time()
        frame = self._read_fits_to_frame(fits_path)
        result["timings"]["read_fits"] = time.time() - t0
        print(f"[{result['timings']['read_fits']:.2f}s] FITS 读取完成")
        self._print_block_status(frame, "读取后")
        result["blocks"]["after_read"] = self._get_block_names(frame)

        try:
            # 2. CALIBRATE
            if self.calib_params is not None:
                print("-" * 40)
                print("阶段: CALIBRATE")
                t0 = time.time()
                if not self._call_handler(self._calib_handler, frame, "CALIBRATE"):
                    self._print_block_status(frame, "CALIBRATE (FAILED)")
                    result["error"] = "校准失败"
                    raise RuntimeError("校准失败")
                result["timings"]["calibrate"] = time.time() - t0
                print(f"[{result['timings']['calibrate']:.2f}s] CALIBRATE 完成")
                self._print_block_status(frame, "CALIBRATE后")
                result["blocks"]["after_calibrate"] = self._get_block_names(frame)
                self._export_debug(frame, "calibrate")

            # 3. PLATESOLVE
            if self.solve_params is not None:
                print("-" * 40)
                print("阶段: PLATESOLVE")
                t0 = time.time()
                if not self._call_handler(self._solve_handler, frame, "PLATESOLVE"):
                    self._print_block_status(frame, "PLATESOLVE (FAILED)")
                    result["error"] = "解析失败"
                    raise RuntimeError("解析失败")
                result["timings"]["platesolve"] = time.time() - t0
                print(f"[{result['timings']['platesolve']:.2f}s] PLATESOLVE 完成")
                # 手动丢弃 weight 块
                if frame.has_block("weight"):
                    frame.remove_block("weight")
                    print("  [清理] 丢弃 weight 块")
                self._print_block_status(frame, "PLATESOLVE后")
                result["blocks"]["after_platesolve"] = self._get_block_names(frame)
                # 收集 WCS 关键字段 (供测试验证)
                for kw in ["CD1_1", "CD1_2", "CD2_1", "CD2_2",
                           "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
                           "CTYPE1", "CTYPE2"]:
                    val = frame.kv_get("header", kw)
                    if val is not None:
                        result["wcs"][kw] = val
                self._export_debug(frame, "platesolve")

            # 4. PSF_FIT (仅在 plate_solve 后执行，需要 star_det 块)
            if self.solve_params is not None:
                print("-" * 40)
                print("阶段: PSF_FIT")
                t0 = time.time()
                if not self._call_handler(self._psf_handler, frame, "PSF_FIT"):
                    self._print_block_status(frame, "PSF_FIT (FAILED)")
                    result["error"] = "PSF拟合失败"
                    raise RuntimeError("PSF拟合失败")
                result["timings"]["psf_fit"] = time.time() - t0
                print(f"[{result['timings']['psf_fit']:.2f}s] PSF_FIT 完成")
                self._print_block_status(frame, "PSF_FIT后")
                result["blocks"]["after_psf_fit"] = self._get_block_names(frame)
                self._export_debug(frame, "psf_fit")

            # 5. PHOTOMETRIC
            if self.photo_params is not None:
                print("-" * 40)
                print("阶段: PHOTOMETRIC")
                t0 = time.time()
                if not self._call_handler(self._photo_handler, frame, "PHOTOMETRIC"):
                    self._print_block_status(frame, "PHOTOMETRIC (FAILED)")
                    result["error"] = "光度校准失败"
                    raise RuntimeError("光度校准失败")
                result["timings"]["photometric"] = time.time() - t0
                print(f"[{result['timings']['photometric']:.2f}s] PHOTOMETRIC 完成")
                # 收集 photo_stats KV 值 (供测试验证)
                for k in ["N_MATCHED", "SCALE_FACTOR"]:
                    v = frame.kv_get("photo_stats", k)
                    if v is not None:
                        result["photo_stats"][k] = v
                # 手动丢弃 star_det/gaia_cat/psf 块 (photometric_calib 已不再生成 grad_map 块)
                for name in ["star_det", "gaia_cat", "psf"]:
                    if frame.has_block(name):
                        frame.remove_block(name)
                        print(f"  [清理] 丢弃 {name} 块")
                self._print_block_status(frame, "PHOTOMETRIC后")
                result["blocks"]["after_photometric"] = self._get_block_names(frame)
                self._export_debug(frame, "photometric")

            # 6. DRIZZLE
            if self.drizzle_params is not None:
                print("-" * 40)
                print("阶段: DRIZZLE")
                t0 = time.time()
                if not self._call_handler(self._drizzle_handler, frame, "DRIZZLE"):
                    self._print_block_status(frame, "DRIZZLE (FAILED)")
                    result["error"] = "Drizzle失败"
                    raise RuntimeError("Drizzle失败")
                result["timings"]["drizzle"] = time.time() - t0
                print(f"[{result['timings']['drizzle']:.2f}s] DRIZZLE 完成")
                self._print_block_status(frame, "DRIZZLE后")
                result["blocks"]["after_drizzle"] = self._get_block_names(frame)
                self._export_debug(frame, "drizzle")
                # 收集输出文件
                ahpx = self._find_output_ahpx(frame, output_dir)
                if ahpx:
                    result["output_files"].append(ahpx)

            print("=" * 60)
            print("编排器处理完成 (成功)")
            print("=" * 60)
            result["success"] = True
            return result

        except Exception as e:
            print(f"编排器错误: {e}")
            self._print_block_status(frame, "ERROR")
            print("=" * 60)
            print("编排器处理失败")
            print("=" * 60)
            if not result["error"]:
                result["error"] = str(e)
            return result
        finally:
            self._cleanup_frame(frame)
            frame.close()
