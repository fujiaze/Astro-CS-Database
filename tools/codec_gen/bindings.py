"""Operation bindings, process metadata and derived typed inputs.

Two kinds of information live here:

  * Mechanical derivation from the frozen declarations (include/acsd/model.hpp):
    the parameter list of every bound public function / provider method becomes
    the field list of the typed Input struct, using the FOUNDATION_API rules
    (Context removed, span -> Ref<vector<T>>, const Provider& -> Ref<Provider>,
    Ref kept, request kept as the science Request type).
  * Build metadata that is not derivable from a C++ signature: which declaration
    implements each whitelisted operation_id, the process bounds table and the
    run-integration status.  Each of those carries its source document; there is
    no second registry and no algorithm here.

The whitelist itself (22 processes / 57 operations) is parsed from
docs/engineering/rebuild/processes/README.md; the process documents supply the
probe ids; SYSTEM.md supplies the static module dependencies.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re

from .model_ast import DeclarationModel, Function, Method, Record, Type


ROOT = Path(__file__).resolve().parents[2]
PROCESS_README = ROOT / "docs/engineering/rebuild/processes/README.md"
PROCESS_DIR = ROOT / "docs/engineering/rebuild/processes"
SYSTEM_DOC = ROOT / "docs/engineering/rebuild/SYSTEM.md"


# ---------------------------------------------------------------------------
# Typed input derivation
# ---------------------------------------------------------------------------

@dataclass
class InputField:
    name: str
    type: Type
    cpp_type: str                 # spelling used in the generated struct


@dataclass
class OpBinding:
    process: str
    operation: str
    struct: str
    kind: str                     # free | method | coadd
    declaration: str              # free function or method name
    interface: str = ""           # provider/action class for kind == method
    field_name: str = ""          # provider/action field for kind == method
    discriminator: str = ""       # substring selecting one free overload
    param_names: dict[int, str] = field(default_factory=dict)
    extra_fields: list[tuple[str, str]] = field(default_factory=list)
    field_order: list[str] = field(default_factory=list)
    existing_struct: str = ""     # struct already declared in a module header
    run_status: str = "pending_m5"
    run_reason: str = ""
    plan_stage: str = "M5"


FREE = "free"
METHOD = "method"
COADD = "coadd"


# Every whitelisted operation_id maps to exactly one public declaration.  The
# operation ids come from docs/engineering/rebuild/processes/README.md; the
# declarations from include/acsd/model.hpp.  The generator verifies each entry
# against the AST, so a renamed or removed declaration fails generation.
OPERATIONS: list[OpBinding] = [
    # observation
    OpBinding("observation.calibrate", "calibrate_observation",
              "CalibrateInput", FREE, "calibrate_observation",
              existing_struct="include/acsd/modules/observation.hpp"),
    OpBinding("observation.regroup", "regroup_observation",
              "RegroupObservationInput", FREE, "regroup_observation"),
    # coordinates
    OpBinding("coordinates.wcs", "make_wcs", "MakeWcsInput", FREE, "make_wcs"),
    OpBinding("coordinates.wcs", "forward.center0", "ForwardCenter0Input", FREE,
              "forward", discriminator="PixelCenter0"),
    OpBinding("coordinates.wcs", "forward.edge0", "ForwardEdge0Input", FREE,
              "forward", discriminator="PixelEdge0"),
    OpBinding("coordinates.wcs", "inverse", "InverseInput", FREE, "inverse"),
    OpBinding("coordinates.wcs", "derivative", "DerivativeInput", FREE,
              "derivative"),
    OpBinding("coordinates.wcs", "solid_angle_jacobian",
              "SolidAngleJacobianInput", FREE, "solid_angle_jacobian"),
    OpBinding("coordinates.wcs", "bind_wcs", "BindWcsInput", FREE, "bind_wcs"),
    # catalogue
    OpBinding("catalogue.access", "build_catalogue_index",
              "BuildCatalogueIndexInput", FREE, "build_catalogue_index"),
    OpBinding("catalogue.access", "make_catalogue_provider",
              "MakeCatalogueProviderInput", FREE, "make_catalogue_provider"),
    OpBinding("catalogue.access", "catalogue.query", "CatalogueQueryInput",
              METHOD, "query", interface="CatalogueProvider",
              field_name="provider"),
    OpBinding("catalogue.access", "catalogue.query_quads",
              "CatalogueQueryQuadsInput", METHOD, "query_quads",
              interface="CatalogueProvider", field_name="provider"),
    OpBinding("catalogue.access", "catalogue.lookup", "CatalogueLookupInput",
              METHOD, "lookup", interface="CatalogueProvider",
              field_name="provider"),
    # sources
    OpBinding("sources.astrometry", "solve_astrometry", "SolveAstrometryInput",
              FREE, "solve_astrometry"),
    OpBinding("sources.psf", "fit_psf", "FitPsfInput", FREE, "fit_psf"),
    OpBinding("sources.psf", "evaluate_psf", "EvaluatePsfInput", FREE,
              "evaluate_psf"),
    OpBinding("sources.psf", "bind_observation_model",
              "BindObservationModelInput", FREE, "bind_observation_model"),
    OpBinding("sources.measure", "measure_science_sources",
              "MeasureScienceSourcesInput", FREE, "measure_science_sources"),
    # response
    OpBinding("response.integrate", "integrate_response",
              "IntegrateResponseInput", FREE, "integrate_response"),
    # photometry
    OpBinding("photometry.fit", "fit_photometry", "FitPhotometryInput", FREE,
              "fit_photometry"),
    OpBinding("photometry.apply", "apply_scale", "ApplyScaleInput", FREE,
              "apply_scale"),
    # geometry.overlap
    OpBinding("geometry.overlap", "make_footprint_provider",
              "MakeFootprintProviderInput", FREE, "make_footprint_provider"),
    OpBinding("geometry.overlap", "detector_footprint",
              "DetectorFootprintInput", FREE, "detector_footprint"),
    OpBinding("geometry.overlap", "footprint", "FootprintInput", FREE,
              "footprint"),
    OpBinding("geometry.overlap", "measure", "MeasureInput", FREE, "measure"),
    OpBinding("geometry.overlap", "compute_overlap", "ComputeOverlapInput",
              FREE, "compute_overlap"),
    OpBinding("geometry.overlap", "refine_geometry", "RefineGeometryInput",
              FREE, "refine_geometry"),
    OpBinding("geometry.overlap", "footprint.forward_interval",
              "FootprintForwardIntervalInput", METHOD, "forward_interval",
              interface="FootprintProvider", field_name="provider",
              param_names={0: "box"}),
    OpBinding("geometry.overlap", "footprint.boundary_interval",
              "FootprintBoundaryIntervalInput", METHOD, "boundary_interval",
              interface="FootprintProvider", field_name="provider",
              param_names={0: "edge", 1: "parameter"}),
    OpBinding("geometry.overlap", "footprint.derivative_interval",
              "FootprintDerivativeIntervalInput", METHOD, "derivative_interval",
              interface="FootprintProvider", field_name="provider",
              param_names={0: "box"}),
    OpBinding("geometry.overlap", "refinable.open", "RefinableOpenInput",
              METHOD, "open", interface="RefinableGeometryProvider",
              field_name="provider"),
    OpBinding("geometry.overlap", "refinable.refine", "RefinableRefineInput",
              METHOD, "refine", interface="RefinableGeometryProvider",
              field_name="provider", param_names={0: "base", 1: "request"}),
    # operators.action
    OpBinding("operators.action", "make_action", "MakeActionInput", FREE,
              "make_action"),
    OpBinding("operators.action", "apply", "ApplyInput", METHOD, "apply",
              interface="LinearAction", field_name="action",
              param_names={0: "values"}),
    OpBinding("operators.action", "transpose_apply", "TransposeApplyInput",
              METHOD, "transpose_apply", interface="LinearAction",
              field_name="action", param_names={0: "values"}),
    OpBinding("operators.action", "compose", "ComposeInput", FREE, "compose"),
    OpBinding("operators.action", "action.dependency_blocks",
              "ActionDependencyBlocksInput", METHOD, "dependency_blocks",
              interface="LinearAction", field_name="action",
              param_names={0: "range"}),
    OpBinding("operators.action", "action.serialize_recipe",
              "ActionSerializeRecipeInput", METHOD, "serialize_recipe",
              interface="LinearAction", field_name="action"),
    OpBinding("operators.action", "action.row_enclosure",
              "ActionRowEnclosureInput", METHOD, "row_enclosure",
              interface="LinearAction", field_name="action"),
    # uncertainty.covariance
    OpBinding("uncertainty.covariance", "make_covariance", "MakeCovarianceInput",
              FREE, "make_covariance"),
    OpBinding("uncertainty.covariance", "covariance_apply",
              "CovarianceApplyInput", FREE, "covariance_apply"),
    OpBinding("uncertainty.covariance", "quadratic_form", "QuadraticFormInput",
              FREE, "quadratic_form"),
    OpBinding("uncertainty.covariance", "geometry_covariance_bound",
              "GeometryCovarianceBoundInput", FREE, "geometry_covariance_bound"),
    OpBinding("uncertainty.covariance", "control_covariance",
              "ControlCovarianceInput", FREE, "control_covariance"),
    # geometry.drizzle
    OpBinding("geometry.drizzle", "build_drizzle", "BuildDrizzleInput", FREE,
              "build_drizzle"),
    # information
    OpBinding("information.reference", "estimate_flux_information",
              "EstimateFluxInformationInput", FREE,
              "estimate_flux_information"),
    OpBinding("information.scene", "evaluate_scene_variance",
              "EvaluateSceneVarianceInput", FREE, "evaluate_scene_variance"),
    OpBinding("information.scene", "build_supported_triangulation",
              "BuildSupportedTriangulationInput", FREE,
              "build_supported_triangulation"),
    OpBinding("information.scene", "reconstruct_scene_variance",
              "ReconstructSceneVarianceInput", FREE,
              "reconstruct_scene_variance"),
    # sky
    OpBinding("sky.controls", "build_sky_controls", "BuildSkyControlsInput",
              FREE, "build_sky_controls"),
    OpBinding("sky.fit", "fit_relative_sky", "FitRelativeSkyInput", FREE,
              "fit_relative_sky"),
    OpBinding("sky.fit", "apply_sky", "ApplySkyInput", FREE, "apply_sky"),
    # coadd
    OpBinding("coadd.select", "select_samples", "SelectSamplesInput", FREE,
              "select_samples"),
    OpBinding("coadd.weights", "solve_weights", "SolveWeightsInput", FREE,
              "solve_weights"),
    OpBinding("coadd.integrate", "coadd", "CoaddInput", COADD, "coadd",
              extra_fields=[("selection", "Ref<Selection>"),
                            ("weights", "Ref<WeightSolution>")],
              field_order=["frames", "sky", "selection", "weights", "request"],
              run_status="blocked_refactor",
              run_reason="coadd() recomputes Selection and WeightSolution "
                         "internally; CoaddInput must consume the explicit "
                         "selection/weights (one implementation, no silent "
                         "recompute) before the operation can run"),
    # projection
    OpBinding("projection.export", "build_export", "BuildExportInput", FREE,
              "build_export"),
]


# Probe kind is owned once by standards/PROBES.md, not assumed per process.
PROBES_DOC = (Path(__file__).resolve().parents[2]
              / "docs/engineering/rebuild/standards/PROBES.md")


def _parse_probe_kinds(document: Path = PROBES_DOC) -> dict[str, str]:
    kinds: dict[str, str] = {}
    pattern = re.compile(r"^\|\s*`([^`]+)`\s*/\s*([a-z_]+)\s*\|")
    for line in document.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line.strip())
        if match:
            kinds[match.group(1)] = match.group(2)
    if not kinds:
        raise SystemExit(f"{document}: no probe catalog rows")
    return kinds


# PlanBounds per process.  min_block_elements comes from the DEVELOPMENT.md
# "模块plan和工作单元表" minimum unit (all processes have a minimum of one
# element/point/Box/leaf).  max_block_elements is the documented upper bound
# where a process document names one; otherwise the explicit provisional
# fallback 65536.  allow_spill stays false until a module supplies its spill
# route in plan (conservative: no spill is always safe).
_BOUNDS_DEFAULT = (1, 65536, "provisional: no block-size upper bound is "
                              "declared by the process document or "
                              "DEVELOPMENT.md; conservative fallback")
PROCESS_BOUNDS = {
    "catalogue.access": (1, 4096,
                         "docs/engineering/rebuild/processes/"
                         "catalogue.access.md: page_size is bounded to 1..4096"),
    "geometry.overlap": (1, 4096,
                         "docs/engineering/rebuild/processes/"
                         "geometry.overlap.md: at most 4096 candidate "
                         "cells/nodes before subdivision"),
}


@dataclass
class ProcessMeta:
    process_id: str
    module: str
    document: str
    operations: list[str]
    dependencies: list[str]
    probes: list[tuple[str, str]]


def _parse_whitelist(readme: Path) -> tuple[list[str], dict[str, tuple[str, list[str]]]]:
    order: list[str] = []
    table: dict[str, tuple[str, list[str]]] = {}
    for line in readme.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|") or "](" not in line:
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 3:
            raise SystemExit(f"{readme}: malformed whitelist row: {line}")
        match = re.match(r"^\[([^\]]+)\]\(([^)]+)\)$", cells[0])
        if not match:
            raise SystemExit(f"{readme}: row does not link a process document")
        process_id, link = match.group(1), match.group(2)
        operations = [item.strip() for item in cells[2].split(",")
                      if item.strip()]
        order.append(process_id)
        table[process_id] = (cells[1], operations)
    return order, table


def _parse_probes(document: Path) -> list[str]:
    text = document.read_text(encoding="utf-8")
    match = re.search(r"本process只登记：([^。]+)。", text)
    if not match:
        raise SystemExit(f"{document}: no '本process只登记' probe declaration")
    probes = re.findall(r"`([^`]+)`", match.group(1))
    if not probes:
        raise SystemExit(f"{document}: probe declaration lists no id")
    return probes


def _parse_module_dependencies(system: Path) -> dict[str, list[str]]:
    legal = {"model", "numerics", "coordinates", "observation", "catalogue",
             "sources", "response", "photometry", "geometry", "operators",
             "uncertainty", "information", "sky", "coadd", "projection",
             "storage", "resources", "runtime", "workflows"}
    dependencies: dict[str, list[str]] = {}
    pattern = re.compile(r"^\|\s*acsd_([a-z_]+)\s*/\s*src/([a-z_]+)\s*\|\s*([^|]+)\|")
    for line in system.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line.strip())
        if not match:
            continue
        target, module, cell = match.group(1), match.group(2), match.group(3)
        if target != module:
            continue
        direct = cell.split("；")[0]
        names = [name.strip() for name in re.split(r"[、,，]", direct)]
        dependencies[module] = [name for name in names if name in legal]
    return dependencies


def load_process_metadata() -> tuple[list[ProcessMeta], dict[str, list[str]]]:
    order, table = _parse_whitelist(PROCESS_README)
    module_dependencies = _parse_module_dependencies(SYSTEM_DOC)
    metadata: list[ProcessMeta] = []
    for process_id in order:
        module, operations = table[process_id]
        document = f"docs/engineering/rebuild/processes/{process_id}.md"
        document_path = PROCESS_DIR / f"{process_id}.md"
        if not document_path.is_file():
            raise SystemExit(f"process document missing: {document_path}")
        probe_kinds = _parse_probe_kinds()
        probes = []
        for probe in _parse_probes(document_path):
            if probe not in probe_kinds:
                raise SystemExit(
                    f"{document}: probe '{probe}' is not registered in "
                    f"standards/PROBES.md")
            probes.append((probe, probe_kinds[probe]))
        if module not in module_dependencies:
            raise SystemExit(f"SYSTEM.md has no dependency row for module "
                             f"'{module}' (process {process_id})")
        metadata.append(ProcessMeta(
            process_id=process_id, module=module, document=document,
            operations=operations,
            dependencies=list(module_dependencies[module]),
            probes=probes))
    return metadata, module_dependencies


# ---------------------------------------------------------------------------
# Derivation of one Input struct from its declaration
# ---------------------------------------------------------------------------

def _declaration_of(binding: OpBinding, model: DeclarationModel):
    if binding.kind in (FREE, COADD):
        candidates = model.functions.get(binding.declaration, [])
        if binding.discriminator:
            candidates = [candidate for candidate in candidates
                          if any(binding.discriminator in param[1]
                                 for param in candidate.params)]
        if len(candidates) != 1:
            raise SystemExit(
                f"{binding.process}/{binding.operation}: expected exactly one "
                f"declaration '{binding.declaration}'"
                + (f" with '{binding.discriminator}'" if binding.discriminator
                   else "")
                + f", found {len(candidates)}")
        function = candidates[0]
        return [(name, type) for name, type in function.params], \
            function.return_type
    record = model.record(binding.interface)
    candidates = [method for method in record.methods
                  if method.name == binding.declaration and method.is_virtual]
    if len(candidates) != 1:
        raise SystemExit(
            f"{binding.process}/{binding.operation}: expected exactly one "
            f"virtual method {binding.interface}::{binding.declaration}, "
            f"found {len(candidates)}")
    method = candidates[0]
    return [(name, type) for name, type in method.params], method.return_type


def _result_value_type(return_type: str) -> str:
    match = re.match(r"^Result<(.+)>\s*\(", return_type)
    if not match:
        raise SystemExit(f"declaration does not return Result<T>: {return_type}")
    return match.group(1).strip()


def _is_context(spelling: str) -> bool:
    return " ".join(spelling.split()) == "Context &"


def derive_input(binding: OpBinding, model: DeclarationModel) -> tuple[list[InputField], Type]:
    """Returns (fields, output type) for one operation binding."""
    params, return_type = _declaration_of(binding, model)
    params = [(name, type) for name, type in params if not _is_context(type)]
    fields: list[InputField] = []
    if binding.kind == METHOD:
        fields.append(InputField(binding.field_name,
                                 Type("ref", f"Ref<{binding.interface}>",
                                      (Type("named", binding.interface),)),
                                 f"Ref<{binding.interface}>"))
    for index, (name, spelling) in enumerate(params):
        field_name = binding.param_names.get(index, name)
        if not field_name:
            raise SystemExit(
                f"{binding.process}/{binding.operation}: parameter {index} "
                f"('{spelling}') has no name and no declared override")
        fields.append(_derive_field(field_name, spelling, model, binding))
    if binding.extra_fields:
        for name, cpp_type in binding.extra_fields:
            fields.append(InputField(name, model.parse(cpp_type, None),
                                     cpp_type))
    if binding.field_order:
        known = {item.name: item for item in fields}
        if set(known) != set(binding.field_order):
            raise SystemExit(
                f"{binding.operation}: field_order does not match the derived "
                f"fields {sorted(known)}")
        fields = [known[name] for name in binding.field_order]
    output = model.parse(_result_value_type(return_type), None)
    if output.kind == "unsupported":
        raise SystemExit(
            f"{binding.operation}: output type '{output.name}' is not wireable: "
            f"{output.reason}")
    return fields, output


def _derive_field(name: str, spelling: str, model: DeclarationModel,
                  binding: OpBinding) -> InputField:
    text = " ".join(spelling.split())
    is_reference = text.endswith("&")
    if text.startswith("std::span<"):
        element = model.parse(text, None)
        if element.kind != "span":
            raise SystemExit(f"{binding.operation}: cannot parse span '{text}'")
        inner = element.args[0]
        cpp = f"Ref<std::vector<{inner.describe()}>>"
        return InputField(name, Type("ref", cpp,
                                     (Type("vector", f"std::vector<{inner.describe()}>",
                                               (inner,)),)), cpp)
    if is_reference:
        base = text[:-1].strip()
        if "const " not in text.split("&")[0] and not text.startswith("const "):
            raise SystemExit(
                f"{binding.operation}: non-const reference parameter "
                f"'{text}' cannot become an immutable Input field")
        underlying = base
        while underlying.startswith("const "):
            underlying = underlying[len("const "):].strip()
        if underlying in ("FramesProvider", "CatalogueProvider",
                          "FootprintProvider", "RefinableGeometryProvider",
                          "ProductProvider", "LinearAction"):
            cpp = f"Ref<{underlying}>"
            return InputField(name, Type("ref", cpp,
                                         (Type("named", underlying),)), cpp)
        parsed = model.parse(underlying, None)
        if parsed.kind == "unsupported":
            raise SystemExit(
                f"{binding.operation}: parameter '{name}' of type '{text}' is "
                f"not wireable: {parsed.reason}")
        return InputField(name, parsed, underlying)
    parsed = model.parse(text, None)
    if parsed.kind == "span":
        inner = parsed.args[0]
        cpp = f"Ref<std::vector<{inner.describe()}>>"
        return InputField(name, Type("ref", cpp,
                                     (Type("vector", f"std::vector<{inner.describe()}>",
                                               (inner,)),)), cpp)
    if parsed.kind == "unsupported":
        raise SystemExit(
            f"{binding.operation}: parameter '{name}' of type '{text}' is not "
            f"wireable: {parsed.reason}")
    return InputField(name, parsed, text)


def type_id_schema(type_: Type) -> str:
    """Stable wire schema id for an operation input/output type."""
    if type_.kind == "ref":
        target = type_.args[0]
        if target.kind == "named":
            return "acsd." + target.name.replace("::", ".")
        if target.kind == "vector":
            return "acsd.vector." + _element_name(target.args[0])
        raise SystemExit(f"cannot derive a TypeId for Ref<{target.describe()}>")
    if type_.kind == "named":
        return "acsd." + type_.name.replace("::", ".")
    if type_.kind == "vector":
        return "acsd.vector." + _element_name(type_.args[0])
    if type_.kind == "enum":
        return "acsd.enum." + type_.name.replace("::", ".")
    raise SystemExit(f"cannot derive a TypeId for {type_.describe()}")


def _element_name(type_: Type) -> str:
    if type_.kind == "named":
        return type_.name.replace("::", ".")
    if type_.kind == "ref":
        return _element_name(type_.args[0])
    return type_.describe().replace("::", ".")
