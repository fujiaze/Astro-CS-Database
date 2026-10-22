// Validation cases of the response and photometry modules.
// Included by phot_validation.cpp inside the anonymous namespace; it uses only
// the public declaration contract and the helpers defined there.

// ---------------------------------------------------------------------------
// Synthetic case builder
// ---------------------------------------------------------------------------

struct CaseSpec {
  bool plane_basis = false;       // constant (1 term) or plane (3 terms)
  std::size_t frames = 2;
  std::size_t stars = 6;
  bool collinear = false;         // the training positions lie on v = u
  bool unknown_variance = false;  // the measurement variance is unknown
  bool missing_enclosure = false; // the numerical flux enclosure is absent
  double enclosure_half_width = 0.0;
  double gaia_perturbation = 0.0; // the perturbation of the measured XP coefficients
  std::size_t latent_dim = 2;
  double response_center = 550.0;
  double response_width = 20.0;
  double log_a = 0.1;
  double spatial_first = 0.03;
  double spatial_second = -0.02;
  double target_counts = 0.0;  // the measured counts of the first star (0: the default)
  double count_scale = 1e-16;  // native ADU per unit photon rate
  double relative_measurement_error = 0.01;
};

struct BuiltCase {
  Ref<SpectralBasis> basis;
  std::vector<Ref<Response>> responses;
  std::vector<Ref<SpectralPrediction>> predictions;
  std::vector<Ref<SourceMeasurements>> measurements;
  std::vector<CatalogueStar> stars;
  PhotometryRequest request;
  std::vector<std::vector<double>> gaia_truth;
  std::vector<std::vector<double>> integrals;  // the analytic basis integrals per frame
  std::vector<double> position_x;
  std::vector<double> position_y;
  std::vector<double> basis_mean;
  double log_a_truth = 0.0;
};

// The analytic integrals of the two basis functions over the top hat band.
std::vector<double> analytic_integrals(double center, double width, bool photon) {
  const double low = center - 0.5 * width;
  const double high = center + 0.5 * width;
  std::vector<double> values(2, 0.0);
  if (photon) {
    values[0] = kPhotonFactor * (high * high - low * low) * 0.5;
    // int (lambda - 500) lambda / 100 dlambda over the band
    const auto primitive = [](double lambda) {
      return lambda * lambda * lambda / 300.0 - 2.5 * lambda * lambda;
    };
    values[1] = kPhotonFactor * (primitive(high) - primitive(low));
  } else {
    values[0] = width;
    const auto primitive = [](double lambda) { return (lambda - 500.0) * (lambda - 500.0) / 200.0; };
    values[1] = primitive(high) - primitive(low);
  }
  return values;
}

