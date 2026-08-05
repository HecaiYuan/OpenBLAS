/* Test harness for dgemv_n_lsx.S and dgemv_t_lsx.S kernels.
 * Kernel signature (LoongArch LP64d):
 *   void CNAME(long m, long n, long dummy1, double alpha, double *a,
 *              long lda, double *x, long incx, double *y, long incy,
 *              double *buffer);
 * Computes y = alpha*A*x + y  (beta=1 assumed by kernel).
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

void dgemv_n_kern(long m, long n, long dummy1, double alpha, double *a,
                  long lda, double *x, long incx, double *y, long incy,
                  double *buffer);
void dgemv_t_kern(long m, long n, long dummy1, double alpha, double *a,
                  long lda, double *x, long incx, double *y, long incy,
                  double *buffer);

static double *alloc_d(long cnt) {
    void *p = NULL;
    if (cnt < 1) cnt = 1;
    if (posix_memalign(&p, 16, cnt * sizeof(double))) { perror("mem"); exit(2); }
    memset(p, 0, cnt * sizeof(double));
    return (double *)p;
}

static void ref_n(long m, long n, double alpha, const double *a, long lda,
                  const double *x, long incx, double *y, long incy) {
    for (long i = 0; i < m; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++)
            s += a[i + j * lda] * x[j * incx];
        y[i * incy] += alpha * s;
    }
}

static void ref_t(long m, long n, double alpha, const double *a, long lda,
                  const double *x, long incx, double *y, long incy) {
    for (long j = 0; j < n; j++) {
        double s = 0.0;
        for (long i = 0; i < m; i++)
            s += a[i + j * lda] * x[i * incx];
        y[j * incy] += alpha * s;
    }
}

static int almost_equal(double a, double b, double tol_abs, double tol_rel) {
    double d = fabs(a - b);
    if (d <= tol_abs) return 1;
    double mg = fmax(fabs(a), fabs(b));
    if (mg == 0.0) return d <= tol_abs;
    return (d / mg) <= tol_rel;
}

static long failures = 0;
static long tests = 0;
static long shown = 0;

static void test_one_lda(int trans, long m, long n, long lda, double alpha, long incx, long incy);

static void test_one(int trans, long m, long n, double alpha, long incx, long incy) {
    long lda = m + 1;
    test_one_lda(trans, m, n, lda, alpha, incx, incy);
}

static void test_one_lda(int trans, long m, long n, long lda, double alpha, long incx, long incy) {
    long xcnt = trans ? m : n;
    long ycnt = trans ? n : m;
    /* allocate enough to cover both forward and backward striding */
    long xslots = xcnt * (incx > 0 ? incx : -incx) + 8;
    long yslots = ycnt * (incy > 0 ? incy : -incy) + 8;
    double *a = alloc_d(lda * n + 4);
    double *x_base = alloc_d(xslots);
    double *yref_base = alloc_d(yslots);
    double *yker_base = alloc_d(yslots);
    double *buf  = alloc_d(256);

    unsigned long seed = (unsigned long)(trans * 100003UL + (unsigned long)m * 97UL + (unsigned long)n * 13UL
                       + (unsigned long)(alpha * 7.0) + (unsigned long)(incx+99) * 31UL + (unsigned long)(incy+99) * 101UL);
    for (long i = 0; i < lda * n; i++) { seed = seed * 6364136223846793005UL + 1442695040888963407UL; a[i] = ((double)((seed >> 33) & 0xfffff) / 1048575.0 - 0.5) * 2.0; }
    /* fill x with stride-aware indexing into the BASE (forward) array */
    for (long i = 0; i < xcnt; i++) { seed = seed * 6364136223846793005UL + 1442695040888963407UL; x_base[i] = ((double)((seed >> 33) & 0xfffff) / 1048575.0 - 0.5) * 2.0; }
    for (long i = 0; i < ycnt; i++) { seed = seed * 6364136223846793005UL + 1442695040888963407UL; yref_base[i] = ((double)((seed >> 33) & 0xfffff) / 1048575.0 - 0.5) * 2.0; }

    /* Mimic OpenBLAS gemv.c: for negative inc, adjust base pointer to last element */
    double *x = x_base, *yref = yref_base, *yker = yker_base;
    if (incx < 0) x = x_base + (xcnt - 1) * (-incx);
    if (incy < 0) { yref = yref_base + (ycnt - 1) * (-incy); yker = yker_base + (ycnt - 1) * (-incy); }
    memcpy(yker_base, yref_base, yslots * sizeof(double));

    if (trans) ref_t(m, n, alpha, a, lda, x, incx, yref, incy);
    else       ref_n(m, n, alpha, a, lda, x, incx, yref, incy);

    if (trans) dgemv_t_kern(m, n, 0, alpha, a, lda, x, incx, yker, incy, buf);
    else       dgemv_n_kern(m, n, 0, alpha, a, lda, x, incx, yker, incy, buf);

    tests++;
    int bad = 0;
    for (long i = 0; i < ycnt; i++) {
        if (!almost_equal(yref[i * incy], yker[i * incy], 1e-13, 1e-13)) {
            bad = 1;
            if (shown < 25) {
                shown++;
                printf("  [%s m=%ld n=%ld alpha=%.4g incx=%ld incy=%ld] y[%ld]: ref=%.17g kern=%.17g diff=%.3g\n",
                       trans ? "T" : "N", m, n, alpha, incx, incy, i,
                       yref[i * incy], yker[i * incy],
                       fabs(yref[i * incy] - yker[i * incy]));
            }
        }
    }
    if (bad) failures++;

    free(a); free(x); free(yref); free(yker); free(buf);
}

