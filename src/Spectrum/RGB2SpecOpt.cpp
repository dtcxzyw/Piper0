/*
    SPDX-License-Identifier: GPL-3.0-or-later

    This file is part of Piper0, a physically based renderer.
    Copyright (C) 2022 Yingwei Zheng

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Based on "A Low-Dimensional Function Space for Efficient Spectral Upsampling", Comput. Graph. Forum 38(2): 147-155 (2019)
// By Wenzel Jakob and Johannes Hanika
// Please refer to https://rgl.epfl.ch/publications/Jakob2019Spectral and https://github.com/mitsuba-renderer/rgb2spec

#include <Piper/Core/Report.hpp>
#include <Piper/Render/Spectrum.hpp>
#include <fstream>

PIPER_NAMESPACE_BEGIN

constexpr uint32_t numberOfCoefficients = 3;

struct RGB2SpecTable final {
    uint32_t res = 0;
    std::pmr::vector<Float> scale;
    std::pmr::vector<Float> data;

    static fs::path path() {
        return fs::current_path() / "rgb2spec.data";
    }

    void load() {
        std::ifstream in{ path(), std::ios::in | std::ios::binary };
        if(!in)
            fatal("Failed to load rgb2spec table");

        if(char header[4]; !in.read(header, 4) || memcmp(header, "SPEC", 4) != 0)
            fatal("Invalid rgb2spec table");

        if(!in.read(reinterpret_cast<char*>(&res), sizeof(uint32_t)))
            fatal("Invalid rgb2spec table");

        scale.resize(res);
        data.resize(3ULL * res * res * res * numberOfCoefficients);

        if(!in.read(reinterpret_cast<char*>(scale.data()), static_cast<std::streamsize>(scale.size() * sizeof(Float))) ||
           !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(Float))))
            fatal("Invalid rgb2spec table");
    }

    void save() const {
        std::ofstream out{ path(), std::ios::out | std::ios::binary };
        out.write("SPEC", 4);
        out.write(reinterpret_cast<const char*>(&res), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(scale.data()), static_cast<std::streamsize>(scale.size() * sizeof(Float)));
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(Float)));
        out.flush();
        if(!out)
            throw std::runtime_error{ "Failed to save rgb2spec table" };
    }
};

PIPER_NAMESPACE_END

#ifndef PIPER_RGB2SPEC_OPT_STANDALONE
PIPER_NAMESPACE_BEGIN
namespace Impl {

    struct RGB2SpecTableLoader final {
        RGB2SpecTable table;

        RGB2SpecTableLoader() {
            table.load();
        }
    };

    static const RGB2SpecTable& getRGB2SpecTable() {
        static RGB2SpecTableLoader inst;
        return inst.table;
    }

    static uint32_t rgb2SpecFindInterval(const float* values, const uint32_t len, const Float x) {
        const auto lastInterval = len - 2;
        uint32_t left = 0, size = lastInterval;

        while(size > 0) {
            const auto half = size >> 1;
            // ReSharper disable once CppTooWideScopeInitStatement
            const auto middle = left + half + 1;

            if(values[middle] <= x) {
                left = middle;
                size -= half + 1;
            } else {
                size = half;
            }
        }

        return std::min(left, lastInterval);
    }

    static void rgb2SpecFetch(glm::vec3 rgb, Float out[numberOfCoefficients]) noexcept {
        const auto& [res, scaleLUT, data] = getRGB2SpecTable();
        /* Determine largest RGB component */
        rgb = glm::clamp(rgb, glm::vec3{ 0.0f }, glm::vec3{ 1.0f });

        int32_t maxComponent = 0;
        for(int32_t j = 1; j < 3; ++j)
            if(rgb[j] >= rgb[maxComponent])
                maxComponent = j;

        const auto z = rgb[maxComponent];
        const auto scale = static_cast<Float>(res - 1U) / z;
        const auto y = rgb[(maxComponent + 2) % 3] * scale;
        const auto x = rgb[(maxComponent + 1) % 3] * scale;

        const auto xi = std::min(static_cast<uint32_t>(x), res - 2);
        const auto yi = std::min(static_cast<uint32_t>(y), res - 2);
        const auto zi = rgb2SpecFindInterval(scaleLUT.data(), res, z);
        /* Trilinearly interpolated lookup */
        auto offset = (((maxComponent * res + zi) * res + yi) * res + xi) * numberOfCoefficients;
        const auto dz = numberOfCoefficients * res * res;
        const auto dy = numberOfCoefficients * res;
        constexpr auto dx = numberOfCoefficients;

        const auto x1 = x - static_cast<Float>(xi);
        const auto x0 = 1.0f - x1;
        const auto y1 = y - static_cast<Float>(yi);
        const auto y0 = 1.0f - y1;
        const auto z1 = (z - scaleLUT[zi]) / (scaleLUT[zi + 1] - scaleLUT[zi]);
        const auto z0 = 1.0f - z1;

        for(uint32_t j = 0; j < numberOfCoefficients; ++j, ++offset) {
            out[j] =
                ((data[offset] * x0 + data[offset + dx] * x1) * y0 + (data[offset + dy] * x0 + data[offset + dy + dx] * x1) * y1) * z0 +
                ((data[offset + dz] * x0 + data[offset + dz + dx] * x1) * y0 +
                 (data[offset + dz + dy] * x0 + data[offset + dz + dy + dx] * x1) * y1) *
                    z1;
        }
    }

    static Float rgb2SpecEval(const Float coeff[numberOfCoefficients], const Float lambda) {
        const Float x = std::fma(std::fma(coeff[0], lambda, coeff[1]), lambda, coeff[2]);
        const Float y = 1.0f / sqrtf(std::fma(x, x, 1.0f));
        return std::fma(0.5f * x, y, 0.5f);
    }

    Float fromRGB(const RGBSpectrum& u, const Float wavelength) noexcept {
        Float coefficients[numberOfCoefficients];
        rgb2SpecFetch(u.raw(), coefficients);
        return rgb2SpecEval(coefficients, wavelength);
    }

    MonoWavelengthSpectrum fromRGB(const RGBSpectrum& u, const MonoWavelengthSpectrum& w) noexcept {
        Float coefficients[numberOfCoefficients];
        rgb2SpecFetch(u.raw(), coefficients);

        return MonoWavelengthSpectrum::fromRaw(rgb2SpecEval(coefficients, w.raw()));
    }

    SampledSpectrum fromRGB(const RGBSpectrum& u, const SampledSpectrum& w) noexcept {
        Float coefficients[numberOfCoefficients];
        rgb2SpecFetch(u.raw(), coefficients);

        const auto lambdas = w.raw();
        SampledSpectrum::VecType res;

        for(int32_t idx = 0; idx < SampledSpectrum::nSamples; ++idx)
            res[idx] = rgb2SpecEval(coefficients, lambdas[idx]);

        return SampledSpectrum::fromRaw(res);
    }
}  // namespace Impl
PIPER_NAMESPACE_END
#else
#include <Piper/Render/ColorMatchingFunction.hpp>
#include <Piper/Render/StandardIlluminant.hpp>
#include <charconv>
#include <iostream>
#include <oneapi/tbb/parallel_for.h>

