/*
 * Comprehensive test for dgemv_n_lsx and dgemv_t_lsx kernels.
 * Tests patterns matching LAPACK DGEES usage: small matrices, alpha=-1, various strides.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Kernel prototypes - matching OpenBLAS internal calling convention */
typedef void (*gemv_kernel_t)(int m, int n, int unused, double alpha,
    double *a, int lda, double *x, int incx,
    double *y, int incy, double *buffer);

extern void dgemv_n_lsx(int m, int n, int unused, double alpha,
    double *a, int lda, double *x, int incx,
    double *y, int incy, double *buffer);
extern void dgemv_t_lsx(int m, int n, int unused, double alpha,
    double *a, int lda, double *x, int incx,
    double *y, int incy, double *buffer);

#define MAXN 32
#define MAXBUF (MAXN * MAXN + 256)

static double A_ref[MAXN * MAXN];
static double x_ref[MAXN * 4];
static double y_ref[MAXN * 4];
static double y_ker[MAXN * 4];
static double buffer[MAXBUF];

static int tests_run = 0;
static int tests_failed = 0;

/* Reference implementation: y = alpha * op(A) * x + y
 * Mimics exactly what the kernel should do (after gemv.c handles beta scaling and pointer adjustment) */
static void dgemv_ref(int trans, int m, int n, double alpha,
    double *a, int lda, double *x, int incx,
    double *y, int incy)
{
    int i, j;

    if (trans == 0) {
        /* y = alpha * A * x + y, A is m x n (column-major) */
        for (j = 0; j < n; j++) {
            double xj = x[j * incx];
            double ax = alpha * xj;
            for (i = 0; i < m; i++) {
                y[i * incy] += ax * a[j * lda + i];
            }
        }
    } else {
        /* y = alpha * A^T * x + y, A is m x n (column-major) */
        for (j = 0; j < n; j++) {
            double sum = 0.0;
            for (i = 0; i < m; i++) {
                sum += a[j * lda + i] * x[i * incx];
            }
            y[j * incy] += alpha * sum;
        }
    }
}

static int arrays_equal(double *a, double *b, int n, double tol)
{
    int i;
    for (i = 0; i < n; i++) {
        double diff = fabs(a[i] - b[i]);
        if (diff > tol) {
            fprintf(stderr, "  MISMATCH at index %d: ref=%.17g ker=%.17g diff=%.17g\n",
                i, a[i], b[i], diff);
            return 0;
        }
    }
    return 1;
}

static void test_case(int trans, int m, int n, double alpha,
    int lda, int incx, int incy)
{
    int i, j;
    int lenx, leny;
    int xcnt, ycnt;
    double *x_ptr, *yref_ptr, *yker_ptr;
    double tol = 1e-10;

    tests_run++;

    /* Fill matrix with structured data */
    for (j = 0; j < n; j++) {
        for (i = 0; i < m; i++) {
            /* Use values that exercise different magnitudes */
            A_ref[j * lda + i] = (double)((i + 1) * (j + 1)) / 3.0 + (i == j ? 10.0 : 0.0);
        }
    }

    /* Fill x with structured data */
    for (i = 0; i < m + n + 2; i++) {
        x_ref[i] = (double)(i + 1) / 7.0 + 0.5;
    }

    /* Fill y with structured data (beta = 1, so y has initial values) */
    for (i = 0; i < m + n + 2; i++) {
        y_ref[i] = (double)(i + 2) / 11.0;
        y_ker[i] = y_ref[i];
    }

    /* Determine logical lengths */
    if (trans == 0) { lenx = n; leny = m; }
    else { lenx = m; leny = n; }

    /* Mimic gemv.c: adjust pointers for negative strides */
    x_ptr = x_ref;
    yref_ptr = y_ref;
    yker_ptr = y_ker;

    if (incx < 0) x_ptr = x_ref + (lenx - 1) * (-incx);
    if (incy < 0) {
        yref_ptr = y_ref + (leny - 1) * (-incy);
        yker_ptr = y_ker + (leny - 1) * (-incy);
    }

    /* Compute reference */
    dgemv_ref(trans, m, n, alpha, A_ref, lda, x_ptr, incx, yref_ptr, incy);

    /* Compute kernel */
    gemv_kernel_t kernel = trans ? dgemv_t_lsx : dgemv_n_lsx;
    kernel(m, n, 0, alpha, A_ref, lda, x_ptr, incx, yker_ptr, incy, buffer);

    /* Compare */
    if (!arrays_equal(yref_ptr, yker_ptr, leny, tol)) {
        fprintf(stderr, "FAILED: trans=%d M=%d N=%d alpha=%.3g LDA=%d INCX=%d INCY=%d\n",
            trans, m, n, alpha, lda, incx, incy);
        tests_failed++;
    }
}