int main(void) {
    long sizes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17, 31, 33, 63, 64, 65, 100};
    int nsz = sizeof(sizes) / sizeof(sizes[0]);
    double alphas[] = {1.0, -1.0, 2.5, 0.0, -3.14};

    for (int trans = 0; trans < 2; trans++) {
        for (int mi = 0; mi < nsz; mi++)
            for (int ni = 0; ni < nsz; ni++) {
                long m = sizes[mi], n = sizes[ni];
                if (m == 0 || n == 0) {
                    test_one(trans, m, n, 1.0, 1, 1);
                    continue;
                }
                for (int ai = 0; ai < 5; ai++)
                    test_one(trans, m, n, alphas[ai], 1, 1);
                test_one(trans, m, n, 1.0, 2, 1);
                test_one(trans, m, n, 1.0, 1, 2);
            }
    }

    /* LAPACK-pattern tests: DLAHQR (QR iteration in DGEES) calls DGEMV on
     * small submatrices with a LARGE leading dimension (the full matrix size),
     * incx=incy=1, alpha=1, contiguous. Exercise large-LDA + small-M/N. */
    long lap_m[] = {1,2,3,4,5,6,7,8,9,10,11,12,15,16};
    long lap_lda[] = {20, 32, 50, 64, 100};
    int nlm = sizeof(lap_m)/sizeof(lap_m[0]);
    int nld = sizeof(lap_lda)/sizeof(lap_lda[0]);
    for (int trans = 0; trans < 2; trans++) {
        for (int mi = 0; mi < nlm; mi++) {
            long m = lap_m[mi];
            for (int ni = 0; ni < nlm; ni++) {
                long n = lap_m[ni];
                for (int li = 0; li < nld; li++) {
                    long lda = lap_lda[li];
                    if (lda < m) continue;
                    test_one_lda(trans, m, n, lda, 1.0, 1, 1);
                    test_one_lda(trans, m, n, lda, -1.0, 1, 1);
                }
            }
        }
    }
    printf("\n=== %ld/%ld tests passed, %ld failed ===\n", tests - failures, tests, failures);
    return failures ? 1 : 0;
}
