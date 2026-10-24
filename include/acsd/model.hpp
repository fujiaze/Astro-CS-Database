#pragma once
// Normative declarations for the scientific data model.
//
// This header is the single owner of the public science DTOs and free
// function signatures.  <acsd/contracts.hpp> is the aggregate entry point and
// includes this header together with the runtime and module headers.  Every
// type is defined exactly once.
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
struct Box { Vec2 lower{0.0,0.0}; Vec2 upper{0.0,0.0}; }; // closed detector or chart rectangle
// The bounds are default initialised: a default constructed rectangle must be
// deterministic, not indeterminate.
struct PixelCenter0 { double x,y; };
struct PixelEdge0 { double x,y; };
struct FitsPixel1 { double x,y; };
struct SkyICRS { double nx,ny,nz; };
struct HealpixLeaf { U32 order; U64 ipix; };
struct FacetPoint { std::uint8_t face; double s,t; };
struct Unit { std::string scale; std::int32_t sr_power, pixel_power, nm_power;
  double multiplier; std::int32_t linear_power; }; // named scale L, with explicit power (variance=2)
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
enum class ExecutionState { complete,cancelled,failed };
enum class Capability { supported,unsupported,unavailable };
struct Scope { Id frame,functional; std::optional<U64> block;
  std::vector<HealpixLeaf> leaves; };
struct Issue { Code code; Scope scope; std::string origin,reason,impact,retry_condition; };
struct Condition { Id estimator; std::string kind; std::vector<Id> fixed_objects,assumptions; };
struct Assessment { bool complete; Validity validity; ScienceState science;
  NoiseState noise; std::vector<std::string> included,omitted;
  // The bound of a position error that is omitted on purpose (for example the
  // topocentric correction of a catalogue position).  It is absent when the
  // omission is unbounded; an omitted error is never zero.
  std::optional<double> omitted_position_bound_rad;
  std::vector<Issue> issues; Condition condition; ExecutionState execution; Capability capability; };
struct Usage { U64 work,peak_payload_bytes,read_bytes,written_bytes,spill_bytes;
  double elapsed_seconds; };
template<class T> struct Result { Code code; std::optional<T> value;
  Assessment assessment; Usage usage; std::vector<Id> dependencies; };
struct Model { Identity identity; Assessment assessment; };
enum class DType { f64, f32, u64, u32, u16, u8, dyadic };
struct Array : Model { DType dtype; std::vector<U64> shape; Unit unit;
  Id index_id; std::vector<Id> block_ids; }; // immutable storage metadata; provider owns bytes
using ArrayRef = Ref<Array>;
struct BlockDescriptor { Id id; std::string path; DType dtype; std::vector<U64> shape;
  Id index_id; Unit unit; U64 begin,count; enum class Codec { raw,zstd } codec;
  U64 raw_bytes,stored_bytes; Id raw_sha256; };
struct SourcePartition { Id dataset,exposure,detector,channel;
  U64 index_begin,count,first_sample,sample_stride; };
struct Index : Model { U64 size; enum class Kind { native,leaf,parameter,control,pixel,latent } kind;
  ArrayRef ids; std::vector<SourceIdentity> sources; U32 order;
  std::vector<SourcePartition> partitions; Id namespace_id; }; // sources inline only <=4096
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
struct Storage { virtual Result<ArrayRef> copy(Context& context,DType dtype,std::span<const std::byte> bytes,
  std::span<const U64> shape,const Unit& unit,Id index)=0;
  virtual Result<ArrayView> read(Context& context,ArrayRef array,BlockRequest range) const=0;
  virtual Result<ArrayRef> open(Context& context,std::string root,const Array& description,
    std::span<const BlockDescriptor> blocks)=0;
  virtual ~Storage()=default; };
struct Arithmetic { enum class Mode { outward,approximate } mode;
  U32 initial_bits,max_bits; bool allow_target_fallback; };
