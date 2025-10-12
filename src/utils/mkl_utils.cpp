#include "mkl_utils.h"

#ifdef USE_ONEMKL
#include <mkl.h>
#include <mkl_vml.h>
#endif

namespace mkl_utils {

#ifdef USE_ONEMKL

/**
 * @brief Compute exponential function for a vector of values using oneMKL VML
 *
 * Uses vdExp function from Intel MKL Vector Math Library.
 * VML functions provide significant performance improvement for batch operations.
 *
 * @param n Number of elements to process
 * @param a Input array of double precision values
 * @param y Output array for exponential results
 */
void vector_exp(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    // Can be changed to VML_LA (low accuracy) or VML_EP (enhanced performance) if needed
    vdExp(n, a, y);
}

/**
 * @brief Compute square root for a vector of values using oneMKL VML
 *
 * Uses vdSqrt function from Intel MKL Vector Math Library.
 * Provides vectorized square root computation with significant speedup for large arrays.
 *
 * @param n Number of elements to process
 * @param a Input array of double precision values (must be non-negative)
 * @param y Output array for square root results
 */
void vector_sqrt(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdSqrt(n, a, y);
}

/**
 * @brief Compute arctangent of y/x for vectors using oneMKL VML
 *
 * Uses vdAtan2 function from Intel MKL Vector Math Library.
 * Computes element-wise atan2(a[i], b[i]) for i in [0, n).
 * This is particularly useful for angle calculations in geometry.
 *
 * @param n Number of elements to process
 * @param a Array of Y values (numerator)
 * @param b Array of X values (denominator)
 * @param y Output array for atan2 results (in radians, range [-π, π])
 */
void vector_atan2(int n, const double* a, const double* b, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdAtan2(n, a, b, y);
}

/**
 * @brief Compute natural logarithm for a vector of values using oneMKL VML
 *
 * Uses vdLn function from Intel MKL Vector Math Library.
 * Provides vectorized logarithm computation with significant speedup for large arrays.
 *
 * @param n Number of elements to process
 * @param a Input array of double precision values (must be positive)
 * @param y Output array for logarithm results
 */
void vector_log(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdLn(n, a, y);
}

/**
 * @brief Compute sine for a vector of values using oneMKL VML
 *
 * Uses vdSin function from Intel MKL Vector Math Library.
 * Provides vectorized sine computation for batch angle processing.
 *
 * @param n Number of elements to process
 * @param a Input array of angles in radians
 * @param y Output array for sine results
 */
void vector_sin(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdSin(n, a, y);
}

/**
 * @brief Compute cosine for a vector of values using oneMKL VML
 *
 * Uses vdCos function from Intel MKL Vector Math Library.
 * Provides vectorized cosine computation for batch angle processing.
 *
 * @param n Number of elements to process
 * @param a Input array of angles in radians
 * @param y Output array for cosine results
 */
void vector_cos(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdCos(n, a, y);
}

/**
 * @brief Compute tangent for a vector of values using oneMKL VML
 *
 * Uses vdTan function from Intel MKL Vector Math Library.
 * Provides vectorized tangent computation for batch angle processing.
 *
 * @param n Number of elements to process
 * @param a Input array of angles in radians
 * @param y Output array for tangent results
 */
void vector_tan(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdTan(n, a, y);
}

/**
 * @brief Compute absolute value for a vector of values using oneMKL VML
 *
 * Uses vdAbs function from Intel MKL Vector Math Library.
 * Provides vectorized absolute value computation for batch processing.
 *
 * @param n Number of elements to process
 * @param a Input array of double precision values
 * @param y Output array for absolute value results
 */
void vector_abs(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdAbs(n, a, y);
}

/**
 * @brief Compute square (x^2) for a vector of values using oneMKL VML
 *
 * Uses vdSqr function from Intel MKL Vector Math Library.
 * Provides vectorized squaring computation for batch processing.
 * This is more efficient than manual multiplication in loops.
 *
 * @param n Number of elements to process
 * @param a Input array of double precision values
 * @param y Output array where y[i] = a[i]^2
 */
void vector_sqr(int n, const double* a, double* y) {
    // Use high accuracy mode (VML_HA) by default
    vdSqr(n, a, y);
}

/**
 * @brief Compute dot product of two vectors using oneMKL BLAS
 *
 * Uses cblas_ddot function from Intel MKL BLAS Level 1.
 * Provides highly optimized dot product computation using SIMD instructions.
 * This is the fundamental building block for many linear algebra operations.
 *
 * @param n Number of elements to process
 * @param a First input vector
 * @param b Second input vector
 * @return Dot product sum(a[i] * b[i])
 */
double vector_ddot(int n, const double* a, const double* b) {
    // cblas_ddot computes: sum(a[i] * b[i]) for i in [0, n)
    // incx=1, incy=1 means stride of 1 (contiguous arrays)
    return cblas_ddot(n, a, 1, b, 1);
}

#endif // USE_ONEMKL

} // namespace mkl_utils