int main(void)
{
    int trans, m, n, lda, incx, incy;
    double alphas[] = {1.0, -1.0, 0.5, 0.0, 2.0, 3.14159};
    int n_alphas = sizeof(alphas) / sizeof(alphas[0]);
    int strides[] = {1, 2, -1, -2};
    int n_strides = sizeof(strides) / sizeof(strides[0]);

    printf("=== Comprehensive DGEMV LSX Kernel Test ===\n");

    /* Test all combinations for small sizes (matching LAPACK DGEES patterns) */
    for (trans = 0; trans <= 1; trans++) {
        for (m = 0; m <= 25; m++) {
            for (n = 0; n <= 25; n++) {
                for (lda = m; lda <= m + 5; lda += (m == 0 ? 1 : 5)) {
                    /* For LDA, test both LDA=M and LDA=M+5 */
                    if (m == 0 && lda == 0) lda = 1; /* LDA must be >= 1 */
                    int actual_lda = (lda < 1) ? 1 : lda;
                    
                    for (incx = 0; incx < n_strides; incx++) {
                        for (incy = 0; incy < n_strides; incy++) {
                            int sx = strides[incx];
                            int sy = strides[incy];
                            int a;
                            for (a = 0; a < n_alphas; a++) {
                                test_case(trans, m, n, alphas[a], actual_lda, sx, sy);
                            }
                        }
                    }
                    if (m == 0) break; /* Only test LDA=1 for m=0 */
                }
            }
        }
    }

    /* Additional specific tests that mimic LAPACK DGEHRD/DLATRD patterns */
    printf("\n=== LAPACK-specific patterns ===\n");

    /* DLATRD pattern: DGEMV with TRANS='T', small submatrix */
    for (m = 1; m <= 20; m++) {
        for (n = 1; n <= 4; n++) {
            test_case(1, m, n, -1.0, m, 1, 1);
            test_case(1, m, n, 1.0, m, 1, 1);
        }
    }

    /* DLATRD pattern: DGEMV with TRANS='N', small submatrix */
    for (m = 1; m <= 4; m++) {
        for (n = 1; n <= 20; n++) {
            test_case(0, m, n, -1.0, m + 1, 1, 1);
            test_case(0, m, n, 1.0, m + 1, 1, 1);
        }
    }

    /* Test with larger LDA (common in LAPACK where LDA = N > M) */
    for (m = 1; m <= 15; m++) {
        for (n = 1; n <= 15; n++) {
            test_case(0, m, n, -1.0, n + 4, 1, 1);
            test_case(1, m, n, -1.0, n + 4, 1, 1);
        }
    }

    /* Test edge cases: M or N = 1 */
    for (m = 1; m <= 10; m++) {
        test_case(0, m, 1, -1.0, m, 1, 1);
        test_case(1, m, 1, -1.0, m, 1, 1);
        test_case(0, 1, m, -1.0, 1, 1, 1);
        test_case(1, 1, m, -1.0, 1, 1, 1);
    }

    /* Test with denormal-like small values */
    {
        int i, j;
        /* Fill with very small values */
        for (i = 0; i < MAXN * MAXN; i++) A_ref[i] = 1e-300;
        for (i = 0; i < MAXN * 4; i++) { x_ref[i] = 1e-300; y_ref[i] = 0; y_ker[i] = 0; }
        
        for (m = 1; m <= 8; m++) {
            for (n = 1; n <= 8; n++) {
                test_case(0, m, n, 1e-300, m, 1, 1);
                test_case(1, m, n, 1e-300, m, 1, 1);
            }
        }
    }

    /* Test with mixed positive/negative values */
    {
        for (m = 1; m <= 15; m++) {
            for (n = 1; n <= 15; n++) {
                int i, j;
                for (j = 0; j < n; j++)
                    for (i = 0; i < m; i++)
                        A_ref[j * m + i] = ((i + j) % 2 ? -1.0 : 1.0) * (double)(i + j + 1);
                for (i = 0; i < m + n; i++) { x_ref[i] = (i % 2 ? -1.0 : 1.0) * (i + 1); y_ref[i] = 0; y_ker[i] = 0; }
                
                test_case(0, m, n, -1.0, m, 1, 1);
                test_case(1, m, n, -1.0, m, 1, 1);
            }
        }
    }

    printf("\n=== Results ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_run - tests_failed);
    printf("Tests failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    } else {
        printf("\n%d TESTS FAILED\n", tests_failed);
        return 1;
    }
}
