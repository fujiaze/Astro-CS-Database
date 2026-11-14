# 现有V2注册清单（代码生成器输入）

仅给已经接入的 typed bindings 代码生成器提供操作ID，不代表必须研发22种算法。真正的代码模块与内存管道在 [MODULES](../MODULES.md) 和 [PIPELINE](../PIPELINE.md)。这些登记会在旧main算法迁入时与实际注册一起删除/合并；不可把登记数量当作质量指标。

| 已登记过程 | 当前模块 | 操作ID |
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
| [geometry.drizzle](geometry.drizzle.md) | geometry | build_drizzle,drizzle_grid |
| [information.reference](information.reference.md) | information | estimate_flux_information |
| [information.scene](information.scene.md) | information | evaluate_scene_variance,build_supported_triangulation,reconstruct_scene_variance |
| [sky.controls](sky.controls.md) | sky | build_sky_controls |
| [sky.fit](sky.fit.md) | sky | fit_relative_sky,apply_sky |
| [coadd.select](coadd.select.md) | coadd | select_samples |
| [coadd.weights](coadd.weights.md) | coadd | solve_weights |
| [coadd.integrate](coadd.integrate.md) | coadd | coadd |
| [projection.export](projection.export.md) | projection | build_export |
