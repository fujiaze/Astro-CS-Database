# 当前可观察的模块运行点（生成器兼容数据）

只保留生成器需要的标识与类型。这里列的是可用于观察实际科学计算/资源的探针，**不是自动评分门禁**；人工依据代码与真实运行情况判断问题，见 [REVIEW](../REVIEW.md)。

| ID/类型 | 实际作用点 | 数量或单位 | 归属 |
|---|---|---|---|
| `normalize.calibration.signal` / snapshot_array | calibrate_observation；after_bias_dark_flat | integrated_values,dq,numerical bounds；NativeObservation.unit；detector_pixel | observation.calibrate |
| `normalize.astrometry.solution` / snapshot_model | solve_astrometry；validated_full_domain | WcsModel,AstrometricSolution；rad and detector pixel, labelled columns；ICRS/detector_pixel | sources.astrometry |
| `normalize.psf.model` / snapshot_model | fit_psf；after_psf_fit_before_scale | EffectivePSF,existing residual arrays；parameter-specific units；detector_pixel | sources.psf |
| `normalize.photometry.measurements` / snapshot_model | measure_science_sources；before_photometric_fit | SourceMeasurements；source count unit; variance squared；detector_pixel/catalogue | sources.measure |
| `normalize.photometry.solution` / snapshot_model | fit_photometry；joint_fit_complete | PhotoSolution；theta log scale and basis conventions；detector_pixel/catalogue | photometry.fit |
| `normalize.drizzle.geometry` / snapshot_model | compute_overlap；completed_source_or_partition | Overlaps and primitive constraints；sr; not an independent random variance；ICRS/HEALPix_NESTED | geometry.overlap |
| `normalize.drizzle.signal` / snapshot_array | build_drizzle；after_same_R | SignalEstimate,DrizzleOperator；L/sr; normalizer sr；HEALPix_NESTED | geometry.drizzle |
| `normalize.information.reference_flux` / snapshot_model | estimate_flux_information；reference_information_complete | Information and requested estimator；reference L squared；ICRS | information.reference |
| `normalize.information.scene_variance` / snapshot_model | evaluate_scene_variance；actual_scene_query_complete | Variance query results；signal unit squared；HEALPix_NESTED | information.scene |
| `mosaic.sky.controls` / snapshot_model | build_sky_controls；before_sky_fit | SkyControls；control unit; covariance squared；ICRS/chart | sky.controls |
| `mosaic.sky.fit` / snapshot_model | fit_relative_sky；component_frozen | SkyFit,existing residual arrays；basis-specific sky unit；ICRS/chart | sky.fit |
| `mosaic.selection.samples` / snapshot_model | select_samples；eligibility_and_rejection_complete | Selection；dimensionless masks; labelled reasons；HEALPix_NESTED | coadd.select |
| `mosaic.weight.coefficients` / snapshot_model | solve_weights；weights_frozen | WeightSolution；dimensionless weights; Gamma signal squared；HEALPix_NESTED | coadd.weights |
| `mosaic.integration.signal` / snapshot_array | coadd；after_coadd_before_writer | SignalEstimate；SignalEstimate.unit; not numerator；HEALPix_NESTED | coadd.integrate |
| `export.projection.overlaps` / snapshot_model | build_export；target_overlap_rows_complete | existing b rows,Q,support；sr for overlap/integral; mean Q dimensionless；target_WCS_pixel | projection.export |
| `export.fits.signal` / snapshot_array | export_signal；before_FITS_encoding | output arrays and target WCS；ExportResult.estimate.unit；target_WCS_pixel | service.storage |
| `normalize.calibration.frame` / span | calibrate_observation；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | observation.calibrate |
| `normalize.astrometry.search` / span | solve_astrometry；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | sources.astrometry |
| `normalize.psf.fit` / span | fit_psf；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | sources.psf |
| `normalize.photometry.fit` / span | fit_photometry；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | photometry.fit |
| `normalize.response.integral` / span | integrate_response；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | response.integrate |
| `normalize.drizzle.overlap` / span | compute_overlap；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | geometry.overlap |
| `operators.apply` / span | apply；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | operators.action |
| `operators.transpose` / span | transpose_apply；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | operators.action |
| `uncertainty.quadratic_form` / span | quadratic_form；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | uncertainty.covariance |
| `mosaic.sky.fit_component` / span | fit_relative_sky；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | sky.fit |
| `mosaic.weights.solve` / span | solve_weights；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | coadd.weights |
| `mosaic.integration.block` / span | coadd；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | coadd.integrate |
| `export.projection.block` / span | build_export；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | projection.export |
| `io.read` / span | Storage.read/read_reserved；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | service.storage |
| `io.write` / span | ArrayBuilder.append/seal；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | service.storage |
| `resources.wait` / span | Resources/worker admission；existing bounded operation | PerfRecord；wall/cpu ns, count or bytes separately labelled；none | service.resources |
| `resources.payload_peak` / gauge | Resources.snapshot；existing accounting boundary | PerfRecord；bytes；none | service.resources |
| `geometry.precision_upgrade` / counter | compute_overlap；existing accounting boundary | PerfRecord；count；none | geometry.overlap |
| `observation.regroup.signal` / snapshot_array | regroup_observation；physical_or_linear_view_complete | NativeObservation/G/DQ；实际输出unit/index；detector_pixel | observation.regroup |
| `photometry.apply.signal` / snapshot_array | apply_scale；scaled_observation_complete | scaled values/PhotoSolution identity；输出L与数值包围；detector_pixel | photometry.apply |
| `coordinates.wcs.solution` / snapshot_model | make_wcs/bind_wcs；canonical_model_validated | WcsModel及既有solution引用；rad/pixel；ICRS/detector_pixel | coordinates.wcs |
| `catalogue.query` / span | CatalogueProvider.query/query_quads/lookup；bounded_lookup_complete | PerfRecord；wall/cpu ns，实际读取bytes/count；none | catalogue.access |