Result<BuiltCase> build_case(Fixture& fixture, const CaseSpec& spec) {
  BuiltCase built;

  // ---------------------------------------------------------------------
  // The XP basis: b_0 = 1 and b_1 = (lambda - 500) / 100 on [500, 600].
  // ---------------------------------------------------------------------
  auto basis = std::make_shared<SpectralBasis>();
  basis->identity.schema_id = "acsd.SpectralBasis";
  basis->identity.schema_major = 2;
  basis->identity.schema_minor = 0;
  basis->identity.revision = 1;
  basis->identity.object_id = "basis:case:" + std::to_string(spec.frames) + ":" +
                              std::to_string(spec.stars) + ":" + std::to_string(spec.plane_basis);
  basis->identity.content_hash = fake_hash(basis->identity.object_id);
  basis->identity.run_id = "validation";
  basis->degree = 1;
  basis->coefficient_count = 2;
  basis->support_nm = Interval{500.0, 600.0};
  Result<ArrayRef> knots = make_f64(fixture, {500.0, 600.0}, {2}, Unit{"nm", 0, 0, 1, 1.0, 0},
                                    basis->identity.object_id);
  if (!good(knots)) {
    Result<BuiltCase> out;
    out.code = knots.code;
    return out;
  }
  // [b_0 = 1, b_1 = t] with ascending powers in the local variable.
  Result<ArrayRef> coefficients =
      make_f64(fixture, {1.0, 0.0, 0.0, 1.0}, {2, 1, 2}, Unit{"1", 0, 0, 0, 1.0, 0},
               basis->identity.object_id);
  if (!good(coefficients)) {
    Result<BuiltCase> out;
    out.code = coefficients.code;
    return out;
  }
  basis->knots_nm = get(knots);
  basis->basis_coefficients = get(coefficients);
  built.basis = basis;

  // ---------------------------------------------------------------------
  // The responses and their spectral predictions.
  // ---------------------------------------------------------------------
  for (std::size_t k = 0; k < spec.frames; ++k) {
    auto response = std::make_shared<Response>();
    response->identity.schema_id = "acsd.Response";
    response->identity.schema_major = 2;
    response->identity.schema_minor = 0;
    response->identity.revision = 1;
    response->identity.object_id = "response:case:" + std::to_string(k);
    response->identity.content_hash = fake_hash(response->identity.object_id);
    response->identity.run_id = "validation";
    response->convention = Response::Convention::photon_throughput;
    response->top_hat_center_width_nm =
        Vec2{spec.response_center + 10.0 * static_cast<double>(k), spec.response_width};
    response->support_nm = Interval{spec.response_center + 10.0 * static_cast<double>(k) -
                                        0.5 * spec.response_width,
                                    spec.response_center + 10.0 * static_cast<double>(k) +
                                        0.5 * spec.response_width};
    response->provenance = "synthetic top hat";
    response->normalization = "unit peak";
    built.responses.push_back(response);

    NumericBudget budget;
    budget.tolerance.absolute = 0.0;
    budget.tolerance.relative = 1e-13;
    budget.max_iterations = 200;
    Result<Ref<SpectralPrediction>> prediction =
        integrate_response(fixture.context, response, basis, budget);
    if (!good(prediction)) {
      Result<BuiltCase> out;
      out.code = prediction.code;
      return out;
    }
    built.predictions.push_back(get(prediction));
    built.integrals.push_back(
        analytic_integrals(spec.response_center + 10.0 * static_cast<double>(k), spec.response_width,
                           true));
  }

  // ---------------------------------------------------------------------
  // The training positions and the spatial basis.
  // ---------------------------------------------------------------------
  // The training positions must not be collinear in the (u, v) plane, because
  // a collinear training set makes the two spatial terms of the plane basis
  // linearly dependent (that is the rank negative case, built separately).
  for (std::size_t s = 0; s < spec.stars; ++s) {
    if (spec.collinear) {
      built.position_x.push_back(100.0 + 12.0 * static_cast<double>(s));
      built.position_y.push_back(100.0 + 12.0 * static_cast<double>(s));
    } else {
      const std::size_t column = s % 3;
      const std::size_t row = s / 3;
      built.position_x.push_back(100.0 + 12.0 * static_cast<double>(column));
      built.position_y.push_back(100.0 + 9.0 * static_cast<double>(row));
    }
  }
  Basis spatial;
  spatial.kind = spec.plane_basis ? Basis::Kind::plane : Basis::Kind::constant;
  spatial.degree = spec.plane_basis ? 1 : 0;
  double center_x = 0.0;
  double center_y = 0.0;
  double span_x = 0.0;
  double span_y = 0.0;
  for (std::size_t s = 0; s < spec.stars; ++s) {
    center_x += built.position_x[s];
    center_y += built.position_y[s];
  }
  center_x /= static_cast<double>(spec.stars);
  center_y /= static_cast<double>(spec.stars);
  for (std::size_t s = 0; s < spec.stars; ++s) {
    span_x = std::max(span_x, std::fabs(built.position_x[s] - center_x));
    span_y = std::max(span_y, std::fabs(built.position_y[s] - center_y));
  }
  spatial.center = Vec2{center_x, center_y};
  spatial.scale = Vec2{std::max(1.0, 2.0 * span_x), std::max(1.0, 2.0 * span_y)};
  spatial.normalization_domain = Box{{0.0, 0.0}, {400.0, 400.0}};

  // The weighted mean of the spatial terms over the training set.
  const std::size_t spatial_terms = spec.plane_basis ? 3 : 1;
  std::vector<double> mean(spatial_terms, 0.0);
  if (spec.plane_basis) {
    double sum_u = 0.0;
    double sum_v = 0.0;
    for (std::size_t s = 0; s < spec.stars; ++s) {
      const double u = (built.position_x[s] - spatial.center[0]) / spatial.scale[0];
      const double v = (built.position_y[s] - spatial.center[1]) / spatial.scale[1];
      sum_u += u;
      sum_v += v;
    }
    mean[1] = sum_u / static_cast<double>(spec.stars);
    mean[2] = sum_v / static_cast<double>(spec.stars);
  }
  built.basis_mean = mean;
  Result<ArrayRef> mean_array =
      make_f64(fixture, mean, {static_cast<U64>(mean.size())}, Unit{"1", 0, 0, 0, 1.0, 0}, "basis:mean");
  if (!good(mean_array)) {
    Result<BuiltCase> out;
    out.code = mean_array.code;
    return out;
  }
  spatial.weighted_mean = get(mean_array);

  // ---------------------------------------------------------------------
  // The catalogue stars and their XP coefficients.
  // ---------------------------------------------------------------------
  Index xp_index = make_index(spec.latent_dim, Index::Kind::latent, "xp");
  // The XP coefficients are known far better than the counts: the latent
  // spectral block is pinned by the catalogue measurement.
  std::vector<double> xp_variance(spec.latent_dim, 1e-10);
  Result<Ref<CovarianceAction>> xp_noise = make_diagonal_covariance(fixture, xp_variance, xp_index);
  if (!good(xp_noise)) {
    Result<BuiltCase> out;
    out.code = xp_noise.code;
    return out;
  }
  Result<Ref<CovarianceAction>> astrometry_noise =
      make_diagonal_covariance(fixture, {1e-8, 1e-8}, xp_index);
  for (std::size_t s = 0; s < spec.stars; ++s) {
    CatalogueStar star;
    star.source_id = 1000 + s;
    star.ra_rad = 0.1;
    star.dec_rad = 0.2;
    star.epoch_year = 2016.0;
    star.brightness_mag = 12.0;
    star.xp_basis = basis;
    star.xp_noise = get(xp_noise);
    star.astrometric_noise = get(astrometry_noise);
    std::vector<double> latent(spec.latent_dim, 0.0);
    for (std::size_t d = 0; d < spec.latent_dim; ++d) {
      latent[d] = 1.0 + 0.05 * static_cast<double>(s) + 0.1 * static_cast<double>(d);
    }
    if (spec.target_counts > 0.0 && s == 0) {
      // The latent is chosen so that the measured counts of the first star
      // are exactly the requested value; with a logarithmic scale of zero the
      // measured counts are then exactly that value.
      const std::vector<double>& v0 = built.integrals[0];
      latent.assign(spec.latent_dim, 0.0);
      latent[0] = spec.target_counts / (spec.count_scale * v0[0]);
    }
    built.gaia_truth.push_back(latent);
    std::vector<double> measured = latent;
    if (spec.gaia_perturbation > 0.0) {
      // A deterministic, reproducible perturbation of the measured XP
      // coefficients (the residual of the shared latent).
      const double phase = 0.37 * static_cast<double>(s + 1);
      for (std::size_t d = 0; d < spec.latent_dim; ++d) {
        measured[d] += spec.gaia_perturbation * std::sin(phase + static_cast<double>(d));
      }
    }
    Result<ArrayRef> coefficients =
        make_f64(fixture, measured, {static_cast<U64>(measured.size())}, Unit{"1", 0, 0, 0, 1.0, 0},
                 "xp:" + std::to_string(star.source_id));
    if (!good(coefficients)) {
      Result<BuiltCase> out;
      out.code = coefficients.code;
      return out;
    }
    star.xp_coefficients = get(coefficients);
    built.stars.push_back(star);
  }

  // ---------------------------------------------------------------------
  // The measurements: the counts follow the model of the fit.
  // ---------------------------------------------------------------------
  built.log_a_truth = spec.log_a;
  for (std::size_t k = 0; k < spec.frames; ++k) {
    const std::vector<double>& v = built.integrals[k];
    auto measurement = std::make_shared<SourceMeasurements>();
    measurement->frame = "frame-" + std::to_string(k);
    measurement->psf = std::make_shared<EffectivePSF>();
    measurement->error = std::make_shared<ErrorRecord>();
    measurement->observation = std::make_shared<NativeObservation>();
    std::vector<double> lower;
    std::vector<double> upper;
    for (std::size_t s = 0; s < spec.stars; ++s) {
      const std::vector<double>& latent = built.gaia_truth[s];
      double flux = 0.0;
      for (std::size_t d = 0; d < spec.latent_dim; ++d) {
        flux += v[d] * latent[d];
      }
      double exponent = spec.log_a;
      if (spec.plane_basis) {
        const double u = (built.position_x[s] - spatial.center[0]) / spatial.scale[0];
        const double v_pos = (built.position_y[s] - spatial.center[1]) / spatial.scale[1];
        exponent += spec.spatial_first * (u - mean[1]);
        exponent += spec.spatial_second * (v_pos - mean[2]);
      }
      const double counts = spec.count_scale * std::exp(exponent) * flux;
      StarFlux star_flux;
      star_flux.gaia_source_id = 1000 + s;
      star_flux.position = PixelCenter0{built.position_x[s], built.position_y[s]};
      star_flux.integrated_flux = counts;
      star_flux.definition = "aperture";
      star_flux.window = Box{{built.position_x[s] - 3.0, built.position_y[s] - 3.0},
                             {built.position_x[s] + 3.0, built.position_y[s] + 3.0}};
      if (!spec.unknown_variance) {
        const double sigma = spec.relative_measurement_error * std::fabs(counts);
        star_flux.variance.value = sigma * sigma;
        star_flux.variance.state = NoiseState::known;
      } else {
        star_flux.variance.value = std::nullopt;
        star_flux.variance.state = NoiseState::unknown;
      }
      measurement->stars.push_back(star_flux);
      lower.push_back(counts - spec.enclosure_half_width);
      upper.push_back(counts + spec.enclosure_half_width);
    }
    if (!spec.missing_enclosure) {
      Result<ArrayRef> low_array = make_f64(fixture, lower, {static_cast<U64>(lower.size())},
                                             Unit{"adu", 0, 0, 0, 1.0, 1}, "flux:low");
      Result<ArrayRef> high_array = make_f64(fixture, upper, {static_cast<U64>(upper.size())},
                                              Unit{"adu", 0, 0, 0, 1.0, 1}, "flux:high");
      if (!good(low_array) || !good(high_array)) {
        Result<BuiltCase> out;
        out.code = good(low_array) ? high_array.code : low_array.code;
        return out;
      }
      measurement->flux_numerical_lower = get(low_array);
      measurement->flux_numerical_upper = get(high_array);
    }
    built.measurements.push_back(measurement);
  }

  // ---------------------------------------------------------------------
  // The request.
  // ---------------------------------------------------------------------
  built.request.responses = built.responses;
  built.request.basis = spatial;
  for (std::size_t k = 0; k < spec.frames; ++k) {
    built.request.frames.push_back("frame-" + std::to_string(k));
  }
  built.request.application_id = "validation-" + std::to_string(spec.frames) + "-" +
                                std::to_string(spec.stars);
  built.request.solve.rank_relative = 1e-12;
  built.request.solve.max_iterations = 100;
  built.request.gradient_tolerance = 1e-8;
  built.request.step_tolerance = 1e-10;
  built.request.max_outer_iterations = 100;
  built.request.likelihood = Condition{"photometric_scale", "joint_flux_gaussian"};
  return model_ok(std::move(built));
}

