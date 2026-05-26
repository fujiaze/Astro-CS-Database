#include "sdet_background.h"
#include "sdet_log.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#define SEX_BIG 1e30f
#define SEX_QUANTIF_NSIGMA 5
#define SEX_QUANTIF_NMAXLEVELS 4096
#define SEX_QUANTIF_AMIN 4
#define SEX_BACK_MINGOODFRAC 0.5f

static float sex_fast_median(float* data, int n) {
    if (n <= 0) return 0.0f;
    std::nth_element(data, data + n / 2, data + n);
    if (n % 2 == 0) {
        float a = data[n / 2];
        std::nth_element(data, data + n / 2 - 1, data + n / 2);
        return (data[n / 2 - 1] + a) * 0.5f;
    }
    return data[n / 2];
}

void sex_backstat(BackMesh* mesh, const float* buf, int bufsize, int w, int bw) {
    int h = bufsize / w;
    double mean = 0.0, sigma = 0.0;
    int npix = 0;
    
    for (int i = 0; i < bufsize; i++) {
        float pix = buf[i];
        if (pix > -SEX_BIG) {
            mean += pix;
            sigma += pix * pix;
            npix++;
        }
    }
    
    if ((float)npix < (float)(bw * h * SEX_BACK_MINGOODFRAC)) {
        mesh->mean = mesh->sigma = -SEX_BIG;
        return;
    }
    
    mean /= npix;
    double sig = sigma / npix - mean * mean;
    sigma = sig > 0.0 ? sqrt(sig) : 0.0;
    
    float lcut = (float)(mean - 2.0 * sigma);
    float hcut = (float)(mean + 2.0 * sigma);
    
    mean = 0.0;
    sigma = 0.0;
    npix = 0;
    
    for (int i = 0; i < bufsize; i++) {
        float pix = buf[i];
        if (pix <= hcut && pix >= lcut) {
            mean += pix;
            sigma += pix * pix;
            npix++;
        }
    }
    
    mesh->npix = npix;
    mean /= npix;
    sig = sigma / npix - mean * mean;
    sigma = sig > 0.0 ? sqrt(sig) : 0.0;
    
    mesh->mean = (float)mean;
    mesh->sigma = (float)sigma;
    mesh->lcut = lcut;
    mesh->hcut = hcut;
    
    double step = sqrt(2.0 / 3.14159265359) * SEX_QUANTIF_NSIGMA / SEX_QUANTIF_AMIN;
    mesh->nlevels = (int)(step * npix + 1);
    if (mesh->nlevels > SEX_QUANTIF_NMAXLEVELS) mesh->nlevels = SEX_QUANTIF_NMAXLEVELS;
    mesh->qscale = sigma > 0.0f ? 2.0f * SEX_QUANTIF_NSIGMA * (float)sigma / mesh->nlevels : 1.0f;
    mesh->qzero = (float)mean - SEX_QUANTIF_NSIGMA * (float)sigma;
}

void sex_backhisto(BackMesh* mesh, const float* buf, int bufsize, int w, int bw) {
    if (mesh->mean <= -SEX_BIG) return;
    
    int h = bufsize / w;
    int nlevels = mesh->nlevels;
    mesh->histo.assign(nlevels, 0);
    
    float qscale = mesh->qscale;
    float cste = 0.499999f - mesh->qzero / qscale;
    
    for (int i = 0; i < bufsize; i++) {
        float pix = buf[i];
        int bin = (int)(pix / qscale + cste);
        if (bin >= 0 && bin < nlevels) {
            mesh->histo[bin]++;
        }
    }
}

