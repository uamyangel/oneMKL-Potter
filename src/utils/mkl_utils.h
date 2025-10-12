#pragma once

#include <cmath>
#include <vector>

/**
 * @file mkl_utils.h
 * @brief Utility functions for Intel oneMKL Vector Math Library (VML) optimization
 *
 * This module provides wrappers around oneMKL VML functions for high-performance
 * mathematical computations. When USE_ONEMKL is defined, it uses optimized MKL
 * functions; otherwise, it falls back to standard C++ math functions.
 */

namespace mkl_utils {

#ifdef USE_ONEMKL

/**
 * @brief Compute exponential function for a vector of values
 *
 * @param n Number of elements
 * @param a Input array
 * @param y Output array (can be same as input for in-place operation)
 */
void vector_exp(int n, const double* a, double* y);

/**
 * @brief Compute square root for a vector of values
 *
 * @param n Number of elements
 * @param a Input array
 * @param y Output array (can be same as input for in-place operation)
 */
void vector_sqrt(int n, const double* a, double* y);

/**
 * @brief Compute arctangent of y/x for vectors
 *
 * @param n Number of elements
 * @param a Y values array
 * @param b X values array
 * @param y Output array for atan2(a[i], b[i])
 */
void vector_atan2(int n, const double* a, const double* b, double* y);

/**
 * @brief Compute natural logarithm for a vector of values
 *
 * @param n Number of elements
 * @param a Input array
 * @param y Output array (can be same as input for in-place operation)
 */
void vector_log(int n, const double* a, double* y);

/**
 * @brief Compute sine for a vector of values
 *
 * @param n Number of elements
 * @param a Input array (angles in radians)
 * @param y Output array
 */
void vector_sin(int n, const double* a, double* y);

/**
 * @brief Compute cosine for a vector of values
 *
 * @param n Number of elements
 * @param a Input array (angles in radians)
 * @param y Output array
 */
void vector_cos(int n, const double* a, double* y);

/**
 * @brief Compute tangent for a vector of values
 *
 * @param n Number of elements
 * @param a Input array (angles in radians)
 * @param y Output array
 */
void vector_tan(int n, const double* a, double* y);

/**
 * @brief Compute absolute value for a vector of values
 *
 * @param n Number of elements
 * @param a Input array
 * @param y Output array (can be same as input for in-place operation)
 */
void vector_abs(int n, const double* a, double* y);

/**
 * @brief Compute square (x^2) for a vector of values
 *
 * @param n Number of elements
 * @param a Input array
 * @param y Output array where y[i] = a[i]^2
 */
void vector_sqr(int n, const double* a, double* y);

/**
 * @brief Compute dot product of two vectors (BLAS ddot)
 *
 * @param n Number of elements
 * @param a First input vector
 * @param b Second input vector
 * @return Dot product sum(a[i] * b[i])
 */
double vector_ddot(int n, const double* a, const double* b);

/**
 * @brief Compute exponential function for a single value
 * Optimized for single-value calls using VML when beneficial
 */
inline double scalar_exp(double x) {
    // For single values, direct call is often faster
    return std::exp(x);
}

/**
 * @brief Compute square root for a single value
 */
inline double scalar_sqrt(double x) {
    return std::sqrt(x);
}

/**
 * @brief Compute arctangent of y/x for a single value
 */
inline double scalar_atan2(double y, double x) {
    return std::atan2(y, x);
}

/**
 * @brief Compute natural logarithm for a single value
 */
inline double scalar_log(double x) {
    return std::log(x);
}

/**
 * @brief Compute sine for a single value
 */
inline double scalar_sin(double x) {
    return std::sin(x);
}

/**
 * @brief Compute cosine for a single value
 */
inline double scalar_cos(double x) {
    return std::cos(x);
}

/**
 * @brief Compute tangent for a single value
 */
inline double scalar_tan(double x) {
    return std::tan(x);
}

/**
 * @brief Compute absolute value for a single value
 */
inline double scalar_abs(double x) {
    return std::abs(x);
}

/**
 * @brief Compute absolute value for a single integer value
 */
inline int scalar_abs(int x) {
    return std::abs(x);
}

/**
 * @brief Compute absolute value for a single value (floating point)
 */
inline double scalar_fabs(double x) {
    return std::fabs(x);
}

/**
 * @brief Compute square for a single value
 */
inline double scalar_sqr(double x) {
    return x * x;
}

// Convenience overloads for std::vector
inline void vector_exp(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_exp(a.size(), a.data(), y.data());
}

inline void vector_sqrt(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_sqrt(a.size(), a.data(), y.data());
}

inline void vector_atan2(const std::vector<double>& a, const std::vector<double>& b, std::vector<double>& y) {
    y.resize(a.size());
    vector_atan2(a.size(), a.data(), b.data(), y.data());
}

inline void vector_log(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_log(a.size(), a.data(), y.data());
}

inline void vector_sin(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_sin(a.size(), a.data(), y.data());
}

inline void vector_cos(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_cos(a.size(), a.data(), y.data());
}

inline void vector_tan(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_tan(a.size(), a.data(), y.data());
}

inline void vector_abs(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_abs(a.size(), a.data(), y.data());
}

inline void vector_sqr(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    vector_sqr(a.size(), a.data(), y.data());
}

inline double vector_ddot(const std::vector<double>& a, const std::vector<double>& b) {
    return vector_ddot(a.size(), a.data(), b.data());
}

#else
// Fallback implementations using standard C++ library
// These will be used when oneMKL is not available

inline void vector_exp(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::exp(a[i]);
    }
}