// ---------------------------------------------------------------------------
// R1: the analytic polynomial spectrum
// ---------------------------------------------------------------------------

void case_response_analytic(Fixture& fixture, Report& report) {
  for (int photon = 1; photon >= 0; --photon) {
    SpectralSetup setup;
    Result<SpectralSetup> built =
        build_top_hat_setup(fixture, 550.0, 20.0, 500.0, 600.0, photon != 0);
    if (!good(built)) {
      report.record("R1_response_analytic", false, "the setup could not be built");
      return;
    }
    // The basis of this setup has a single constant function; the analytic
    // truth is the exact integral of the top hat.
    const double low = 550.0 - 10.0;
    const double high = 550.0 + 10.0;
    const double truth =
        photon ? kPhotonFactor * (high * high - low * low) * 0.5 : 20.0;
    NumericBudget budget;
    budget.tolerance.absolute = 0.0;
    budget.tolerance.relative = 1e-13;
    budget.max_iterations = 200;
    Result<Ref<SpectralPrediction>> prediction =
        integrate_response(fixture.context, get(built).response, get(built).basis, budget);
    if (!good(prediction)) {
      report.record("R1_response_analytic", false, "integrate_response failed");
      return;
    }
    Result<std::vector<double>> integrals =
        read_f64(fixture, get(prediction)->basis_integrals);
    if (!good(integrals)) {
      report.record("R1_response_analytic", false, "the basis integrals cannot be read");
      return;
    }
    const double value = get(integrals).front();
    const double difference = relative_difference(value, truth);
    std::ostringstream message;
    message << (photon ? "photon" : "energy") << " convention, truth " << std::setprecision(17)
            << truth << ", computed " << value << ", relative difference " << difference;
    report.record(photon ? "R1_response_analytic_photon" : "R1_response_analytic_energy",
                  difference <= 1e-15, message.str());
  }
}

