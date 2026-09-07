# Module: phase1_session

> P1-SESSION-DOC 冻结页（2026-09-07）：Phase1 进程内装配会话模块合同页 =
> lib/phase1_session/README.md（CONTRACT_READY，assembly 层）+ lib/
> phase1_session/module.yaml（astrocs.phase1.session，静态库
> astrocs_phase1_session，根 CMakeLists.txt:448-452）。合同 ID：
> DATA-P1-SESSION（DATA_SEMANTICS §16）/ API-P1-SESSION（PUBLIC_API）；
> 编排级上游合同 API-P1-001（docs/api/PHASE1_API_V1.md，FROZEN）；不设
> 独立 DLL（MODULE_MIGRATION_MATRIX 无 P1-SESSION 行）。registry 关系：
> 五函数经 P1Api（lib/core/src/module_adapters.cpp:693-700）被 8 个
> Phase1 descriptor 工厂委托（:728-735/:755-770）。

## 职责

Phase1 装配/编排：四段 io_read→calibrate→cosmetic→io_write 的进程内
会话（create/validate/run/inspect/destroy 生命周期）；config 键集校验、
取消传播、线程预算注入、manifest 产出。科学实现全部委托既有冻结 C API
（ac_calibrate_frame / ac_correct_frame，lib/calibration）。

## 非职责

不拥有任何算法（校准/星点/PSF/WCS/测光/SNR/drizzle/HiPS 公式均不在本
层，见各 SCI/ALG 冻结合同）；不描述 Phase2/3；现行 4 段实现不覆盖
API-P1-001 冻结 7-stage 序列的检测/PSF/platesolve/测光/SNR/Drizzle/HiPS
段（差距如实登记，补齐归 P1-SESSION-IMPL）。

## Public API

五导出 C API + astrocs::phase1::last_error：合同=API-P1-SESSION
（PUBLIC_API.md「Phase1 装配会话 C API」节）；签名权威=lib/phase1_session/
p1_session.h。

## Data contract

config JSON 键集 / host services / manifest / 校准 artifact：
DATA-P1-SESSION（DATA_SEMANTICS §16）；像素输出 float32 ADU [h,w]，仅
FITS 落盘。

## Ownership

handle=创建者（唯一 create/destroy 对）；inspect 输出 host alloc、调用方
host free；AIOImageData 经 canonical deleter aio_free_image_data（IO-002）。

## Thread safety

threadsafe:no（handle 级单线程）；reentrant:yes；omp worker 数=
host budget.max_workers 注入（禁硬编码）；B 线 registry 通道经
SessionModule ThreadLease 租借（module_adapters.cpp:156-162）。

## Errors

ACS_ERR_* 全集与触发锚见 API-P1-SESSION 返回码节；失败短路返回，不留
伪完整产物；manifest 记 status=failed + error/error_kind。

## Science IDs

不新设（assembly 层无新算法）：科学权威=SCI-CAL-001（docs/science/
CALIBRATION.md）+ ALG-CAL-001..006 / ALG-COS-001..005 既有冻结合同。

## Tests

TEST-P1-SESSION-001 = tests/unit/p1_ir_facade_test.cpp（facade 语义/
canonical 4 节点/委托）；生命周期登记=tests/api/test_p1_api.py。

## Source files

lib/phase1_session/p1_session.cpp / p1_session.h。