struct Tolerance { double absolute,relative,absolute_floor; Unit unit; };
struct NumericBudget { Tolerance tolerance; U32 max_iterations; Arithmetic arithmetic; };
struct SolvePolicy { double relative_residual,absolute_residual,rank_relative;
  U32 max_iterations; U64 dense_max_bytes; bool huber; double huber_delta; };
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
  AstrometricContext epoch_frame_observer; HeaderFields original_header; Id detector_frame; };
struct InverseResult { PixelCenter0 point; Box root_box;
  Interval polynomial_residual_pixel,sky_residual_rad; };
struct DerivativeResult { std::array<double,6> jacobian; std::array<Interval,6> enclosure; };
struct Ring { std::vector<PixelEdge0> vertices; bool hole; };
struct Response;
struct EffectivePSF;
struct PixelResponse;
struct NoiseModel;
struct LinearAction;
struct ErrorRecord;
struct AstrometricSolution;
struct PhotoSolution;
struct DetectorFootprint : Model { Ref<WcsModel> wcs; std::vector<Ring> detector_rings;
  Ref<PixelResponse> pixel_response; SourceIdentity source; PixelCenter0 contraction_center; };
struct PixelResponse : Model { std::vector<Box> cells; ArrayRef gains;
  std::vector<double> wavelength_edges_nm; bool relative_mean_one;
  Ref<NoiseModel> uncertainty; }; // gains[wavelength-bin,cell]; empty cells denote uniform PRF
struct FootprintExceptions : Model { ArrayRef sample_rows,ring_offsets,vertex_offsets,
  vertices,hole_flags; };
struct FootprintSet : Model { Ref<Index> index; U64 width,height;
  Mat2 detector_from_sample; Vec2 offset; Box local_cell;
  Ref<WcsModel> wcs; Ref<PixelResponse> pixel_response;
  Ref<FootprintExceptions> exceptions;
  std::vector<Ref<DetectorFootprint>> inline_footprints; Id detector_frame;
  Ref<AstrometricSolution> astrometry; ArrayRef sample_response_gain; };
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
struct GeometryPrimitiveTable : Model { Ref<Index> source_index;
  ArrayRef source_rows,leaf_orders,leaf_ipix,kinds,lower,upper; U32 precision_bits; };
struct GeometryConstraints : Model { Ref<GeometryPrimitiveTable> primitives;
  Ref<LinearAction> matrix; ArrayRef lower,upper; }; // lower <= matrix * primitive_mass <= upper
struct GeometryRecipe : Model { Ref<FootprintSet> footprints; Ref<Index> source_index;
  ArrayRef source_rows; U32 order; double pixfrac;
  GeometryBudget initial_budget; Arithmetic arithmetic; std::string algorithm; U32 version; };
struct OverlapTable : Model { ArrayRef source_rows,leaf_orders,leaf_ipix,
  native_primitive,drop_primitive,overlap_primitive; };
struct Overlaps : Model { std::vector<OverlapContribution> contributions;
  Ref<OverlapTable> contribution_blocks; std::vector<UnresolvedDomain> coarse_candidates;
  Ref<GeometryRecipe> recipe; Ref<GeometryPrimitiveTable> primitives; Ref<GeometryConstraints> constraints;
  GeometryStatus status; }; // inline OR block representation, not both
struct FootprintProvider { virtual Ref<WcsModel> model() const=0;
  virtual Result<EnclosedSkyRegion> forward_interval(Context&,Box) const=0;
  virtual Result<DerivativeResult> derivative_interval(Context&,Box) const=0;
  virtual Result<EnclosedSkyRegion> boundary_interval(Context&,const CurvedEdge&,Interval) const=0;
  virtual Result<SphericalAreaInterval> measure(Context&,const DetectorFootprint&,const GeometryBudget&) const=0;
  virtual ~FootprintProvider()=default; };
struct SolveSpec : Model { enum class Kind { square,least_squares } kind;
  Ref<LinearAction> column_lift,row_lift,preconditioner;
  ArrayRef column_scales; U64 reduced_rank; bool rank_certified;
  double rank_error; SolvePolicy policy; std::vector<Id> gauge_constraints; };
