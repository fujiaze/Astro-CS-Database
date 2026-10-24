# 科学process目录

每行是一份独立维护的算法模块文档，operation是该process内的typed操作白名单。固定调度/注册/资源规则只在[SYSTEM](../SYSTEM.md)维护；公共格式与执行标准不在本目录复制。此表是文档目录，不再维护一份手工代码注册表。生产构建从模块本地声明汇总只读注册表并校验本目录覆盖；实现前不能把本表当已有可执行registry。

| process_id / 文档 | 模块 | typed operation |
|---|---|---|
| [observation.calibrate](observation.calibrate.md) | observation | calibrate_observation |
| [observation.regroup](observation.regroup.md) | observation | regroup_observation |
| [coordinates.wcs](coordinates.wcs.md) | coordinates | make_wcs,forward.center0,forward.edge0,inverse,derivative,solid_angle_jacobian,bind_wcs |
| [catalogue.access](catalogue.access.md) | catalogue | build_catalogue_index,make_catalogue_provider,catalogue.query,catalogue.query_quads,catalogue.lookup |
| [sources.astrometry](sources.astrometry.md) | sources | solve_astrometry |
| [sources.psf](sources.psf.md) | sources | fit_psf,evaluate_psf,bind_observation_model |
| [sources.measure](sources.measure.md) | sources | measure_science_sources |
| [response.integrate](response.integrate.md) | response | integrate_response |
| [photometry.fit](photometry.fit.md) | photometry | fit_photometry |
| [photometry.apply](photometry.apply.md) | photometry | apply_scale |
| [geometry.overlap](geometry.overlap.md) | geometry | make_footprint_provider,detector_footprint,footprint,measure,compute_overlap,refine_geometry,footprint.forward_interval,footprint.boundary_interval,footprint.derivative_interval,refinable.open,refinable.refine |
| [operators.action](operators.action.md) | operators | make_action,apply,transpose_apply,compose,action.dependency_blocks,action.serialize_recipe,action.row_enclosure |
| [uncertainty.covariance](uncertainty.covariance.md) | uncertainty | make_covariance,covariance_apply,quadratic_form,geometry_covariance_bound,control_covariance |
| [geometry.drizzle](geometry.drizzle.md) | geometry | build_drizzle |
| [information.reference](information.reference.md) | information | estimate_flux_information |
| [information.scene](information.scene.md) | information | evaluate_scene_variance,build_supported_triangulation,reconstruct_scene_variance |
| [sky.controls](sky.controls.md) | sky | build_sky_controls |
| [sky.fit](sky.fit.md) | sky | fit_relative_sky,apply_sky |
| [coadd.select](coadd.select.md) | coadd | select_samples |
| [coadd.weights](coadd.weights.md) | coadd | solve_weights |
| [coadd.integrate](coadd.integrate.md) | coadd | coadd |
| [projection.export](projection.export.md) | projection | build_export |

## 添加或修改一个process

按[TEMPLATE](TEMPLATE.md)定义一个有明确数据边界的科学步骤，在所属模块声明一次descriptor/typed binding，提供CPU实现与独立反例。输入输出沿公开类型传递，不把每个标量原语变成调度节点。构建自动汇总注册；工作流只选择process_id/operation及显式数据边，框架无需新增算法switch。CLI、合同驱动、独立C++进程内调用必须经过同一registry边界；内部科学helper调用同一实现，不递归建立调度池。

流程组合见[WORKFLOWS](../standards/WORKFLOWS.md)；通用求解方法见[NUMERICS](../standards/NUMERICS.md)。具体实现与证据状态按模块提交和冻结验证记录，本文目录不证明其已经接线。

## 实际接入状态

本目录 22 个 process 文档和 57 个 operation 是科学责任和目标覆盖范围，**不代表每个 operation 都已通过生产级验证**。实际接入与独立证据见 [M0—M6 状态表](../../../audit/IMPLEMENTATION-STATUS.md) 及 [M5 固定用例](../../../../validation/process/README.md)。

## 独立算法复现

每篇末尾“可独立实施的算法合同”固定其步骤、参数来源、资源/停止与独立例；统一门槛见[ALGORITHM_CONTRACTS](../standards/ALGORITHM_CONTRACTS.md)。sources.measure明确窗口/联合GLS/背景/误差，response.integrate明确认证积分，geometry.drizzle明确外存CSR/误差/细化；其余模块也不能只靠骨架段落登记完成。案例目录不意味着生产已实跑。