// ---------------------------------------------------------------------------
// R2: a sampled response with breakpoints
// ---------------------------------------------------------------------------

void case_response_segmented(Fixture& fixture, Report& report) {
  // A piecewise linear response with four nodes and a constant basis function
  // on [500, 600].
  const std::vector<double> wavelength{500.0, 520.0, 545.0, 570.0, 600.0};
  const std::vector<double> throughput{0.0, 0.4, 1.0, 0.25, 0.0};
  auto basis = std::make_shared<SpectralBasis>();
  basis->identity.schema_id = "acsd.SpectralBasis";
  basis->identity.schema_major = 2;
  basis->identity.schema_minor = 0;
  basis->identity.revision = 1;
  basis->identity.object_id = "basis:segmented";
  basis->identity.content_hash = fake_hash(basis->identity.object_id);
  basis->identity.run_id = "validation";
  basis->degree = 0;
  basis->coefficient_count = 1;
  basis->support_nm = Interval{500.0, 600.0};
  Result<ArrayRef> knots =
      make_f64(fixture, {500.0, 600.0}, {2}, Unit{"nm", 0, 0, 1, 1.0, 0}, "basis:segmented");
  Result<ArrayRef> coefficients =
      make_f64(fixture, {1.0}, {1, 1, 1}, Unit{"1", 0, 0, 0, 1.0, 0}, "basis:segmented");
  auto response = std::make_shared<Response>();
  response->identity.schema_id = "acsd.Response";
  response->identity.schema_major = 2;
  response->identity.schema_minor = 0;
  response->identity.revision = 1;
  response->identity.object_id = "response:segmented";
  response->identity.content_hash = fake_hash(response->identity.object_id);
  response->identity.run_id = "validation";
  response->convention = Response::Convention::photon_throughput;
  response->provenance = "synthetic sampled response";
  response->normalization = "unit peak";
  response->support_nm = Interval{500.0, 600.0};
  Result<ArrayRef> wave_array =
      make_f64(fixture, wavelength, {static_cast<U64>(wavelength.size())}, Unit{"nm", 0, 0, 1, 1.0, 0},
               "response:segmented");
  Result<ArrayRef> throughput_array =
      make_f64(fixture, throughput, {static_cast<U64>(throughput.size())}, Unit{"1", 0, 0, 0, 1.0, 0},
               "response:segmented");
  if (!good(knots) || !good(coefficients) || !good(wave_array) || !good(throughput_array)) {
    report.record("R2_response_segmented", false, "the setup could not be built");
    return;
  }
  basis->knots_nm = get(knots);
  basis->basis_coefficients = get(coefficients);
  response->wavelength_nm = get(wave_array);
  response->throughput = get(throughput_array);

  // The analytic truth: the exact integral of the piecewise linear response
  // times lambda, computed segment by segment.
  double truth = 0.0;
  for (std::size_t i = 0; i + 1 < wavelength.size(); ++i) {
    const double a = wavelength[i];
    const double b = wavelength[i + 1];
    const double ta = throughput[i];
    const double tb = throughput[i + 1];
    const double slope = (tb - ta) / (b - a);
    // int (ta + slope (lambda - a)) lambda dlambda
    // int (ta + slope (x - a)) x dx from a to lambda
    const auto primitive = [&](double lambda) {
      return ta * (lambda * lambda - a * a) / 2.0 +
             slope * (lambda * lambda * lambda / 3.0 - a * lambda * lambda / 2.0 + a * a * a / 6.0);
    };
    truth += kPhotonFactor * (primitive(b) - primitive(a));
  }

  NumericBudget budget;
  budget.tolerance.absolute = 0.0;
  budget.tolerance.relative = 1e-13;
  budget.max_iterations = 200;
  Result<Ref<SpectralPrediction>> prediction =
      integrate_response(fixture.context, response, basis, budget);
  if (!good(prediction)) {
    report.record("R2_response_segmented", false, "integrate_response failed");
    return;
  }
  Result<std::vector<double>> integrals = read_f64(fixture, get(prediction)->basis_integrals);
  if (!good(integrals)) {
    report.record("R2_response_segmented", false, "the basis integrals cannot be read");
    return;
  }
  const double value = get(integrals).front();
  const double difference = relative_difference(value, truth);
  std::ostringstream message;
  message << "truth " << std::setprecision(17) << truth << ", computed " << value
          << ", relative difference " << difference;
  report.record("R2_response_segmented", difference <= 1e-14, message.str());

  // A top hat whose edge lies exactly on a knot of the basis: the integral
  // must not leak across the step.
  SpectralSetup setup;
  Result<SpectralSetup> built = build_top_hat_setup(fixture, 500.0, 100.0, 500.0, 600.0, true);
  if (!good(built)) {
    report.record("R2_response_top_hat_edge_on_knot", false, "the setup could not be built");
    return;
  }
  NumericBudget edge_budget;
  edge_budget.tolerance.absolute = 0.0;
  edge_budget.tolerance.relative = 1e-13;
  edge_budget.max_iterations = 200;
  Result<Ref<SpectralPrediction>> edge =
      integrate_response(fixture.context, get(built).response, get(built).basis, edge_budget);
  if (!good(edge)) {
    report.record("R2_response_top_hat_edge_on_knot", false, "integrate_response failed");
    return;
  }
  Result<std::vector<double>> edge_integrals = read_f64(fixture, get(edge)->basis_integrals);
  const double edge_truth = kPhotonFactor * (550.0 * 550.0 - 500.0 * 500.0) * 0.5;
  const double edge_difference =
      good(edge_integrals) ? relative_difference(get(edge_integrals).front(), edge_truth) : 1.0;
  std::ostringstream edge_message;
  edge_message << "truth " << std::setprecision(17) << edge_truth << ", computed "
               << (good(edge_integrals) ? get(edge_integrals).front() : 0.0)
               << ", relative difference " << edge_difference;
  report.record("R2_response_top_hat_edge_on_knot", edge_difference <= 1e-15, edge_message.str());
}