struct ActionRecipe { enum class Kind { identity,diagonal,csr,compose,sum,transpose,
  factor_covariance,solve,sky,selection,leaf_merge } kind;
  std::vector<Id> children,arrays; std::string algorithm; U32 version;
  DomainUnits spaces; Ref<ErrorRecord> error_record; Ref<SolveSpec> solve; }; // solve null except for solve kind
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
  ArrayRef states,area_lower_sr,area_upper_sr; Id geometry_snapshot;
  ArrayRef cell_face,cell_depth,cell_x,cell_y,cell_state;
  ArrayRef normalization_states,observational_states; }; // separate geometry/numeric/observational support
struct Estimator { Id id; std::string quantity; Unit unit; Condition condition;
  std::optional<Id> reference_sed; };
struct SpatialResponse : Model { std::vector<Ref<LinearAction>> spatial_actions;
  std::vector<Ref<Response>> responses; ArrayRef weights,reference_factors;
  bool signed_response; Condition condition; }; // sum_k t_k c_k K_k, not generally separable
struct SignalEstimate : Model { ArrayRef signal; Unit unit; Estimator estimator;
  ArrayRef numerical_lower,numerical_upper; Ref<SupportMap> support;
  Ref<SpatialResponse> effective_response; Ref<CovarianceAction> noise;
  Ref<ErrorRecord> error; }; // null noise means unknown, not zero
struct Calibration { ArrayRef bias,dark,flat; bool dark_contains_bias;
  double exposure_seconds,dark_exposure_seconds,flat_normalization;
  std::optional<double> gain_e_per_adu,read_rms_e;
  Ref<NoiseModel> raw_noise,master_noise; }; // absent maps use 0,0,1 by explicit configuration
struct RawObservation : Model { ArrayRef adu,dq; U64 width,height;
  AstrometricContext context; Ref<Index> source_index; Ref<FootprintSet> footprints; Id frame; };
struct NativeObservation : Model { ArrayRef integrated_values,dq; Ref<Index> native_index;
  Ref<FootprintSet> footprints; Ref<LinearAction> regrouping;
  std::vector<Id> scale_applications; Ref<NoiseModel> noise; Unit unit;
  Ref<Response> response; Ref<EffectivePSF> kernel;
  Ref<NativeObservation> native_parent; bool physical_collection;
  AstrometricContext context; Id frame;
  ArrayRef numerical_lower,numerical_upper; Ref<ErrorRecord> error; };
struct RegroupRequest { Ref<LinearAction> G; Ref<FootprintSet> output_footprints;
  Mat2 old_from_new; Vec2 offset; bool input_is_mean; U32 mean_member_count;
  enum class Mode { physical_sum,physical_mean,linear_view } mode; };
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
  double brightness_mag; Ref<SpectralBasis> xp_basis; ArrayRef xp_coefficients;
  Ref<CovarianceAction> astrometric_noise,xp_noise; };
struct SkySearchRegion { SkyICRS center; double radius_rad; };
struct CatalogueMetadata : Model { Id release,calibration;
  Ref<SpectralBasis> xp_basis; Interval quad_baseline_rad;
  U32 sky_order,max_stars_per_cell,neighbors_per_anchor,quads_per_anchor;
  std::string index_algorithm; bool index_complete; U64 star_count,quad_count; };
struct CatalogueQuery { SkySearchRegion region; double epoch_julian_year;
  U32 page_size; Id cursor; };
struct CataloguePage { std::vector<CatalogueStar> stars; Id next_cursor; };
struct QuadQuery { std::array<Interval,4> descriptor; Interval baseline_rad;
  std::vector<SkySearchRegion> regions; U32 page_size; Id cursor; };
struct QuadMatch { std::array<U64,4> source_ids; std::array<double,4> descriptor;
  double baseline_rad; SkyICRS center; };