float sex_backguess(BackMesh* mesh, float* mean, float* sigma) {
#define EPS 1e-4
    if (mesh->mean <= -SEX_BIG) {
        *mean = *sigma = -SEX_BIG;
        return -SEX_BIG;
    }
    
    int nlevels = mesh->nlevels;
    std::vector<int>& histo = mesh->histo;
    
    int lcut = 0, hcut = nlevels - 1;
    double sig = 10.0 * (nlevels - 1);
    double sig1 = 1.0;
    double mea = mesh->mean;
    double med = mesh->mean;
    
    for (int n = 100; n-- && sig >= 0.1 && fabs(sig / sig1 - 1.0) > EPS;) {
        sig1 = sig;
        double sum = 0.0, mea_sum = 0.0, sig_sum = 0.0;
        unsigned long lowsum = 0, highsum = 0;
        
        int hilow = lcut, hihigh = hcut;
        
        for (int i = lcut; i <= hcut; i++) {
            int pix = histo[i];
            if (lowsum < highsum) {
                lowsum += histo[hilow++];
            } else {
                highsum += histo[hihigh--];
            }
            sum += pix;
            mea_sum += (double)pix * i;
            sig_sum += (double)pix * i * i;
        }
        
        med = hihigh >= 0 ? 
            ((hihigh) + 0.5 + ((double)highsum - lowsum) / (2.0 * std::max(histo[hilow], histo[hihigh]))) 
            : 0.0;
        
        if (sum > 0) {
            mea = mea_sum / sum;
            sig = sig_sum / sum - mea * mea;
        }
        sig = sig > 0.0 ? sqrt(sig) : 0.0;
        
        lcut = (int)(med - 3.0 * sig);
        hcut = (int)(med + 3.0 * sig);
        lcut = std::max(0, lcut);
        hcut = std::min(nlevels - 1, hcut);
    }
    
    *mean = mesh->qzero + (float)mea * mesh->qscale;
    *sigma = (float)sig * mesh->qscale;
    
    return *mean;
#undef EPS
}

void sex_filterback(BackMap* bmap) {
    int nx = bmap->nx;
    int ny = bmap->ny;
    int np = nx * ny;
    
    std::vector<float> back2(np), sigma2(np);
    
    int nfx = 3, nfy = 3;
    int npx = nfx / 2, npy = nfy / 2;
    
    std::vector<float> bmask((2 * npx + 1) * (2 * npy + 1));
    std::vector<float> smask((2 * npx + 1) * (2 * npy + 1));
    
    for (int py = 0; py < ny; py++) {
        int npy2 = std::min({py, ny - py - 1, npy});
        for (int px = 0; px < nx; px++) {
            int npx2 = std::min({px, nx - px - 1, npx});
            
            int i = 0;
            for (int dpy = -npy2; dpy <= npy2; dpy++) {
                for (int dpx = -npx2; dpx <= npx2; dpx++) {
                    int idx = (py + dpy) * nx + (px + dpx);
                    bmask[i] = bmap->back[idx];
                    smask[i++] = bmap->sigma[idx];
                }
            }
            
            float med = sex_fast_median(bmask.data(), i);
            back2[py * nx + px] = med;
            sigma2[py * nx + px] = sex_fast_median(smask.data(), i);
        }
    }
    
    bmap->back = std::move(back2);
    bmap->sigma = std::move(sigma2);
    
    std::vector<float> tmp_back = bmap->back;
    bmap->backmean = sex_fast_median(tmp_back.data(), np);
    
    std::vector<float> tmp_sigma = bmap->sigma;
    bmap->backsig = sex_fast_median(tmp_sigma.data(), np);
}