// ---------------------------------------------------------------------------
// P1: the shared latent across two frames
// ---------------------------------------------------------------------------

void case_photometry_shared_latent(Fixture& fixture, Report& report) {
  CaseSpec spec;
  spec.plane_basis = true;
  spec.frames = 2;
  spec.stars = 6;
  Result<BuiltCase> built = build_case(fixture, spec);
  if (!good(built)) {
    report.record("P1_photometry_shared_latent", false,
                  "the case could not be built, code " + std::to_string(static_cast<int>(built.code)));
    return;
  }
  const BuiltCase& test = get(built);
  const std::vector<Ref<SourceMeasurements>> measurements(test.measurements);
  const std::vector<CatalogueStar> stars(test.stars);
  const std::vector<Ref<SpectralPrediction>> predictions(test.predictions);
  Result<Ref<PhotoSolution>> solution =
      fit_photometry(fixture.context, measurements, stars, predictions, test.request);
  if (!good(solution)) {
    std::string reason = "fit_photometry failed with code " +
                         std::to_string(static_cast<int>(solution.code));
    if (!solution.assessment.issues.empty()) {
      reason += ": " + solution.assessment.issues.front().reason;
    }
    report.record("P1_photometry_shared_latent", false, reason);
    return;
  }
  Result<std::vector<double>> theta = read_f64(fixture, get(solution)->theta);
  if (!good(theta)) {
    report.record("P1_photometry_shared_latent", false, "the parameters cannot be read");
    return;
  }
  const std::size_t terms = spec.plane_basis ? 3 : 1;
  const double recovered_log_a_0 = get(theta)[0];
  const double recovered_log_a_1 = get(theta)[terms];
  // The fitted logarithmic scale is the native ADU per unit photon rate: it
  // carries the count scale of the synthetic observation.
  const double truth_log_a = std::log(spec.count_scale) + spec.log_a;
  const double error_0 = std::fabs(recovered_log_a_0 - truth_log_a);
  const double error_1 = std::fabs(recovered_log_a_1 - truth_log_a);
  std::ostringstream message;
  message << "log a recovered " << std::setprecision(17) << recovered_log_a_0 << " and "
          << recovered_log_a_1 << " (truth " << truth_log_a << ")";
  const bool recovered = error_0 < 1e-9 && error_1 < 1e-9;
  report.record("P1_photometry_shared_latent", recovered, message.str());

  // The cross frame correlation of the two frame scales: the shared latent
  // must couple them, so the covariance is not diagonal.
  double cross = 0.0;
  double diagonal = 0.0;
  std::vector<double> diagonal_terms(2 * terms, 0.0);
  if (get(solution)->covariance) {
    // The covariance action is a factor action; the covariance matrix is
    // recovered from the quadratic form of the unit vectors.
    const std::size_t n = 2 * terms;
    for (std::size_t i = 0; i < n; ++i) {
      {
        std::vector<double> functional(n, 0.0);
        functional[i] = 1.0;
        Result<ArrayRef> input = make_f64(fixture, functional, {static_cast<U64>(n)},
                                           Unit{"1", 0, 0, 0, 1.0, 0}, "quadratic");
        if (good(input)) {
          Result<Variance> variance =
              quadratic_form(fixture.context, get(solution)->covariance, get(input));
          if (good(variance) && get(variance).value) {
            diagonal_terms[i] = *(get(variance).value);
          }
        }
      }
      for (std::size_t j = 0; j < n; ++j) {
        if (i != 0 || j != terms) {
          continue;
        }
        std::vector<double> functional(n, 0.0);
        functional[i] = 1.0;
        functional[j] = 1.0;
        Result<ArrayRef> input = make_f64(fixture, functional, {static_cast<U64>(n)},
                                           Unit{"1", 0, 0, 0, 1.0, 0}, "quadratic");
        if (!good(input)) {
          continue;
        }
        Result<Variance> variance =
            quadratic_form(fixture.context, get(solution)->covariance, get(input));
        if (!good(variance) || !get(variance).value) {
          continue;
        }
        // The quadratic form of (e_i + e_j) is C_ii + 2 C_ij + C_jj.
        cross = 0.5 * (*(get(variance).value) - diagonal_terms[i] - diagonal_terms[j]);
      }
    }
  }
  diagonal = diagonal_terms[0];
  const double correlation =
      diagonal > 0.0 ? cross / std::sqrt(diagonal * diagonal_terms[terms]) : 0.0;
  std::ostringstream correlation_message;
  correlation_message << "cov(log a_0, log a_1) = " << std::setprecision(17) << cross
                      << ", var(log a_0) = " << diagonal << ", correlation = " << correlation;
  // The shared latent couples the two frame scales: the correlation is non
  // zero and stays a valid correlation coefficient.
  const bool correlated = std::fabs(correlation) > 1e-6 && std::fabs(correlation) <= 1.0 + 1e-9;
  report.record("P1_photometry_cross_frame_correlation", correlated, correlation_message.str());
}