struct QuadPage { std::vector<QuadMatch> matches; Id next_cursor; };
struct CatalogueColumns : Model { ArrayRef source_ids,astrometry,brightness_mag,
  xp_coefficients; Ref<SpectralBasis> xp_basis;
  Ref<NoiseModel> astrometric_noise,xp_noise; };
struct CatalogueBuildRequest { Ref<CatalogueColumns> columns; U32 sky_order,
  max_stars_per_cell,neighbors_per_anchor,quads_per_anchor;
  Interval quad_baseline_rad; Id release,calibration; };
struct CatalogueIndex : Model { CatalogueMetadata metadata; Ref<CatalogueColumns> columns;
  ArrayRef quad_source_ids,quad_descriptors,quad_baselines,quad_centers,
  kd_left,kd_right,kd_axis,kd_split,kd_bounds; };
struct CatalogueProvider { virtual Result<Ref<CatalogueMetadata>> describe(Context& context) const=0;
  virtual Result<CataloguePage> query(Context& context,const CatalogueQuery& request) const=0;
  virtual Result<QuadPage> query_quads(Context& context,const QuadQuery& request) const=0;
  virtual Result<std::vector<CatalogueStar>> lookup(Context& context,std::span<const U64> source_ids) const=0;
  virtual Id dataset_hash() const=0; virtual ~CatalogueProvider()=default; };
struct SolverRequest { std::vector<SkySearchRegion> regions; Interval scale_rad_pixel;
  U32 max_candidates,max_hypotheses,sip_order; double detect_snr,match_radius_pixel;
  U32 min_matches; double quad_descriptor_tolerance; NumericBudget budget; }; // empty regions = full index
struct AstrometricSolution : Model { Ref<WcsModel> wcs; ArrayRef parameters;
  Ref<CovarianceAction> parameter_noise; FitInfluence influence;
  ArrayRef solver_detections; Box support; std::optional<double> model_bias_rad; };
struct Basis { enum class Kind { constant,plane,polynomial } kind;
  U32 degree; Box normalization_domain;
  // The centre and the scale of the basis have usable defaults: a request
  // that does not need a spatial basis must not carry indeterminate values.
  Vec2 center{0.0,0.0},scale{1.0,1.0}; ArrayRef weighted_mean; };
struct PsfRequest { enum class Family { gaussian,moffat,airy } family;
  U32 spatial_degree; double initial_width_pixel,tail_absolute_flux,window_radius_pixel;
  NumericBudget budget; };
struct EffectivePSF : Model { PsfRequest::Family family; ArrayRef parameters;
  Box spatial_domain; Interval wavelength_nm; double tail_mass_upper;
  Basis spatial_basis; bool achromatic; Ref<FootprintSet> detector_samples;
  Ref<LinearAction> regroup_scale_project;
  FitInfluence influence; ArrayRef parameter_lower,parameter_upper; Ref<ErrorRecord> error;
  Ref<PhotoSolution> source_scale; Id frame; }; // source_scale null before photometric application
using ObservationKernel = EffectivePSF;
struct PsfEvaluationRequest { std::vector<PixelCenter0> source_positions; NumericBudget budget; std::vector<U64> output_rows; };
struct PsfEvaluation : Model { Ref<EffectivePSF> psf; std::vector<PixelCenter0> source_positions;
  ArrayRef values,numerical_lower,numerical_upper,native_tail_upper; Ref<ErrorRecord> error;
  Ref<Index> output_index,source_index; };
struct StarFlux { U64 gaia_source_id; PixelCenter0 position; double integrated_flux;
  Variance variance; std::string definition; Box window; FitInfluence influence; };
struct SourceMeasurements : Model { Id frame; std::vector<StarFlux> stars; Ref<EffectivePSF> psf;
  Ref<CovarianceAction> joint_noise; ArrayRef training_pixel_ids;
  ArrayRef flux_numerical_lower,flux_numerical_upper; Ref<ErrorRecord> error;
  Ref<NativeObservation> observation; };
struct RankReport { U64 columns,rank; ArrayRef singular_values,nullspace;
  double singular_error_bound; bool certified; };
