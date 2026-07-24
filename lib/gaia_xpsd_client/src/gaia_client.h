#ifndef GAIA_CLIENT_H
#define GAIA_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define GAIA_EXPORT __declspec(dllexport)
#else
#define GAIA_EXPORT __attribute__((visibility("default")))
#endif

typedef enum {
    GAIA_DB_AUTO = 0,
    GAIA_DB_DR3 = 1,
    GAIA_DB_DR3SP = 2
} GaiaDbType;

typedef struct {
    double ra;
    double dec;
    double magG;
    double magBP;
    double magRP;
    float parallax;
    float pmra;
    float pmdec;
    int64_t source_id;
} GaiaStar;

typedef struct {
    double ra;
    double dec;
    double magG;
} GaiaSpectrumStar;

typedef struct {
    double ra;
    double dec;
    double magG;
    double magBP;
    double magRP;
} GaiaPhotometryStar;

typedef struct GaiaClient GaiaClient;

#ifdef __cplusplus
extern "C" {
#endif

GAIA_EXPORT GaiaClient *gaia_client_create(const char *data_dir);
GAIA_EXPORT GaiaClient *gaia_client_create_ex(const char *data_dir, GaiaDbType db_type);
GAIA_EXPORT void gaia_client_destroy(GaiaClient *client);

GAIA_EXPORT int gaia_client_cone_search(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaStar **out_stars, int *out_count);

GAIA_EXPORT int gaia_client_cone_search_for_solver(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_high,
    double **out_ra, double **out_dec, float **out_mag,
    int *out_count);

GAIA_EXPORT int gaia_client_get_db_type(GaiaClient *client);
GAIA_EXPORT int gaia_client_get_file_count(GaiaClient *client);
GAIA_EXPORT int gaia_client_get_total_sources(GaiaClient *client);

GAIA_EXPORT int gaia_client_cone_search_with_spectrum(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int *out_count);

GAIA_EXPORT int gaia_client_query_spectrum_by_coords(
    GaiaClient *client,
    const double *ra_list,
    const double *dec_list,
    int n_coords,
    double match_radius_arcsec,
    double mag_low,
    double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int **out_match_idx,
    int *out_count);

GAIA_EXPORT int gaia_client_cone_search_with_photometry(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaPhotometryStar **out_stars,
    int *out_count);

GAIA_EXPORT int gaia_client_get_spectrum_params(
    GaiaClient *client,
    int *out_start_nm,
    int *out_step_nm,
    int *out_count);

#ifdef __cplusplus
}
#endif

#endif