using namespace Piper;
constexpr double optEpslion = 1e-4;

inline double sigmoid(const double x) noexcept {
    return 0.5 * x / std::sqrt(1.0 + x * x) + 0.5;
}

constexpr double smoothstep(const double x) noexcept {
    return x * x * (3.0 - 2.0 * x);
}

// LU decomposition & triangular solving code lifted from Wikipedia

/* INPUT: A - array of pointers to rows of a square matrix having dimension N
 *        Tol - small tolerance number to detect failure when the matrix is near degenerate
 * OUTPUT: Matrix A is changed, it contains both matrices L-E and U as A=(L-E)+U such that P*A=L*U.
 *        The permutation matrix is not stored as a matrix, but in an integer vector P of size N+1
 *        containing column indexes where the permutation matrix has "1". The last element P[N]=S+N,
 *        where S is the number of row exchanges needed for determinant computation, det(P)=(-1)^S
 */

bool evalLUPDecompose(double** mat, const uint32_t n, const double tol, uint32_t* p) noexcept {
    for(uint32_t i = 0; i <= n; ++i)
        p[i] = i;  // Unit permutation matrix, P[N] initialized with N

    for(uint32_t i = 0; i < n; ++i) {
        uint32_t iMax = i;
        double maxA = 0.0;

        for(uint32_t k = i; k < n; ++k)
            if(const auto absA = std::fabs(mat[k][i]); absA > maxA) {
                maxA = absA;
                iMax = k;
            }

        if(maxA < tol)
            return false;  // failure, matrix is degenerate

        if(iMax != i) {
            // pivoting P
            std::swap(p[i], p[iMax]);

            // pivoting rows of A
            std::swap(mat[i], mat[iMax]);

            // counting pivots starting from N (for determinant)
            ++p[n];
        }

        for(uint32_t j = i + 1; j < n; ++j) {
            mat[j][i] /= mat[i][i];

            for(uint32_t k = i + 1; k < n; ++k)
                mat[j][k] -= mat[j][i] * mat[i][k];
        }
    }

    return true;  // decomposition done
}