struct PhotometryRequest { std::vector<Ref<Response>> responses; Ref<ReferenceSED> reference;
  Basis basis; std::vector<Id> frames; Id application_id;
  SolvePolicy solve; double gradient_tolerance,step_tolerance; U32 max_outer_iterations;
  Condition likelihood; }; // baseline joint_flux_gaussian; other likelihoods unsupported
struct PhotoSolution : Model { ArrayRef theta; Ref<CovarianceAction> covariance;
  Basis spatial_basis; Box support; std::vector<Ref<Response>> responses;
  Ref<ReferenceSED> reference; RankReport rank; FitInfluence influence;
  std::vector<Id> frames; Id application_id;
  ArrayRef parameter_lower,parameter_upper; Ref<ErrorRecord> error; };
struct DrizzleRequest { U32 order; double pixfrac; GeometryBudget budget; };
struct DrizzleOperator : Model { Ref<LinearAction> rows; Ref<Index> source_index;
  Ref<Overlaps> bounds; ArrayRef normalizer_sr; Ref<SupportMap> support; GeometryStatus status; };
struct DrizzleResult { Ref<DrizzleOperator> op; Ref<SignalEstimate> estimate; };
struct GeometrySnapshot : Model { std::vector<Ref<Overlaps>> overlaps;
  std::vector<Ref<DrizzleOperator>> operators; std::vector<Id> dependents;
  std::vector<Ref<GeometryRecipe>> recipes; std::vector<Ref<NativeObservation>> source_inputs;
  std::vector<QuantityRequest> functionals; };
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
  Ref<CovarianceAction> covariance; Estimator reference; SolvePolicy solve;
  Ref<PsfEvaluation> template_evaluation; U64 template_column; }; // optional for exact matrix h
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
struct ObjectEntry { Id hash; std::string path,type; U32 major,minor; };
struct AlgorithmIdentity { std::string name,version; Id source_hash; };
struct ProductManifest { std::string format; U32 major,minor; ProductGraph::Stage stage;
  Id product_id; U64 revision; Id root_object; std::string profile;
  std::vector<std::string> capabilities; std::vector<Id> inputs;
  std::vector<ObjectEntry> objects; std::vector<BlockDescriptor> blocks;
  Id configuration_hash; std::vector<AlgorithmIdentity> algorithm_set;
  Arithmetic arithmetic; Assessment assessment; bool complete; };
struct ProductLocation { std::string directory; Id manifest_hash; };
struct ProductProvider : FramesProvider { virtual Ref<ProductGraph> graph() const=0;
  virtual Result<ArrayView> read_array(Context&,ArrayRef,BlockRequest) const=0; };
struct PublishRequest { std::string destination; DType signal_dtype;
  std::string profile; U32 tile_log2_width; bool durable; };
struct NormalizeFrame { Id frame; Ref<RawObservation> raw; Calibration calibration;
  Ref<WcsModel> supplied_wcs; Ref<Response> response; std::optional<RegroupRequest> regroup; };
struct ReferenceInformationRequest { Estimator reference; U32 control_order;
  std::vector<SkyICRS> positions; Basis background;
  Ref<NoiseModel> native_noise; SolvePolicy solve; };
struct NormalizeRequest { std::vector<NormalizeFrame> frames; SolverRequest astrometry;
  PsfRequest psf; PhotometryRequest photometry; DrizzleRequest drizzle;
  std::vector<ReferenceInformationRequest> information; bool allow_relative_uncalibrated;
  std::vector<PublishRequest> outputs; };
struct NormalizeResult { std::vector<ProductLocation> published;
  std::vector<std::pair<Id,Assessment>> frames; };
struct MosaicRequest { std::vector<ProductLocation> inputs; SkyRequest sky{};
  CoaddRequest coaddition; PublishRequest output; };
struct ExportWorkflowRequest { ProductLocation input; ExportRequest projection;
  PublishRequest output; };
