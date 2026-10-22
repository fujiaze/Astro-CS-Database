#pragma once
// Normative declarations only; no science implementation in this design branch.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <utility>
#include <vector>
namespace acsd {
using Id = std::string; // UTF-8 NFC, nonempty; content IDs are sha256:<64 lowercase hex>
using U64 = std::uint64_t;
using U32 = std::uint32_t;
template<class T> using Ref = std::shared_ptr<const T>;
using Vec2 = std::array<double,2>;
using Vec3 = std::array<double,3>;
using Mat2 = std::array<double,4>; // row major
struct Interval { double lower, upper; };
struct Dyadic { std::string signed_integer; std::int32_t exponent2; };
struct ExactInterval { Dyadic lower, upper; U32 precision_bits; };
struct Box { Vec2 lower, upper; }; // closed detector or chart rectangle
struct PixelCenter0 { double x,y; };
struct PixelEdge0 { double x,y; };
struct FitsPixel1 { double x,y; };
struct SkyICRS { double nx,ny,nz; };
struct HealpixLeaf { U32 order; U64 ipix; };
struct FacetPoint { std::uint8_t face; double s,t; };
struct Unit { std::string scale; std::int32_t sr_power, pixel_power, nm_power;
  double multiplier; }; // scale is a named linear L, electron, ADU, photon_rate_m2, or 1
struct SourceIdentity { Id dataset,exposure,detector,channel; U64 sample; };
struct SharedId { Id external_namespace,entity,model_revision,component; };
struct Identity { Id object_id,content_hash,run_id; U64 revision;
  std::string schema_id; U32 schema_major,schema_minor; };
enum class Code { ok, invalid_request, invalid_model, domain, unsupported,
  unavailable, unsolved, not_identifiable, invalid_covariance, duplicate_source,
  identity_conflict, hash_mismatch, corrupt_product, missing_dependency,
  no_support, support_unresolved, geometry_solve_unresolved, selection_unresolved,
  nonconverged, precision_exhausted, resource_exhausted, io_error, cancelled, internal };
enum class NoiseState { known, conditional, approximate, unknown };
enum class Validity { certified, approximate, invalid };
enum class SupportState { absent, positive, bounded_undecided };
enum class Termination { tolerance_met, work, depth, precision, unsupported };
enum class ScienceState { bounded, unconverged, conditional, selection_unresolved,
  geometry_solve_unresolved };
struct Scope { Id frame,functional; std::optional<U64> block;
  std::vector<HealpixLeaf> leaves; };
struct Issue { Code code; Scope scope; std::string origin,reason,impact,retry_condition; };
struct Condition { Id estimator; std::string kind; std::vector<Id> fixed_objects,assumptions; };
struct Assessment { bool complete; Validity validity; ScienceState science;
  NoiseState noise; std::vector<std::string> included,omitted;
  std::vector<Issue> issues; Condition condition; };
struct Usage { U64 work,peak_payload_bytes,read_bytes,written_bytes,spill_bytes;
  double elapsed_seconds; };
template<class T> struct Result { Code code; std::optional<T> value;
  Assessment assessment; Usage usage; std::vector<Id> dependencies; };
struct Model { Identity identity; Assessment assessment; };
enum class DType { f64, f32, u64, u32, u16, u8, dyadic };
struct Array : Model { DType dtype; std::vector<U64> shape; Unit unit;
  Id index_id; std::vector<Id> block_ids; }; // immutable storage metadata; provider owns bytes
using ArrayRef = Ref<Array>;
struct Index : Model { U64 size; enum class Kind { native,leaf,parameter,control,pixel,latent } kind;
  ArrayRef ids; std::vector<SourceIdentity> sources; U32 order; };
struct DomainUnits { Ref<Index> domain,codomain; Unit input,output; };
struct BlockRequest { U64 begin,count; }; // half-open flattened interval, checked overflow
struct Lease { std::shared_ptr<const void> owner; }; // host implementation, nonserialized
struct ArrayView { DType dtype; std::vector<U64> shape; std::span<const std::byte> bytes;
  Lease lease; }; // little endian C-contiguous, lifetime is lease lifetime
struct Cancellation { virtual bool requested() const noexcept = 0; virtual ~Cancellation()=default; };
struct Limits { U64 bytes,work,spill_bytes; U32 threads,max_depth,max_precision_bits;
  std::string scratch_directory; }; // zero is a real zero budget, never unlimited
struct Reservation { Lease lease; U64 bytes; U32 threads; };
struct Resources { virtual Result<Reservation> reserve(U64 bytes,U32 threads)=0;
  virtual Result<U64> consume_work(U64 units)=0; virtual Usage usage() const=0;
  virtual ~Resources()=default; };
struct Storage;
struct Context { Resources& resources; const Cancellation& cancellation; Storage& storage; };
struct Storage { virtual Result<ArrayRef> copy(Context&,DType,std::span<const std::byte>,
  std::span<const U64> shape,const Unit&,Id index)=0;
  virtual Result<ArrayView> read(Context&,ArrayRef,BlockRequest) const=0;
  virtual ~Storage()=default; };
struct Arithmetic { enum class Mode { outward,approximate } mode;
  U32 initial_bits,max_bits; bool allow_target_fallback; };
struct Tolerance { double absolute,relative,absolute_floor; Unit unit; };
struct NumericBudget { Tolerance tolerance; U32 max_iterations; Arithmetic arithmetic; };
struct HeaderField { std::string key; std::variant<std::string,double,std::int64_t> value; };
using HeaderFields = std::vector<HeaderField>;
struct AstrometricContext { double epoch_julian_year,observation_jd_tdb;
  std::optional<Vec3> barycentric_observer_au;
  std::optional<Vec3> geodetic_lon_lat_height_rad_m;
  std::vector<std::string> omitted; std::optional<double> omitted_position_bound_rad; };
struct Term2 { U32 x_power,y_power; double coefficient; };
struct Polynomial2D { U32 order; std::vector<Term2> terms; };
struct InjectivityCertificate { Mat2 preconditioner; Interval eta;
  Box domain; Arithmetic arithmetic; Id proof_hash; };
struct WcsModel : Model { Vec2 crpix_fits1; Mat2 cd_rad_per_pixel;
  Vec3 c,e,north; Polynomial2D sip_a,sip_b; Box domain;
  InjectivityCertificate injectivity; ExactInterval normalization_error_rad;
  AstrometricContext epoch_frame_observer; HeaderFields original_header; };
struct InverseResult { PixelCenter0 point; Box root_box;
  Interval polynomial_residual_pixel,sky_residual_rad; };
struct DerivativeResult { std::array<double,6> jacobian; std::array<Interval,6> enclosure; };
struct Ring { std::vector<PixelEdge0> vertices; bool hole; };
struct Response;
struct EffectivePSF;
struct DetectorFootprint : Model { Ref<WcsModel> wcs; std::vector<Ring> detector_rings;
  Ref<Response> pixel_response; SourceIdentity source; PixelCenter0 contraction_center; };
struct CurvedEdge { std::vector<std::array<ExactInterval,2>> bernstein;
  Interval parameter; bool reversed; Id common_edge; };
struct CurvedBoundary : Model { std::vector<std::vector<CurvedEdge>> rings; };
struct EnclosedSkyRegion { std::array<Interval,3> xyz; }; // may over-enclose
struct QuantityRequest { Id id; enum class Kind { brightness,assigned_flux,supported_flux,
  aperture,covariance,area } kind; Ref<Index> index; ArrayRef left,right;
  Tolerance tolerance; Condition condition; }; // left/right sparse or dense functional vectors
struct LeafAreaLimit { HealpixLeaf leaf; double absolute_sr; };
struct GeometryBudget { std::vector<QuantityRequest> requested_functionals;
  std::vector<LeafAreaLimit> per_leaf_absolute_area; double area_absolute_sr,normalizer_min_sr;
  U64 max_work,max_bytes; U32 max_depth; Arithmetic arithmetic; };
struct UnresolvedDomain { std::optional<Box> detector; std::optional<std::uint8_t> face;
  std::optional<Box> facet; ExactInterval mass_sr; std::optional<HealpixLeaf> leaf; };
struct GeometryStatus { Termination termination; Validity bound_validity;
  SupportState support; std::vector<UnresolvedDomain> unresolved; };
struct SphericalAreaInterval { double lower_sr,upper_sr; Id simultaneous_bound;
  ExactInterval exact; }; // double endpoints are outward display; exact endpoints authoritative
struct OverlapContribution { SourceIdentity source; HealpixLeaf leaf;
  SphericalAreaInterval native_area,drop_area,overlap_area; Id shared_geometry_constraints; };
struct Overlaps : Model { std::vector<OverlapContribution> contributions;
  ArrayRef contribution_blocks; std::vector<UnresolvedDomain> coarse_candidates;
  GeometryStatus status; }; // inline OR block representation, not both
struct FootprintProvider { virtual Ref<WcsModel> model() const=0;
  virtual Result<EnclosedSkyRegion> forward_interval(Context&,Box) const=0;
  virtual Result<DerivativeResult> derivative_interval(Context&,Box) const=0;
  virtual Result<EnclosedSkyRegion> boundary_interval(Context&,const CurvedEdge&,Interval) const=0;
  virtual Result<SphericalAreaInterval> measure(Context&,const DetectorFootprint&,const GeometryBudget&) const=0;
  virtual ~FootprintProvider()=default; };
struct ActionRecipe { enum class Kind { identity,diagonal,csr,compose,sum,transpose,
  factor_covariance,solve,sky,selection,leaf_merge } kind;
  std::vector<Id> children,arrays; std::string algorithm; U32 version;
  DomainUnits spaces; Id error_record; }; // concrete layouts in OBJECTS
struct LinearAction : Model { DomainUnits spaces;
  virtual Result<ArrayRef> apply(Context&,ArrayRef) const=0;
  virtual Result<ArrayRef> transpose_apply(Context&,ArrayRef) const=0;
  virtual Result<std::vector<Id>> dependency_blocks(BlockRequest,bool transpose) const=0;
  virtual Result<ActionRecipe> serialize_recipe() const=0;
  virtual Result<std::vector<ExactInterval>> row_enclosure(Context&,U64 row) const=0;
  virtual ~LinearAction()=default; };
struct NoiseRoot : Model { SharedId shared_id; Ref<Index> coordinates;
  Unit unit; Ref<LinearAction> factor; }; // Croot=F F^T, including exact zero coordinates
struct UnknownTerm { Id id; std::string reason; Ref<LinearAction> influence;
  std::optional<double> spectral_norm_upper; };
struct FitInfluence { Ref<LinearAction> from_measurements; ArrayRef linearization_point;
  Condition condition; std::vector<Id> roots; };
struct NoiseModel : Model { Unit variance_unit; Condition condition;
  std::vector<Ref<NoiseRoot>> innovations,shared_factors;
  std::vector<Ref<LinearAction>> influence; std::vector<UnknownTerm> unknown_terms;
  NoiseState state; }; // one influence per concatenated root, ordered as innovations then shared
struct CovarianceAction : Model { Ref<NoiseModel> model; Ref<LinearAction> influence;
  DomainUnits input_output; std::optional<Ref<LinearAction>> absolute_factor_bound;
  std::optional<double> operator_norm_upper; std::optional<double> omitted_action_norm; };
struct Variance { std::optional<double> value; Unit unit; NoiseState state;
  std::optional<Interval> numeric_enclosure; std::vector<Id> unknown_terms; };
struct ErrorRecord : Model { std::vector<QuantityRequest> requested;
  std::vector<ExactInterval> achieved; Ref<CovarianceAction> stochastic_noise;
  std::vector<ExactInterval> finite_model_bias; std::vector<std::string> unknown_model_terms;
  Condition linearization_selection; };
struct BoundOrUnavailable { std::optional<ExactInterval> bound; std::string reason; };
struct SupportMap : Model { enum class Kind { primitives,dyadic_cells,full_leaf_approx,
  effective_area_approx } kind; Ref<Index> index; std::vector<Id> primitives;
  ArrayRef states,area_lower_sr,area_upper_sr; Id geometry_snapshot; };
struct Estimator { Id id; std::string quantity; Unit unit; Condition condition;
  std::optional<Id> reference_sed; };
struct SpatialResponse : Model { std::vector<Ref<LinearAction>> spatial_actions;
  std::vector<Ref<Response>> responses; ArrayRef weights,reference_factors;
  bool signed_response; Condition condition; }; // sum_k t_k c_k K_k, not generally separable
struct SignalEstimate : Model { ArrayRef signal; Unit unit; Estimator estimator;
  ArrayRef numerical_lower,numerical_upper; Ref<SupportMap> support;
  Ref<SpatialResponse> effective_response; Ref<CovarianceAction> noise;
  Id error_identity; }; // null noise means unknown, not zero
struct Calibration { ArrayRef bias,dark,flat; bool dark_contains_bias;
  double exposure_seconds,dark_exposure_seconds,flat_normalization;
  std::optional<double> gain_e_per_adu,read_rms_e;
  Ref<NoiseModel> raw_noise,master_noise; }; // absent maps use 0,0,1 by explicit configuration
struct RawObservation : Model { ArrayRef adu,dq; U64 width,height;
  AstrometricContext context; std::vector<SourceIdentity> sources;
  std::vector<Ref<DetectorFootprint>> footprints; };
struct NativeObservation : Model { ArrayRef integrated_values,dq; Ref<Index> native_index;
  std::vector<Ref<DetectorFootprint>> footprints; Ref<LinearAction> regrouping;
  std::vector<Id> scale_applications; Ref<NoiseModel> noise; Unit unit;
  Ref<Response> response; Ref<EffectivePSF> kernel; };
struct RegroupRequest { Ref<LinearAction> G; std::vector<Ref<DetectorFootprint>> output_footprints;
  Mat2 old_from_new; Vec2 offset; bool input_is_mean; U32 mean_member_count; };
struct Spectrum : Model { ArrayRef wavelength_nm,flux_W_m2_nm; Ref<CovarianceAction> noise;
  Interval support_nm; std::string interpolation; };
struct SpectralBasis : Model { ArrayRef knots_nm,basis_coefficients; U32 degree,coefficient_count;
  Interval support_nm; }; // piecewise polynomial, row order in OBJECTS
struct Response : Model { enum class Convention { photon_throughput,includes_photon_conversion } convention;
  ArrayRef wavelength_nm,throughput; std::optional<Vec2> top_hat_center_width_nm;
  Interval support_nm; std::string provenance,normalization;
  Ref<NoiseModel> shape_noise; std::vector<double> breakpoints_nm; };
struct ReferenceSED : Model { Ref<Spectrum> spectrum; Ref<Response> target_response; };
struct SpectralPrediction : Model { ArrayRef basis_integrals,absolute_errors;
  Ref<SpectralBasis> basis; Ref<Response> response; Interval support_nm;
  std::vector<SharedId> shared_spectral_error; };
struct CatalogueStar { U64 source_id; double ra_rad,dec_rad,epoch_year,pmra_cosdec_rad_year,
  pmdec_rad_year,parallax_rad; std::optional<double> radial_velocity_m_s;
  ArrayRef xp_coefficients; Ref<CovarianceAction> astrometric_noise,xp_noise; };
struct CatalogueProvider { virtual Result<std::vector<CatalogueStar>> query(Context&,SkyICRS,double radius_rad,double epoch) const=0;
  virtual Id dataset_hash() const=0; virtual ~CatalogueProvider()=default; };
struct SolverRequest { std::vector<SkyICRS> centers; Interval scale_rad_pixel;
  U32 max_candidates,max_hypotheses,sip_order; double detect_snr,match_radius_pixel;
  U32 min_matches; NumericBudget budget; }; // empty centers = full index search
struct AstrometricSolution : Model { Ref<WcsModel> wcs; ArrayRef parameters;
  Ref<CovarianceAction> parameter_noise; FitInfluence influence;
  ArrayRef solver_detections; Box support; std::optional<double> model_bias_rad; };
struct PsfRequest { enum class Family { gaussian,moffat,airy } family;
  U32 spatial_degree; double initial_width_pixel,tail_absolute_flux,window_radius_pixel;
  NumericBudget budget; };
struct EffectivePSF : Model { PsfRequest::Family family; ArrayRef parameters;
  Box spatial_domain; Interval wavelength_nm; double tail_mass_upper;
  Ref<LinearAction> pixel_integral,regroup_scale_project;
  FitInfluence influence; }; // untruncated normalized family; window does not renormalize
using ObservationKernel = EffectivePSF;
struct StarFlux { U64 gaia_source_id; PixelCenter0 position; double integrated_flux;
  Variance variance; std::string definition; Box window; FitInfluence influence; };
struct SourceMeasurements : Model { Id frame; std::vector<StarFlux> stars; Ref<EffectivePSF> psf;
  Ref<CovarianceAction> joint_noise; ArrayRef training_pixel_ids; };
struct RankReport { U64 columns,rank; ArrayRef singular_values,nullspace;
  double singular_error_bound; bool certified; };
struct Basis { enum class Kind { constant,plane,polynomial } kind;
  U32 degree; Box normalization_domain; Vec2 center,scale; ArrayRef weighted_mean; };
struct SolvePolicy { double relative_residual,absolute_residual,rank_relative;
  U32 max_iterations; U64 dense_max_bytes; bool huber; double huber_delta; };
struct PhotometryRequest { std::vector<Ref<Response>> responses; Ref<ReferenceSED> reference;
  Basis basis; std::vector<Id> frames,reference_frames; Id application_id;
  SolvePolicy solve; double gradient_tolerance,step_tolerance; U32 max_outer_iterations;
  Condition likelihood; }; // baseline joint_flux_gaussian; other likelihoods unsupported
struct PhotoSolution : Model { ArrayRef theta; Ref<CovarianceAction> covariance;
  Basis spatial_basis; Box support; std::vector<Ref<Response>> responses;
  Ref<ReferenceSED> reference; RankReport rank; FitInfluence influence;
  std::vector<Id> frames; Id application_id; };
struct DrizzleRequest { U32 order; double pixfrac; GeometryBudget budget; };
struct DrizzleOperator : Model { Ref<LinearAction> rows; Ref<Index> source_index;
  Ref<Overlaps> bounds; ArrayRef normalizer_sr; Ref<SupportMap> support; GeometryStatus status; };
struct DrizzleResult { Ref<DrizzleOperator> op; Ref<SignalEstimate> estimate; };
struct GeometrySnapshot : Model { std::vector<Ref<Overlaps>> overlaps;
  std::vector<Ref<DrizzleOperator>> operators; std::vector<Id> dependents; };
struct Replacement { Id old_hash,new_hash; };
struct GeometryRevision : Model { Ref<GeometrySnapshot> base,next;
  std::vector<SourceIdentity> affected_sources; std::vector<HealpixLeaf> affected_leaves;
  std::vector<Box> affected_domains; std::vector<Id> affected_constraints;
  std::vector<Replacement> replacements; Usage consumed;
  GeometryStatus status; }; // transaction invalidation, never in-place change
struct RefinableGeometryProvider { virtual Result<Ref<GeometrySnapshot>> open(Context&,Id snapshot) const=0;
  virtual Result<Ref<GeometryRevision>> refine(Context&,Ref<GeometrySnapshot>,const GeometryBudget&) const=0;
  virtual ~RefinableGeometryProvider()=default; };
struct InformationRequest { ArrayRef h; Ref<LinearAction> background;
  Ref<CovarianceAction> covariance; Estimator reference; SolvePolicy solve; };
struct Information { Variance flux_variance; RankReport rank; };
struct PrecisionControls : Model { enum class Kind { reference_flux,actual_scene,control_statistic } kind;
  ArrayRef positions,values,log_errors; Estimator estimator; Condition condition;
  Ref<SupportMap> support; std::optional<double> hessian_norm_upper; };
struct Chart { SkyICRS center; double maximum_radius_rad; };
struct SupportedTriangulation : Model { Chart chart; ArrayRef vertices,triangles;
  std::vector<Ring> holes; }; // triangles must be wholly in observed support
struct VariancePrediction { ArrayRef values,lower,upper,support_states;
  Estimator estimator; bool conditional; };
struct FrameDescription : Model { Id frame; U32 order; Ref<Index> index;
  Estimator estimator; Ref<SupportMap> support; Ref<EffectivePSF> psf;
  Ref<SpatialResponse> response; Ref<CovarianceAction> noise;
  Ref<LinearAction> native_to_leaf; std::vector<std::string> capabilities; };
struct LeafBlock { Ref<FrameDescription> description; ArrayRef signal,dq;
  Ref<Index> index; std::vector<ArrayRef> halo; Lease lease; };
struct FrameReadRequest { Id frame; std::vector<HealpixLeaf> leaves,halo;
  std::vector<std::string> fields; };
struct FramesProvider { virtual Result<std::vector<Id>> list_frames(Context&) const=0;
  virtual Result<Ref<FrameDescription>> describe_frame(Context&,Id) const=0;
  virtual Result<LeafBlock> read_leaf_block(Context&,const FrameReadRequest&) const=0;
  virtual Result<std::vector<ArrayRef>> read_native_dependencies(Context&,std::span<const SourceIdentity>) const=0;
  virtual Result<Ref<LinearAction>> open_action(Context&,Id) const=0;
  virtual Result<Ref<SupportMap>> query_support(Context&,Id,Ref<Index>) const=0;
  virtual Result<Ref<RefinableGeometryProvider>> open_refinable_geometry(Context&,Id) const=0;
  virtual ~FramesProvider()=default; };
using ScientificFrameView = FrameDescription;
struct Comparability { std::vector<Id> frames; Id response_proof,spatial_kernel_proof;
  Ref<LinearAction> matching; Condition assumptions; };
struct Gauge { enum class Kind { reference_frame,weighted_sum } kind;
  Id reference; ArrayRef weights; };
struct SkyRequest { std::vector<Comparability> groups; Basis basis; Gauge gauge;
  Ref<LinearAction> T,H; Ref<CovarianceAction> control_noise; SolvePolicy solve;
  bool allow_prediction; }; // T/H may be null only for standard patch builder
struct SkyControls : Model { ArrayRef c; Ref<LinearAction> T,H,A,E;
  Ref<CovarianceAction> noise; ArrayRef component_ids,positions;
  Ref<SupportMap> training_support; };
struct SkyFit : Model { Basis basis; Gauge gauge; ArrayRef beta;
  Ref<LinearAction> K,T,E,P; Ref<CovarianceAction> parameter_cov;
  RankReport rank; Ref<SupportMap> support; ArrayRef component_ids; };
struct RejectionPolicy { enum class Kind { dq_only,single_max_loo } kind;
  double alpha; Ref<CovarianceAction> external_fixed_covariance; Id common_mean_proof; };
enum class WeightPolicy { automatic,equal,blue,nonnegative_qp };
struct CoaddRequest { Estimator target; bool mixed_observation; WeightPolicy weights;
  RejectionPolicy rejection; std::vector<QuantityRequest> quantities; SolvePolicy solve;
  std::vector<Comparability> groups; };
struct Selection : Model { Ref<Index> index; ArrayRef eligibility,accepted,reasons;
  RejectionPolicy policy; Condition condition; };
struct WeightSolution : Model { ArrayRef coefficients; Ref<Index> samples;
  WeightPolicy policy; Ref<CovarianceAction> covariance; Ref<Selection> selection;
  Condition condition; };
struct CoaddResult { Ref<SignalEstimate> estimate; Ref<Selection> selection;
  Ref<WeightSolution> weights; Ref<SkyFit> sky; };
struct OutputWcs { Ref<WcsModel> model; U64 width,height; };
struct ExportRequest { OutputWcs target; enum class Quantity { supported_mean,supported_flux,
  extrapolated_full_flux } quantity; SupportMap::Kind support_policy;
  bool assume_uniform_uncovered; DType output_dtype; GeometryBudget budget; };
struct ExportResult { Ref<LinearAction> Q; Ref<SignalEstimate> estimate; OutputWcs target; };
struct ProductGraph : Model { enum class Stage { normalize,mosaic,export_image } stage;
  Ref<SignalEstimate> signal; std::vector<Ref<Model>> closure;
  std::vector<Id> inputs; std::string profile; };
struct ProductLocation { std::string directory; Id manifest_hash; };
struct ProductProvider : FramesProvider { virtual Ref<ProductGraph> graph() const=0;
  virtual Result<ArrayView> read_array(Context&,ArrayRef,BlockRequest) const=0; };
struct PublishRequest { std::string destination; DType signal_dtype;
  std::string profile; U32 tile_log2_width; bool durable; };
struct NormalizeFrame { Id frame; Ref<RawObservation> raw; Calibration calibration;
  Ref<WcsModel> supplied_wcs; Ref<Response> response; };
struct NormalizeRequest { std::vector<NormalizeFrame> frames; SolverRequest astrometry;
  PsfRequest psf; PhotometryRequest photometry; DrizzleRequest drizzle;
  std::vector<InformationRequest> information; bool allow_relative_uncalibrated;
  std::vector<PublishRequest> outputs; };
struct NormalizeResult { std::vector<ProductLocation> published;
  std::vector<std::pair<Id,Assessment>> frames; };
struct MosaicRequest { std::vector<ProductLocation> inputs; SkyRequest sky;
  CoaddRequest coaddition; PublishRequest output; };
struct ExportWorkflowRequest { ProductLocation input; ExportRequest projection;
  PublishRequest output; };
// All functions synchronous. Context and spans borrowed through return; Ref outputs retain dependencies.
Result<Ref<WcsModel>> make_wcs(Context&,const HeaderFields&,Box,const AstrometricContext&,const NumericBudget&);
Result<Ref<LinearAction>> make_action(Context&,const ActionRecipe&,
  std::span<const Ref<LinearAction>> children,std::span<const ArrayRef> arrays);
Result<Ref<CovarianceAction>> make_covariance(Context&,Ref<NoiseModel>);
Result<Ref<FramesProvider>> make_frames_provider(Context&,std::span<const Ref<FrameDescription>>,
  std::span<const ArrayRef> signals,std::span<const ArrayRef> dq);
FitsPixel1 to_fits(PixelCenter0); FitsPixel1 to_fits(PixelEdge0);
PixelCenter0 from_fits_center(FitsPixel1); PixelEdge0 from_fits_edge(FitsPixel1);
Result<SkyICRS> forward(Context&,Ref<WcsModel>,PixelCenter0,const NumericBudget&);
Result<SkyICRS> forward(Context&,Ref<WcsModel>,PixelEdge0,const NumericBudget&);
Result<InverseResult> inverse(Context&,Ref<WcsModel>,SkyICRS,const NumericBudget&);
Result<DerivativeResult> derivative(Context&,Ref<WcsModel>,PixelCenter0,const NumericBudget&);
Result<Interval> solid_angle_jacobian(Context&,Ref<WcsModel>,Box,const NumericBudget&);
Result<Ref<CurvedBoundary>> footprint(Context&,const FootprintProvider&,Ref<DetectorFootprint>,double pixfrac,const GeometryBudget&);
Result<SphericalAreaInterval> measure(Context&,const FootprintProvider&,Ref<DetectorFootprint>,const GeometryBudget&);
Result<Ref<Overlaps>> compute_overlap(Context&,const FootprintProvider&,Ref<DetectorFootprint>,U32 order,double pixfrac,const GeometryBudget&);
Result<Ref<NativeObservation>> calibrate_observation(Context&,Ref<RawObservation>,const Calibration&);
Result<Ref<NativeObservation>> regroup_observation(Context&,Ref<NativeObservation>,const RegroupRequest&);
Result<Ref<AstrometricSolution>> solve_astrometry(Context&,Ref<NativeObservation>,const CatalogueProvider&,const SolverRequest&);
Result<Ref<EffectivePSF>> fit_psf(Context&,Ref<NativeObservation>,Ref<WcsModel>,std::span<const CatalogueStar>,const PsfRequest&);
Result<Ref<SourceMeasurements>> measure_science_sources(Context&,Ref<NativeObservation>,Ref<WcsModel>,std::span<const CatalogueStar>,Ref<EffectivePSF>,const PsfRequest&);
Result<Ref<SpectralPrediction>> integrate_response(Context&,Ref<Response>,Ref<SpectralBasis>,const NumericBudget&);
Result<Ref<PhotoSolution>> fit_photometry(Context&,std::span<const Ref<SourceMeasurements>>,std::span<const CatalogueStar>,std::span<const Ref<SpectralPrediction>>,const PhotometryRequest&);
Result<Ref<NativeObservation>> apply_scale(Context&,Ref<PhotoSolution>,Ref<NativeObservation>,Id frame);
Result<DrizzleResult> build_drizzle(Context&,Ref<Overlaps>,Ref<NativeObservation>,const DrizzleRequest&);
Result<Ref<GeometryRevision>> refine_geometry(Context&,const RefinableGeometryProvider&,Ref<GeometrySnapshot>,const GeometryBudget&);
Result<ArrayRef> apply(Context&,Ref<LinearAction>,ArrayRef);
Result<ArrayRef> transpose_apply(Context&,Ref<LinearAction>,ArrayRef);
Result<Ref<LinearAction>> compose(Context&,Ref<LinearAction> outer,Ref<LinearAction> inner);
Result<ArrayRef> covariance_apply(Context&,Ref<CovarianceAction>,ArrayRef);
Result<Variance> quadratic_form(Context&,Ref<CovarianceAction>,ArrayRef);
Result<BoundOrUnavailable> geometry_covariance_bound(Context&,Ref<CovarianceAction>,Ref<LinearAction> nominal,Ref<LinearAction> absolute_error,ArrayRef functional);
Result<Information> estimate_flux_information(Context&,const InformationRequest&);
Result<std::vector<Variance>> evaluate_scene_variance(Context&,const FramesProvider&,Id frame,std::span<const HealpixLeaf>);
Result<Ref<CovarianceAction>> control_covariance(Context&,Ref<CovarianceAction>,Ref<LinearAction> T);
Result<VariancePrediction> reconstruct_scene_variance(Context&,Ref<PrecisionControls>,Ref<SupportedTriangulation>,std::span<const SkyICRS>);
Result<Ref<SkyControls>> build_sky_controls(Context&,const FramesProvider&,const SkyRequest&);
Result<Ref<SkyFit>> fit_relative_sky(Context&,Ref<SkyControls>,const SkyRequest&);
Result<Ref<SignalEstimate>> apply_sky(Context&,Ref<SignalEstimate>,Ref<SkyFit>);
Result<Ref<Selection>> select_samples(Context&,const FramesProvider&,const CoaddRequest&);
Result<Ref<WeightSolution>> solve_weights(Context&,const FramesProvider&,Ref<Selection>,const CoaddRequest&);
Result<CoaddResult> coadd(Context&,const FramesProvider&,Ref<SkyFit>,const CoaddRequest&);
Result<ExportResult> build_export(Context&,const ProductProvider&,const ExportRequest&);
Result<ProductLocation> export_signal(Context&,const ExportResult&,const PublishRequest&);
Result<Ref<ProductProvider>> open_product(Context&,const ProductLocation&);
Result<ProductLocation> publish_product(Context&,Ref<ProductGraph>,const PublishRequest&);
Result<NormalizeResult> normalize(Context&,const CatalogueProvider&,const NormalizeRequest&);
Result<ProductLocation> mosaic(Context&,const MosaicRequest&);
Result<ProductLocation> export_image(Context&,const ExportWorkflowRequest&);
} // namespace acsd