// ---------------------------------------------------------------------------
// P2: the rank deficiency of a collinear training set
// ---------------------------------------------------------------------------

void case_photometry_rank_deficient(Fixture& fixture, Report& report) {
  CaseSpec spec;
  spec.plane_basis = true;
  spec.frames = 1;
  spec.stars = 6;
  spec.collinear = true;
  Result<BuiltCase> built = build_case(fixture, spec);
  if (!good(built)) {
    report.record("P2_photometry_rank_deficient", false, "the case could not be built");
    return;
  }
  const BuiltCase& test = get(built);
  const std::vector<Ref<SourceMeasurements>> measurements(test.measurements);
  const std::vector<CatalogueStar> stars(test.stars);
  const std::vector<Ref<SpectralPrediction>> predictions(test.predictions);
  Result<Ref<PhotoSolution>> solution =
      fit_photometry(fixture.context, measurements, stars, predictions, test.request);
  std::ostringstream message;
  message << "the fit returned code " << static_cast<int>(solution.code);
  const bool rejected = solution.code == Code::not_identifiable;
  report.record("P2_photometry_rank_deficient", rejected, message.str());
}

// ---------------------------------------------------------------------------
// P3: an unknown measurement variance
// ---------------------------------------------------------------------------

void case_photometry_unknown_variance(Fixture& fixture, Report& report) {
  CaseSpec spec;
  spec.plane_basis = true;
  spec.frames = 2;
  spec.stars = 6;
  spec.unknown_variance = true;
  // The equal weight fallback is only meaningful when the measured counts are
  // of the order of one; the count scale is chosen accordingly.
  spec.count_scale = 1e-20;
  Result<BuiltCase> built = build_case(fixture, spec);
  if (!good(built)) {
    report.record("P3_photometry_unknown_variance", false, "the case could not be built");
    return;
  }
  const BuiltCase& test = get(built);
  const std::vector<Ref<SourceMeasurements>> measurements(test.measurements);
  const std::vector<CatalogueStar> stars(test.stars);
  const std::vector<Ref<SpectralPrediction>> predictions(test.predictions);
  Result<Ref<PhotoSolution>> solution =
      fit_photometry(fixture.context, measurements, stars, predictions, test.request);
  if (!good(solution)) {
    std::string reason = "fit_photometry failed with code " +
                         std::to_string(static_cast<int>(solution.code));
    if (!solution.assessment.issues.empty()) {
      reason += ": " + solution.assessment.issues.front().reason;
    }
    // With an unknown measurement variance the equal weight fallback cannot
    // anchor the scale; the fit must fail explicitly and must not publish a
    // scale at all.
    const bool no_scale = solution.code == Code::nonconverged ||
                          solution.code == Code::not_identifiable ||
                          solution.code == Code::unavailable;
    report.record("P3_photometry_unknown_variance", no_scale, reason);
    return;
  }
  Result<std::vector<double>> theta = read_f64(fixture, get(solution)->theta);
  if (!good(theta)) {
    report.record("P3_photometry_unknown_variance", false, "the parameters cannot be read");
    return;
  }
  bool recorded = false;
  if (get(solution)->error) {
    for (const std::string& term : get(solution)->error->unknown_model_terms) {
      if (term.find("variance") != std::string::npos || term.find("covariance") != std::string::npos) {
        recorded = true;
      }
    }
  }
  std::ostringstream message;
  message << "log a recovered " << std::setprecision(17) << get(theta)[0]
          << ", the unknown term is recorded: " << (recorded ? "yes" : "no")
          << ", the assessment is conditional: "
          << (get(solution)->assessment.science == ScienceState::conditional ? "yes" : "no");
  // A published scale must stay conditional when the measurement variance is
  // unknown.
  const bool conditional = get(solution)->assessment.science == ScienceState::conditional;
  const double truth_log_a = std::log(spec.count_scale) + spec.log_a;
  const double error = std::fabs(get(theta)[0] - truth_log_a);
  message << ", the logarithmic scale error is " << std::setprecision(6) << error;
  report.record("P3_photometry_unknown_variance", recorded && conditional && error < 1e-6,
                message.str());
}