void sex_makeback(const float* image, int w, int h, int mesh_size, BackMap* bmap) {
    bmap->width = w;
    bmap->height = h;
    bmap->mesh_width = mesh_size;
    bmap->mesh_height = mesh_size;
    bmap->nx = (w + mesh_size - 1) / mesh_size;
    bmap->ny = (h + mesh_size - 1) / mesh_size;
    
    int nb = bmap->nx * bmap->ny;
    bmap->back.resize(nb);
    bmap->sigma.resize(nb);
    
    sdet_log(SDET_LOG_INFO, "BACKGROUND", "Computing background map: %dx%d meshes (%dx%d each)", 
             bmap->nx, bmap->ny, mesh_size, mesh_size);
    
    std::vector<BackMesh> meshes(bmap->nx);
    
    for (int jy = 0; jy < bmap->ny; jy++) {
        for (int jx = 0; jx < bmap->nx; jx++) {
            int x0 = jx * mesh_size;
            int y0 = jy * mesh_size;
            int x1 = std::min(x0 + mesh_size, w);
            int y1 = std::min(y0 + mesh_size, h);
            int bw = x1 - x0;
            int bh = y1 - y0;
            
            std::vector<float> buf(bw * bh);
            for (int y = 0; y < bh; y++) {
                for (int x = 0; x < bw; x++) {
                    buf[y * bw + x] = image[(y0 + y) * w + (x0 + x)];
                }
            }
            
            sex_backstat(&meshes[jx], buf.data(), bw * bh, bw, bw);
            
            if (meshes[jx].mean > -SEX_BIG) {
                sex_backhisto(&meshes[jx], buf.data(), bw * bh, bw, bw);
                float mean, sigma;
                sex_backguess(&meshes[jx], &mean, &sigma);
                
                int k = jx + bmap->nx * jy;
                bmap->back[k] = mean;
                bmap->sigma[k] = sigma;
            } else {
                int k = jx + bmap->nx * jy;
                bmap->back[k] = 0.0f;
                bmap->sigma[k] = 1.0f;
            }
        }
    }
    
    sex_filterback(bmap);
    
    sdet_log(SDET_LOG_INFO, "BACKGROUND", "Background: mean=%.2f sigma=%.2f", 
             bmap->backmean, bmap->backsig);
}

float sex_get_back(const BackMap* bmap, int x, int y) {
    int nx = bmap->nx;
    int ny = bmap->ny;
    
    double dx = (double)x / bmap->mesh_width - 0.5;
    double dy = (double)y / bmap->mesh_height - 0.5;
    
    int xl = (int)dx;
    int yl = (int)dy;
    dx -= xl;
    dy -= yl;
    
    if (xl < 0) { xl = 0; dx -= 1.0; }
    else if (xl >= nx - 1) { xl = nx < 2 ? 0 : nx - 2; dx += 1.0; }
    
    if (yl < 0) { yl = 0; dy -= 1.0; }
    else if (yl >= ny - 1) { yl = ny < 2 ? 0 : ny - 2; dy += 1.0; }
    
    int pos = yl * nx + xl;
    double cdx = 1.0 - dx;
    
    float b0 = bmap->back[pos];
    float b1 = nx < 2 ? b0 : bmap->back[pos + 1];
    float b2 = ny < 2 ? b0 : bmap->back[pos + nx];
    float b3 = nx < 2 ? b2 : bmap->back[pos + nx + 1];
    
    return (float)((1.0 - dy) * (cdx * b0 + dx * b1) + dy * (dx * b2 + cdx * b3));
}

float sex_get_sigma(const BackMap* bmap, int x, int y) {
    int nx = bmap->nx;
    int ny = bmap->ny;
    
    double dx = (double)x / bmap->mesh_width - 0.5;
    double dy = (double)y / bmap->mesh_height - 0.5;
    
    int xl = (int)dx;
    int yl = (int)dy;
    dx -= xl;
    dy -= yl;
    
    if (xl < 0) { xl = 0; dx -= 1.0; }
    else if (xl >= nx - 1) { xl = nx < 2 ? 0 : nx - 2; dx += 1.0; }
    
    if (yl < 0) { yl = 0; dy -= 1.0; }
    else if (yl >= ny - 1) { yl = ny < 2 ? 0 : ny - 2; dy += 1.0; }
    
    int pos = yl * nx + xl;
    double cdx = 1.0 - dx;
    
    float s0 = bmap->sigma[pos];
    float s1 = nx < 2 ? s0 : bmap->sigma[pos + 1];
    float s2 = ny < 2 ? s0 : bmap->sigma[pos + nx];
    float s3 = nx < 2 ? s2 : bmap->sigma[pos + nx + 1];
    
    return (float)((1.0 - dy) * (cdx * s0 + dx * s1) + dy * (dx * s2 + cdx * s3));
}