// All functions synchronous. Context/spans borrowed through return; Ref outputs retain dependencies.
// Driver argument keys are exactly these parameter names; context is injected, request is envelope.request.
Result<Ref<WcsModel>> make_wcs(Context& context,const HeaderFields& header,Box domain,const AstrometricContext& astrometric_context,Id detector_frame,const NumericBudget& request);
Result<Ref<LinearAction>> make_action(Context& context,const ActionRecipe& recipe,std::span<const Ref<LinearAction>> children,std::span<const ArrayRef> arrays);
Result<Ref<CovarianceAction>> make_covariance(Context& context,Ref<NoiseModel> model);
Result<Ref<FramesProvider>> make_frames_provider(Context& context,std::span<const Ref<FrameDescription>> descriptions,std::span<const ArrayRef> signals,std::span<const ArrayRef> dq);
FitsPixel1 to_fits(PixelCenter0 point); FitsPixel1 to_fits(PixelEdge0 point);
PixelCenter0 from_fits_center(FitsPixel1 point); PixelEdge0 from_fits_edge(FitsPixel1 point);
Result<SkyICRS> forward(Context& context,Ref<WcsModel> wcs,PixelCenter0 point,const NumericBudget& request);
Result<SkyICRS> forward(Context& context,Ref<WcsModel> wcs,PixelEdge0 point,const NumericBudget& request);
Result<InverseResult> inverse(Context& context,Ref<WcsModel> wcs,SkyICRS sky,const NumericBudget& request);
Result<DerivativeResult> derivative(Context& context,Ref<WcsModel> wcs,PixelCenter0 point,const NumericBudget& request);
Result<Interval> solid_angle_jacobian(Context& context,Ref<WcsModel> wcs,Box domain,const NumericBudget& request);
Result<Ref<DetectorFootprint>> detector_footprint(Context& context,Ref<FootprintSet> footprints,U64 source_row);
Result<Ref<FootprintProvider>> make_footprint_provider(Context& context,Ref<WcsModel> wcs);
Result<Ref<NativeObservation>> bind_wcs(Context& context,Ref<NativeObservation> observation,Ref<WcsModel> wcs,Ref<AstrometricSolution> solution);
Result<Ref<NativeObservation>> bind_observation_model(Context& context,Ref<NativeObservation> observation,Ref<EffectivePSF> kernel,Ref<Response> response);
Result<Ref<CurvedBoundary>> footprint(Context& context,const FootprintProvider& provider,Ref<DetectorFootprint> source,double pixfrac,const GeometryBudget& request);
Result<SphericalAreaInterval> measure(Context& context,const FootprintProvider& provider,Ref<DetectorFootprint> source,const GeometryBudget& request);
Result<Ref<Overlaps>> compute_overlap(Context& context,const FootprintProvider& provider,Ref<DetectorFootprint> source,U32 order,double pixfrac,const GeometryBudget& request);
Result<Ref<NativeObservation>> calibrate_observation(Context& context,Ref<RawObservation> raw,const Calibration& request);
Result<Ref<NativeObservation>> regroup_observation(Context& context,Ref<NativeObservation> observation,const RegroupRequest& request);
Result<Ref<CatalogueIndex>> build_catalogue_index(Context& context,const CatalogueBuildRequest& request);
Result<Ref<CatalogueProvider>> make_catalogue_provider(Context& context,Ref<CatalogueIndex> index);
Result<Ref<AstrometricSolution>> solve_astrometry(Context& context,Ref<NativeObservation> observation,const CatalogueProvider& catalogue,const SolverRequest& request);
Result<Ref<EffectivePSF>> fit_psf(Context& context,Ref<NativeObservation> observation,Ref<WcsModel> wcs,std::span<const CatalogueStar> stars,const PsfRequest& request);
Result<Ref<PsfEvaluation>> evaluate_psf(Context& context,Ref<EffectivePSF> psf,const PsfEvaluationRequest& request);
Result<Ref<SourceMeasurements>> measure_science_sources(Context& context,Ref<NativeObservation> observation,Ref<WcsModel> wcs,std::span<const CatalogueStar> stars,Ref<EffectivePSF> psf,const PsfRequest& request);
Result<Ref<SpectralPrediction>> integrate_response(Context& context,Ref<Response> response,Ref<SpectralBasis> basis,const NumericBudget& request);
Result<Ref<PhotoSolution>> fit_photometry(Context& context,std::span<const Ref<SourceMeasurements>> measurements,std::span<const CatalogueStar> stars,std::span<const Ref<SpectralPrediction>> predictions,const PhotometryRequest& request);
Result<Ref<NativeObservation>> apply_scale(Context& context,Ref<PhotoSolution> solution,Ref<NativeObservation> observation,Id frame);
Result<DrizzleResult> build_drizzle(Context& context,Ref<Overlaps> overlaps,Ref<NativeObservation> observation,const DrizzleRequest& request);
Result<Ref<GeometryRevision>> refine_geometry(Context& context,const RefinableGeometryProvider& provider,Ref<GeometrySnapshot> base,const GeometryBudget& request);
Result<ArrayRef> apply(Context& context,Ref<LinearAction> action,ArrayRef values);
Result<ArrayRef> transpose_apply(Context& context,Ref<LinearAction> action,ArrayRef values);
Result<Ref<LinearAction>> compose(Context& context,Ref<LinearAction> outer,Ref<LinearAction> inner);
Result<ArrayRef> covariance_apply(Context& context,Ref<CovarianceAction> covariance,ArrayRef values);
Result<Variance> quadratic_form(Context& context,Ref<CovarianceAction> covariance,ArrayRef functional);
Result<BoundOrUnavailable> geometry_covariance_bound(Context& context,Ref<CovarianceAction> covariance,Ref<LinearAction> nominal,Ref<LinearAction> absolute_error,ArrayRef functional);
Result<Information> estimate_flux_information(Context& context,const InformationRequest& request);
Result<std::vector<Variance>> evaluate_scene_variance(Context& context,const FramesProvider& frames,Id frame,std::span<const HealpixLeaf> leaves);
Result<Ref<CovarianceAction>> control_covariance(Context& context,Ref<CovarianceAction> covariance,Ref<LinearAction> T);
Result<VariancePrediction> reconstruct_scene_variance(Context& context,Ref<PrecisionControls> controls,Ref<SupportedTriangulation> triangulation,std::span<const SkyICRS> queries);
Result<Ref<SupportedTriangulation>> build_supported_triangulation(Context& context,Ref<PrecisionControls> controls,Chart chart,Ref<SupportMap> support);
Result<Ref<SkyControls>> build_sky_controls(Context& context,const FramesProvider& frames,const SkyRequest& request);
Result<Ref<SkyFit>> fit_relative_sky(Context& context,Ref<SkyControls> controls,const SkyRequest& request);
Result<Ref<SignalEstimate>> apply_sky(Context& context,Ref<SignalEstimate> signal,Ref<SkyFit> fit);
Result<Ref<Selection>> select_samples(Context& context,const FramesProvider& frames,const CoaddRequest& request);
Result<Ref<WeightSolution>> solve_weights(Context& context,const FramesProvider& frames,Ref<Selection> selection,const CoaddRequest& request);
Result<CoaddResult> coadd(Context& context,const FramesProvider& frames,Ref<SkyFit> sky,const CoaddRequest& request);
Result<ExportResult> build_export(Context& context,const ProductProvider& product,const ExportRequest& request);
Result<ProductLocation> export_signal(Context& context,const ExportResult& image,const PublishRequest& request);
Result<Ref<ProductProvider>> open_product(Context& context,const ProductLocation& location);
Result<ProductLocation> publish_product(Context& context,Ref<ProductGraph> graph,const PublishRequest& request);
Result<NormalizeResult> normalize(Context& context,const CatalogueProvider& catalogue,const NormalizeRequest& request);
Result<ProductLocation> mosaic(Context& context,const MosaicRequest& request);
Result<ProductLocation> export_image(Context& context,const ExportWorkflowRequest& request);
} // namespace acsd