// ---------------------------------------------------------------------------
// F01: the numerical enclosure of the counts changes the scale interval
// ---------------------------------------------------------------------------

struct ScaleInterval {
  bool built = false;
  double log_a = 0.0;
  double lower = 0.0;
  double upper = 0.0;
  bool has_box = false;
};

ScaleInterval measure_scale_interval(Fixture& fixture, double enclosure_half_width) {
  ScaleInterval result;
  // One frame, one star, the counts are exactly 100 and the model predicts
  // exactly 100, so the true scale is exactly one.
  CaseSpec spec;
  spec.plane_basis = false;
  spec.frames = 1;
  spec.stars = 1;
  spec.latent_dim = 2;
  spec.log_a = 0.0;
  spec.target_counts = 100.0;
  spec.enclosure_half_width = enclosure_half_width;
  Result<BuiltCase> built = build_case(fixture, spec);
  if (!good(built)) {
    return result;
  }
  const BuiltCase& test = get(built);
  const std::vector<Ref<SourceMeasurements>> measurements(test.measurements);
  const std::vector<CatalogueStar> stars(test.stars);
  const std::vector<Ref<SpectralPrediction>> predictions(test.predictions);
  Result<Ref<PhotoSolution>> solution =
      fit_photometry(fixture.context, measurements, stars, predictions, test.request);
  if (!good(solution)) {
    return result;
  }
  Result<std::vector<double>> theta = read_f64(fixture, get(solution)->theta);
  if (!good(theta)) {
    return result;
  }
  result.built = true;
  result.log_a = get(theta).front();
  if (get(solution)->parameter_lower && get(solution)->parameter_upper) {
    Result<std::vector<double>> lower = read_f64(fixture, get(solution)->parameter_lower);
    Result<std::vector<double>> upper = read_f64(fixture, get(solution)->parameter_upper);
    if (good(lower) && good(upper)) {
      result.has_box = true;
      result.lower = get(lower).front();
      result.upper = get(upper).front();
    }
  }
  return result;
}

void case_scale_interval_f01(Fixture& fixture, Report& report) {
  const ScaleInterval tight = measure_scale_interval(fixture, 0.0);
  const ScaleInterval loose = measure_scale_interval(fixture, 1.0);
  if (!tight.built || !loose.built) {
    report.record("F01_scale_interval", false, "the scale interval could not be measured");
    return;
  }
  std::ostringstream message;
  message << "tight box [" << std::setprecision(17) << tight.lower << ", " << tight.upper
          << "], loose box [" << loose.lower << ", " << loose.upper << "]";
  // The exact interval of the scale for the enclosure [99, 101] and a true
  // flux of 100 is a in [0.99, 1.01], that is a relative width of one percent
  // around the fitted logarithmic scale.
  const double exact_low = loose.log_a + std::log(0.99);
  const double exact_high = loose.log_a + std::log(1.01);
  const bool contains =
      loose.has_box && loose.lower <= exact_low + 1e-12 && loose.upper >= exact_high - 1e-12;
  report.record("F01_scale_interval", contains, message.str());

  // The narrow enclosure must give a much narrower interval than the wide
  // one, and the difference must come from the numerical enclosure only.
  const double tight_width = tight.has_box ? tight.upper - tight.lower : 1.0;
  const double loose_width = loose.has_box ? loose.upper - loose.lower : 0.0;
  std::ostringstream width_message;
  width_message << "tight width " << std::setprecision(17) << tight_width << ", loose width "
                << loose_width;
  report.record("F01_scale_interval_width", tight_width < 1e-9 && loose_width > 1e-3,
                width_message.str());
}