/* INPUT: A,P filled in LUPDecompose; b - rhs vector; N - dimension
 * OUTPUT: x - solution vector of A*x=b
 */
void solveLUP(double** mat, const uint32_t* p, const double* b, const uint32_t n, double* x) noexcept {
    for(uint32_t i = 0; i < n; ++i) {
        x[i] = b[p[i]];

        for(uint32_t k = 0; k < i; ++k)
            x[i] -= mat[i][k] * x[k];
    }

    for(int32_t i = n - 1; i >= 0; --i) {
        for(int k = i + 1; k < n; k++)
            x[i] -= mat[i][k] * x[k];

        x[i] = x[i] / mat[i][i];
    }
}

constexpr double xyz2rgb[3][3] = { { 3.240479, -1.537150, -0.498535 },
                                   { -0.969256, 1.875991, 0.041556 },
                                   { 0.055648, -0.204043, 1.057311 } };

constexpr double rgb2xyz[3][3] = { { 0.412453, 0.357580, 0.180423 }, { 0.212671, 0.715160, 0.072169 }, { 0.019334, 0.119193, 0.950227 } };

double rgbTable[3][spectralLUTSize], xyzWhitePoint[3];

static void initTables() noexcept {
    memset(rgbTable, 0, sizeof(rgbTable));
    memset(xyzWhitePoint, 0, sizeof(xyzWhitePoint));

    for(uint32_t i = 0; i < spectralLUTSize; ++i) {
        const auto lambda = static_cast<double>(wavelengthMin + i);

        const auto xyz = wavelength2XYZ(lambda);
        const auto illuminant = wavelength2IlluminantD65(lambda);

        double weight = 3.0 / 8.0;
        if(i == 0 || i == spectralLUTSize - 1)
            ;
        else if((i - 1) % 3 == 2)
            weight *= 2.0f;
        else
            weight *= 3.0f;

        for(uint32_t k = 0; k < 3; ++k)
            for(uint32_t j = 0; j < 3; ++j)
                rgbTable[k][i] += xyz2rgb[k][j] * xyz[j] * illuminant * weight;

        for(uint32_t i = 0; i < 3; ++i)
            xyzWhitePoint[i] += xyz[i] * illuminant * weight;
    }
}

static void evalCIELab(double* p) noexcept {
    double x = 0.0, y = 0.0, z = 0.0;

    for(uint32_t j = 0; j < 3; ++j) {
        x += p[j] * rgb2xyz[0][j];
        y += p[j] * rgb2xyz[1][j];
        z += p[j] * rgb2xyz[2][j];
    }

    auto f = [](const double t) {
        constexpr double delta = 6.0 / 29.0;
        if(t > delta * delta * delta)
            return std::cbrt(t);
        else
            return t / (delta * delta * 3.0) + (4.0 / 29.0);
    };

    const auto [xw, yw, zw] = xyzWhitePoint;
    p[0] = 116.0 * f(y / yw) - 16.0;
    p[1] = 500.0 * (f(x / xw) - f(y / yw));
    p[2] = 200.0 * (f(y / yw) - f(z / zw));
}

static void evalResidual(const double* coeffs, const double* rgb, double* residual) noexcept {
    double out[3] = { 0.0, 0.0, 0.0 };

    for(uint32_t i = 0; i < spectralLUTSize; ++i) {
        /* Scale lambda to 0..1 range */
        const auto lambda = i / static_cast<double>(wavelengthMax - wavelengthMin);

        /* Polynomial */
        double x = 0.0;
        for(uint32_t i = 0; i < 3; ++i)
            x = std::fma(x, lambda, coeffs[i]);

        /* Sigmoid */
        const auto s = sigmoid(x);

        /* Integrate against precomputed curves */
        for(uint32_t j = 0; j < 3; ++j)
            out[j] += rgbTable[j][i] * s;
    }
    evalCIELab(out);
    memcpy(residual, rgb, sizeof(double) * 3);
    evalCIELab(residual);

    for(uint32_t j = 0; j < 3; ++j)
        residual[j] -= out[j];
}

static void evalJacobian(const double* coeffs, const double* rgb, double** jac) noexcept {
    double r0[3], r1[3], tmp[3];

    for(uint32_t i = 0; i < 3; ++i) {
        memcpy(tmp, coeffs, sizeof(double) * 3);
        tmp[i] -= optEpslion;
        evalResidual(tmp, rgb, r0);

        memcpy(tmp, coeffs, sizeof(double) * 3);
        tmp[i] += optEpslion;
        evalResidual(tmp, rgb, r1);

        for(uint32_t j = 0; j < 3; ++j)
            jac[j][i] = (r1[j] - r0[j]) / (2.0 * optEpslion);
    }
}

