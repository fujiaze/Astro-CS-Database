# 内部开发probe初始登记

每项登记另有semantic_revision=1，单位从真实被观测对象取；producer/boundary/payload为唯一生产边界，后续实现将本表固化为只读登记，不按线程/输入创建新probe ID。新增科学语义必须升semantic_revision或新ID。表中存在不表示已采集；没有生产该量时记NOT_SEEN，不能重算或合成。span统一记录steady wall/cpu ns及明确的工作量/字节；counter为count，gauge为bytes。

| probe_id / kind | producer与边界 | 既有载荷、科学单位与坐标 | registration_owner |
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

## 与process登记的关系

这是初始34项加4项独立process边界，共38项需求清单，实际生产登记按registration_owner在所属模块descriptor或基建service声明一次，并由构建生成能力目录与本清单核对。科学producer/操作必须映射到唯一登记process；service.storage/service.resources的探针由服务登记并绑定当前父invocation，不能要求I/O或writer伪装科学process；同一模块在别的命令中调用时仍用相同probe语义，process_id/operation_id/invocation_id逐事件携带。默认framework.invocation.phase与framework.invocation.usage是另外两项Host通用观测，不需要科学模块复制。

原清单中io.read/io.write/resources.wait/resources.payload_peak等属于基建，科学模块不能重复注册这些ID；process文档提到它们只表示会由框架自动归因。修改某一科学观测点的边界/单位时只改所属process文档和descriptor，再生成清单，不能在这里另外定义不同payload。span单位ns，CPU不可得null，科学snapshot各列单位及坐标必须来自实际对象。

每个probe_id的registration_owner唯一；其它模块只能引用其上游结果，不重复登记或改变boundary。factory/bind的WCS观测与solver观测、校准/重组/标度观测分用不同ID。当前FITS writer的export.fits.signal由service.storage拥有，projection.export只产生Q/输出数组；writer不是额外科学算法。

服务探针有实际科学父invocation时继承其归因；初始reader/最终writer/publish等没有科学父调用时，按SYSTEM建立command子服务scope，owner_kind=service，process_id=service.storage/resources、operation_id=具名服务操作、invocation_id独立分配。parent_span_id仅引用真实command span或null，不要求存在科学父节点，也不把服务消耗重复计入父子汇总。