inline void vector_sqrt(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::sqrt(a[i]);
    }
}

inline void vector_atan2(int n, const double* a, const double* b, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::atan2(a[i], b[i]);
    }
}

inline void vector_log(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::log(a[i]);
    }
}

inline void vector_sin(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::sin(a[i]);
    }
}

inline void vector_cos(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::cos(a[i]);
    }
}

inline void vector_tan(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::tan(a[i]);
    }
}

inline void vector_abs(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = std::abs(a[i]);
    }
}

inline void vector_sqr(int n, const double* a, double* y) {
    for (int i = 0; i < n; i++) {
        y[i] = a[i] * a[i];
    }
}

inline double vector_ddot(int n, const double* a, const double* b) {
    double result = 0.0;
    for (int i = 0; i < n; i++) {
        result += a[i] * b[i];
    }
    return result;
}

inline double scalar_exp(double x) {
    return std::exp(x);
}

inline double scalar_sqrt(double x) {
    return std::sqrt(x);
}

inline double scalar_atan2(double y, double x) {
    return std::atan2(y, x);
}

inline double scalar_log(double x) {
    return std::log(x);
}

inline double scalar_sin(double x) {
    return std::sin(x);
}

inline double scalar_cos(double x) {
    return std::cos(x);
}

inline double scalar_tan(double x) {
    return std::tan(x);
}

inline double scalar_abs(double x) {
    return std::abs(x);
}

inline int scalar_abs(int x) {
    return std::abs(x);
}

inline double scalar_fabs(double x) {
    return std::fabs(x);
}

inline double scalar_sqr(double x) {
    return x * x;
}

// Convenience overloads for std::vector
inline void vector_exp(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::exp(a[i]);
    }
}

inline void vector_sqrt(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::sqrt(a[i]);
    }
}

inline void vector_atan2(const std::vector<double>& a, const std::vector<double>& b, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::atan2(a[i], b[i]);
    }
}

inline void vector_log(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::log(a[i]);
    }
}

inline void vector_sin(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::sin(a[i]);
    }
}

inline void vector_cos(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::cos(a[i]);
    }
}

inline void vector_tan(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::tan(a[i]);
    }
}

inline void vector_abs(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = std::abs(a[i]);
    }
}

inline void vector_sqr(const std::vector<double>& a, std::vector<double>& y) {
    y.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        y[i] = a[i] * a[i];
    }
}

inline double vector_ddot(const std::vector<double>& a, const std::vector<double>& b) {
    double result = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        result += a[i] * b[i];
    }
    return result;
}

#endif // USE_ONEMKL

} // namespace mkl_utils
