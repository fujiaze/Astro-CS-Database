#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *data_dir = argc > 1 ? argv[1] : "./GaiaDR3SP";
    double ra = argc > 2 ? atof(argv[2]) : 266.4167;
    double dec = argc > 3 ? atof(argv[3]) : -28.9867;
    double radius = argc > 4 ? atof(argv[4]) : 1.0;
    double mag_high = argc > 5 ? atof(argv[5]) : 14.0;

    printf("Loading XPSD files from: %s\n", data_dir);
    GaiaClient *client = gaia_client_create(data_dir);
    if (!client) {
        fprintf(stderr, "Failed to create GaiaClient\n");
        return 1;
    }

    printf("Searching: ra=%.4f dec=%.4f radius=%.2f mag<%.1f\n", ra, dec, radius, mag_high);

    GaiaStar *stars = NULL;
    int count = 0;
    int ret = gaia_client_cone_search(client, ra, dec, radius, -1.5, mag_high, &stars, &count);

    if (ret != 0) {
        fprintf(stderr, "Cone search failed\n");
        gaia_client_destroy(client);
        return 1;
    }

    printf("Found %d stars\n", count);
    for (int i = 0; i < count && i < 20; i++) {
        printf("  [%d] ra=%.6f dec=%.6f magG=%.3f\n", i, stars[i].ra, stars[i].dec, stars[i].magG);
    }
    if (count > 20) printf("  ... (%d more)\n", count - 20);

    free(stars);
    gaia_client_destroy(client);
    return 0;
}