// ---------------------------------------------------------------------------
// S1: apply_scale acts on every native pixel
// ---------------------------------------------------------------------------

void case_apply_scale(Fixture& fixture, Report& report) {
  CaseSpec spec;
  spec.plane_basis = true;
  spec.frames = 1;
  spec.stars = 4;
  Result<BuiltCase> built = build_case(fixture, spec);
  if (!good(built)) {
    report.record("S1_apply_scale", false, "the case could not be built");
    return;
  }
  const BuiltCase& test = get(built);
  const std::vector<Ref<SourceMeasurements>> measurements(test.measurements);
  const std::vector<CatalogueStar> stars(test.stars);
  const std::vector<Ref<SpectralPrediction>> predictions(test.predictions);
  Result<Ref<PhotoSolution>> solution =
      fit_photometry(fixture.context, measurements, stars, predictions, test.request);
  if (!good(solution)) {
    report.record("S1_apply_scale", false, "fit_photometry failed");
    return;
  }

  // A native observation with a 4 x 4 detector.
  const U64 width = 4;
  const U64 height = 4;
  const U64 samples = width * height;
  Index native_index = make_index(samples, Index::Kind::native, "native");
  auto footprints = std::make_shared<FootprintSet>();
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->identity.revision = 1;
  footprints->identity.object_id = "footprints:s1";
  footprints->identity.content_hash = fake_hash("footprints:s1");
  footprints->identity.run_id = "validation";
  footprints->index = std::make_shared<const Index>(native_index);
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = Mat2{1.0, 0.0, 0.0, 1.0};
  footprints->offset = Vec2{0.0, 0.0};
  footprints->local_cell = Box{{-0.5, -0.5}, {0.5, 0.5}};
  footprints->detector_frame = "detector";

  std::vector<double> values(samples, 10.0);
  Result<ArrayRef> values_array =
      make_f64(fixture, values, {samples}, Unit{"adu", 0, 0, 0, 1.0, 1}, native_index.identity.object_id);
  Result<ArrayRef> lower_array =
      make_f64(fixture, values, {samples}, Unit{"adu", 0, 0, 0, 1.0, 1}, native_index.identity.object_id);
  if (!good(values_array) || !good(lower_array)) {
    report.record("S1_apply_scale", false, "the native observation cannot be built");
    return;
  }
  auto observation = std::make_shared<NativeObservation>();
  observation->integrated_values = get(values_array);
  observation->numerical_lower = get(lower_array);
  observation->numerical_upper = get(lower_array);
  observation->native_index = std::make_shared<const Index>(native_index);
  observation->footprints = footprints;
  observation->unit = Unit{"adu", 0, 0, 0, 1.0, 1};
  observation->frame = "frame-0";
  observation->physical_collection = true;
  observation->error = std::make_shared<ErrorRecord>();
  Result<Ref<LinearAction>> identity = make_identity_action(fixture, native_index);
  if (!good(identity)) {
    report.record("S1_apply_scale", false, "the identity action cannot be built");
    return;
  }
  observation->regrouping = get(identity);

  Result<Ref<NativeObservation>> scaled =
      apply_scale(fixture.context, get(solution), observation, "frame-0");
  if (!good(scaled)) {
    report.record("S1_apply_scale", false,
                  "apply_scale failed with code " + std::to_string(static_cast<int>(scaled.code)));
    return;
  }
  Result<std::vector<double>> scaled_values = read_f64(fixture, get(scaled)->integrated_values);
  if (!good(scaled_values)) {
    report.record("S1_apply_scale", false, "the scaled values cannot be read");
    return;
  }
  // The factor must differ from pixel to pixel, because the spatial field is
  // not constant over the detector.
  double smallest = std::numeric_limits<double>::infinity();
  double largest = -std::numeric_limits<double>::infinity();
  for (double value : get(scaled_values)) {
    smallest = std::min(smallest, value / 10.0);
    largest = std::max(largest, value / 10.0);
  }
  std::ostringstream message;
  message << "the per pixel factors range from " << std::setprecision(17) << smallest << " to "
          << largest;
  const bool per_pixel = (largest - smallest) > 1e-6;
  report.record("S1_apply_scale_per_pixel", per_pixel, message.str());

  // A repeated application with the same application id is refused.
  Result<Ref<NativeObservation>> repeated =
      apply_scale(fixture.context, get(solution), get(scaled), "frame-0");
  std::ostringstream conflict_message;
  conflict_message << "the repeated application returned code "
                   << static_cast<int>(repeated.code);
  report.record("S1_apply_scale_identity_conflict", repeated.code == Code::identity_conflict,
                conflict_message.str());
}

// ---------------------------------------------------------------------------

void run_all_cases(Fixture& fixture, Report& report) {
  case_response_analytic(fixture, report);
  case_response_segmented(fixture, report);
  case_photometry_shared_latent(fixture, report);
  case_photometry_rank_deficient(fixture, report);
  case_photometry_unknown_variance(fixture, report);
  case_scale_interval_f01(fixture, report);
  case_apply_scale(fixture, report);
}