static double gaussNewton(const double rgb[3], double coeffs[3], uint32_t iteration) {
    double r = 0.0;
    for(uint32_t i = 0; i < iteration; ++i) {
        double J0[3], J1[3], J2[3], *J[3] = { J0, J1, J2 }, residual[3];

        evalResidual(coeffs, rgb, residual);
        evalJacobian(coeffs, rgb, J);

        uint32_t P[4];
        uint32_t rv = evalLUPDecompose(J, 3, 1e-15, P);
        if(rv != 1) {
            std::cout << "RGB " << rgb[0] << " " << rgb[1] << " " << rgb[2] << std::endl;
            std::cout << "-> " << coeffs[0] << " " << coeffs[1] << " " << coeffs[2] << std::endl;
            throw std::runtime_error("LU decomposition failed!");
        }

        double x[3];
        solveLUP(J, P, residual, 3, x);

        r = 0.0;
        for(uint32_t j = 0; j < 3; ++j) {
            coeffs[j] -= x[j];
            r += residual[j] * residual[j];
        }
        const auto max = std::fmax(std::fmax(coeffs[0], coeffs[1]), coeffs[2]);

        if(max > 200.0) {
            for(uint32_t j = 0; j < 3; ++j)
                coeffs[j] *= 200.0 / max;
        }

        if(r < 1e-6)
            break;
    }
    return std::sqrt(r);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cerr << "Usage:  PiperRGB2SpecOpt <resolution>" << std::endl;
        return EXIT_FAILURE;
    }

    RGB2SpecTable table;
    auto& [res, scale, data] = table;
    std::pmr::vector<double> errors;

    initTables();

    const std::string_view resStr{ argv[1] };
    std::from_chars(resStr.data(), resStr.data() + resStr.size(), res);
    if(res == 0 || res > 256) {
        std::cerr << "Invalid resolution!" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Optimizing spectra" << std::flush;

    scale.resize(res);
    for(int k = 0; k < res; ++k)
        scale[k] = static_cast<Float>(smoothstep(smoothstep(k / static_cast<double>(res - 1))));

    data.resize(3ULL * res * res * res * numberOfCoefficients);
    errors.resize(3ULL * res * res * res);

    for(uint32_t t = 0; t < 3; ++t) {
        tbb::parallel_for(tbb::blocked_range<uint32_t>(0, res), [&](const tbb::blocked_range<uint32_t>& r) {
            for(size_t j = r.begin(); j != r.end(); ++j) {
                const auto y = j / static_cast<double>(res - 1);
                std::cout << "." << std::flush;

                for(uint32_t i = 0; i < res; ++i) {
                    const double x = i / static_cast<double>(res - 1);
                    double coeffs[3], rgb[3];
                    memset(coeffs, 0, sizeof(double) * 3);

                    const auto eval = [&](const uint32_t k) {
                        const auto v = smoothstep(smoothstep(k / static_cast<double>(res - 1)));
                        rgb[t] = v;
                        rgb[(t + 1) % 3] = x * v;
                        rgb[(t + 2) % 3] = y * v;

                        const auto error = gaussNewton(rgb, coeffs, 30);

                        const auto c0 = static_cast<double>(wavelengthMin), c1 = 1.0 / static_cast<double>(wavelengthMax - wavelengthMin);
                        const auto [a, b, c] = coeffs;

                        const auto idx = ((t * res + k) * res + j) * res + i;

                        data[3 * idx + 0] = static_cast<Float>(a * sqr(c1));
                        data[3 * idx + 1] = static_cast<Float>(b * c1 - 2.0 * a * c0 * sqr(c1));
                        data[3 * idx + 2] = static_cast<Float>(c - b * c0 * c1 + a * sqr(c0 * c1));
                        errors[idx] = error;
                    };

                    const auto start = res / 5;
                    for(uint32_t k = start; k < res; ++k)
                        eval(k);

                    memset(coeffs, 0, sizeof(double) * 3);
                    for(int32_t k = start; k >= 0; --k)
                        eval(k);
                }
            }
        });
    }

    table.save();

    std::cout << " done." << std::endl;
    std::sort(errors.begin(), errors.end());

    std::cout << "Residual error (99%th) = " << errors[static_cast<size_t>(static_cast<double>(errors.size() - 1) * 0.99)] << std::endl;
    return EXIT_SUCCESS;
}

#endif
