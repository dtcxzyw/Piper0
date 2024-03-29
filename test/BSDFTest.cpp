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

#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/TestUtil.hpp>
#include <fstream>
#include <oneapi/tbb/parallel_for.h>

PIPER_NAMESPACE_BEGIN

constexpr uint32_t resolution = 100;
constexpr double significanceLevel = 0.01;
constexpr uint32_t minExpFrequency = 5;

// sampled distribution
static std::pmr::vector<Float> evalFrequencyTable(SampleProvider& sampler, const Direction<FrameOfReference::World>& wo,
                                                  const BSDF<RSSMono>& bsdf, const uint32_t sampleCount) {
    std::pmr::vector<Float> res(2ULL * resolution * resolution);

    constexpr auto scaleFactor = resolution * invPi;
    uint32_t validCount = 0;

    for(uint32_t idx = 0; idx < sampleCount; ++idx) {
        const auto sample = bsdf.sample(sampler, wo);

        if(!sample.valid() || match(sample.part, BxDFPart::Specular))
            continue;

        ++validCount;

        auto coords = sample.wi.toSphericalCoord() * scaleFactor;

        if(coords.y < 0)
            coords.y += 2ULL * resolution;

        const auto thetaIdx = std::clamp(static_cast<int32_t>(coords.x), 0, static_cast<int32_t>(resolution - 1));
        const auto phiIdx = std::clamp(static_cast<int32_t>(coords.y), 0, static_cast<int32_t>(2U * resolution - 1));

        ++res[thetaIdx * (2ULL * resolution) + phiIdx];
    }
    return validCount > 100 ? res : std::pmr::vector<Float>{};
}

template <typename Callable>
static Float adaptiveSimpsonImpl(const Callable& f, const Float a, const Float b, const Float c, const Float fa, const Float fb,
                                 const Float fc, const Float val, const Float eps, const int32_t depth) {
    /* Evaluate the function at two intermediate points */
    const auto d = 0.5f * (a + b), e = 0.5f * (b + c), fd = f(d), fe = f(e);  // NOLINT(clang-diagnostic-shadow)

    /* Simpson integration over each subinterval */
    const auto h = c - a, val0 = static_cast<Float>(1.0 / 12.0) * h * (fa + 4 * fd + fb),
               val1 = static_cast<Float>(1.0 / 12.0) * h * (fb + 4 * fe + fc), valP = val0 + val1;

    /* Stopping criterion from J.N. Lyness (1969)
      "Notes on the adaptive Simpson quadrature routine" */
    if(depth <= 0 || std::fabs(valP - val) < 15 * eps) {
        // Richardson extrapolation
        return valP + static_cast<Float>(1.0 / 15.0) * (valP - val);
    }

    return adaptiveSimpsonImpl(f, a, d, b, fa, fd, fb, val0, 0.5f * eps, depth - 1) +
        adaptiveSimpsonImpl(f, b, e, c, fb, fe, fc, val1, 0.5f * eps, depth - 1);
}

template <typename Callable>
static Float adaptiveSimpson(const Callable& f, const Float x0, const Float x1, Float eps, int32_t depth) {
    const auto a = x0, b = 0.5f * (x0 + x1), c = x1;
    const auto fa = f(a), fb = f(b), fc = f(c);
    const auto val = (c - a) * static_cast<Float>(1.0 / 6.0) * (fa + 4 * fb + fc);
    return adaptiveSimpsonImpl(f, a, b, c, fa, fb, fc, val, eps, depth);
}

template <typename Callable>
static Float adaptiveSimpson2D(const Callable& f, const Float x0, const Float y0, const Float x1, const Float y1, const Float eps,
                               const int32_t depth) {
    return adaptiveSimpson([&](Float y) { return adaptiveSimpson([&f, y](Float x) { return f(x, y); }, x0, x1, eps, depth); }, y0, y1, eps,
                           depth);
}

// pdf
static std::pmr::vector<Float> evalExpectedFrequencyTable(const Direction<FrameOfReference::World>& wo, const BSDF<RSSMono>& bsdf,
                                                          const uint32_t sampleCount) {
    constexpr auto scaleFactor = pi / resolution;
    std::pmr::vector<Float> res(2ULL * resolution * resolution);

    for(uint32_t i = 0; i < resolution; ++i) {
        for(uint32_t j = 0; j < 2U * resolution; ++j) {
            res[i * (2ULL * resolution) + j] =
                static_cast<Float>(sampleCount) *
                adaptiveSimpson2D(
                    [&](const Float theta, const Float phi) -> Float {
                        const auto wi = Direction<FrameOfReference::World>::fromSphericalCoord({ theta, phi });
                        if(const auto inversePdf = bsdf.pdf(wo, wi); inversePdf.valid())
                            return rcp(inversePdf.raw()) * std::sin(theta);
                        return 0.0f;
                    },
                    static_cast<Float>(i) * scaleFactor, static_cast<Float>(j) * scaleFactor, static_cast<Float>(i + 1) * scaleFactor,
                    static_cast<Float>(j + 1) * scaleFactor, 1e-8f, 20);
        }
    }

    return res;
}

/// Regularized lower incomplete gamma function (based on code from Cephes)
static double evalRLGamma(const double a, const double x) {
    constexpr double maxError = 1e-15;
    // ReSharper disable once CppTooWideScopeInitStatement
    constexpr double big = 4503599627370496.0;
    // ReSharper disable once CppTooWideScope
    constexpr double bigInv = 2.22044604925031308085e-16;

    if(a < 0 || x < 0)
        throw std::runtime_error("LLGamma: invalid arguments range!");

    if(x <= 0.0)
        return 0.0;

    const double ax = a * std::log(x) - x - std::lgamma(a);  // NOLINT(concurrency-mt-unsafe)
    if(ax < -709.78271289338399)
        return a < x ? 1.0 : 0.0;

    if(x <= 1 || x <= a) {
        double r2 = a;
        double c2 = 1;
        double ans2 = 1;

        do {
            r2 = r2 + 1;
            c2 = c2 * x / r2;
            ans2 += c2;
        } while((c2 / ans2) > maxError);

        return std::exp(ax) * ans2 / a;
    }

    int32_t c = 0;
    double y = 1 - a;
    double z = x + y + 1;
    double p3 = 1;
    double q3 = x;
    double p2 = x + 1;
    double q2 = z * x;
    double ans = p2 / q2;
    double error;

    do {
        ++c;
        y += 1;
        z += 2;
        const double yc = y * c;
        const double p = (p2 * z) - (p3 * yc);
        const double q = (q2 * z) - (q3 * yc);

        if(std::fabs(q) > 1e-15) {
            const auto nextAns = p / q;
            error = std::abs((ans - nextAns) / nextAns);
            ans = nextAns;
        } else {
            // zero div, skip
            error = 1;
        }

        // shift
        p3 = p2;
        p2 = p;
        q3 = q2;
        q2 = q;

        // normalize fraction when the numerator becomes large
        if(std::abs(p) > big) {
            p3 *= bigInv;
            p2 *= bigInv;
            q3 *= bigInv;
            q2 *= bigInv;
        }
    } while(error > maxError);

    return 1.0 - std::exp(ax) * ans;
}

/// Chi^2 distribution cumulative distribution function
static double chi2Cdf(const double x, const uint32_t dof) {
    if(dof < 1 || x < 0)
        return 0.0;
    if(dof == 2)
        return 1.0 - std::exp(-0.5 * x);
    return evalRLGamma(0.5 * dof, 0.5 * x);
}

static std::pair<bool, std::string> chi2Test(const std::pmr::vector<Float>& sampled, const std::pmr::vector<Float>& expected,
                                             const uint32_t testCount, const uint32_t sampleCount) {
    /* Sort all cells by their expected frequencies */
    std::vector<std::pair<Float, uint32_t>> cells(sampled.size());
    for(uint32_t i = 0; i < cells.size(); ++i)
        cells[i] = { expected[i], i };
    std::ranges::sort(cells, [](const auto& a, const auto& b) { return a.first < b.first; });

    /* Compute the Chi^2 statistic and pool cells as necessary */
    Float pooledFrequencies = 0, pooledExpFrequencies = 0, x = 0;
    int32_t pooledCells = 0, dof = 0;

    for(const auto& [expectedP, idx] : cells) {
        if(expectedP <= 0.0f) {
            if(sampled[idx] > static_cast<Float>(sampleCount) * 1e-5f)
                return { false,
                         fmt::format("Encountered {} samples in a c with expected "
                                     "frequency 0. Rejecting the null hypothesis!",
                                     sampled[idx]) };
        } else if(expectedP < minExpFrequency || /* Pool cells with low expected frequencies */
                  (pooledExpFrequencies > 0 && pooledExpFrequencies < minExpFrequency)
                  /* Keep on pooling cells until a sufficiently high
                     expected frequency is achieved. */
        ) {
            pooledFrequencies += sampled[idx];
            pooledExpFrequencies += expectedP;
            ++pooledCells;
        } else {
            x += sqr(sampled[idx] - expectedP) / expectedP;
            ++dof;
        }
    }

    if(pooledExpFrequencies > 0 || pooledFrequencies > 0) {
        x += sqr(pooledFrequencies - pooledExpFrequencies) / pooledExpFrequencies;
        ++dof;
    }

    /* All parameters are assumed to be known, so there is no
       additional DF reduction due to model parameters */
    dof -= 1;

    if(dof <= 0)
        return { false, fmt::format("The number of degrees of freedom {} is too low!", dof) };

    /* Probability of obtaining a test statistic at least
       as extreme as the one observed under the assumption
       that the distributions match */
    const auto p = 1.0 - chi2Cdf(static_cast<double>(x), dof);

    /* Apply the Sidak correction term, since we'll be conducting multiple
       independent
       hypothesis tests. This accounts for the fact that the probability of a
       failure
       increases quickly when several hypothesis tests are run in sequence. */
    // ReSharper disable once CppTooWideScopeInitStatement
    const auto alpha = 1.0 - std::pow(1.0 - significanceLevel, 1.0 / testCount);

    if(std::isfinite(p) && p > alpha)
        return { true, {} };
    return { false,
             fmt::format("Rejected the null hypothesis (p-value = {}, "
                         "significance level = {})",
                         p, alpha) };
}

static void dumpTable(const std::string& path, const std::pmr::vector<Float>& sampled, const std::pmr::vector<Float>& expected) {
    std::ofstream out{ path };

    constexpr uint32_t thetaRes = resolution, phiRes = 2U * resolution;

    out << "frequencies = [ ";
    for(uint32_t i = 0; i < thetaRes; ++i) {
        for(uint32_t j = 0; j < phiRes; ++j) {
            out << sampled[i * phiRes + j];
            if(j + 1 < phiRes)
                out << ", ";
        }
        if(i + 1 < thetaRes)
            out << "; ";
    }
    out << " ];" << std::endl << "expFrequencies = [ ";
    for(uint32_t i = 0; i < thetaRes; ++i) {
        for(uint32_t j = 0; j < phiRes; ++j) {
            out << expected[i * phiRes + j];
            if(j + 1 < phiRes)
                out << ", ";
        }
        if(i + 1 < thetaRes)
            out << "; ";
    }
    out << " ];" << std::endl
        << "colormap(jet);" << std::endl
        << "clf; subplot(2,1,1);" << std::endl
        << "imagesc(frequencies);" << std::endl
        << "title('Observed frequencies');" << std::endl
        << "axis equal;" << std::endl
        << "subplot(2,1,2);" << std::endl
        << "imagesc(expFrequencies);" << std::endl
        << "axis equal;" << std::endl
        << "title('Expected frequencies');" << std::endl;
}

static void chi2Test(const std::string_view name, const BSDF<RSSMono>& bsdf) {
    constexpr uint32_t testCount = 20;
    constexpr uint32_t sampleCount = 1 << 20;
    auto& sampler = getTestSampler();

    for(uint32_t k = 0; k < testCount; ++k) {
        const auto wo = sampleCosineHemisphere<FrameOfReference::World>(sampler.sampleVec2());

        const auto freq = evalFrequencyTable(sampler, wo, bsdf, sampleCount);
        if(freq.empty()) {  // no valid sample, restart
            --k;
            continue;
        }

        const auto expectedFreq = evalExpectedFrequencyTable(wo, bsdf, sampleCount);
        const auto [res, reason] = chi2Test(freq, expectedFreq, testCount, sampleCount);

        if(!res)
            dumpTable(fmt::format("chi2test_{}_table_{}.m", name, k), freq, expectedFreq);

        ASSERT_TRUE(res) << " name " << name << " reason: " << reason << " iteration " << k;
    }
}

static void testEnergyConservation(const std::string_view name, const Normal<FrameOfReference::World>& normal, const BSDF<RSSMono>& bsdf,
                                   const bool specular) {
    constexpr uint32_t testCount = 64;
    constexpr uint32_t sampleCount = 1 << 20;
    auto& sampler = getTestSampler();

    for(uint32_t k = 0; k < testCount; ++k) {
        const auto wo = sampleUniformSphere<FrameOfReference::World>(sampler.sampleVec2());

        uint32_t count = 0;
        std::pmr::vector<Float> vec(sampleCount);

        for(uint32_t idx = 0; idx < sampleCount; ++idx) {
            if(const auto sample = bsdf.sample(sampler, wo, TransportMode::Importance); sample.valid()) {
                vec[idx] += (sample.f * (sample.inversePdf * absDot(normal, sample.wi))).raw();

                if(specular ||
                   (std::fabs(sample.f.raw() - bsdf.evaluate(wo, sample.wi, TransportMode::Importance).raw()) < 1e-3f &&
                    (sample.inversePdf.raw() - bsdf.pdf(wo, sample.wi).raw()) < 1e-3f)) {
                    ++count;
                }
            }

            const uint32_t dst = idx + ((idx + 1) & -(idx + 1));
            if(dst < sampleCount)
                vec[dst] += vec[idx];
        }

        const auto sum = vec.back() / sampleCount;

        ASSERT_LT(sum, 1.01) << " name " << name << " iteration " << k;
        // NOTICE: half vector causes the inconsistence
        ASSERT_GT(static_cast<double>(count) / static_cast<double>(sampleCount), 0.7) << " name " << name << " iteration " << k;
    }
}

static void testBSDF(const std::string_view name, const Normal<FrameOfReference::World>& normal, const BSDF<RSSMono>& bsdf,
                     const bool specular) {
    if(!specular)
        chi2Test(name, bsdf);
    testEnergyConservation(name, normal, bsdf, specular);
}

static void testBSDF(const std::string_view name, const std::string_view config, const bool specular = false) {
    FloatingPointExceptionProbe::on();

    const auto mat = getStaticFactory().make<Material<RSSMono>>(parseJSONConfigNodeFromStr(config, {}));
    SurfaceHit hit{ Point<FrameOfReference::World>::fromRaw(glm::zero<glm::vec3>()),
                    Distance::fromRaw(10.0f),
                    Normal<FrameOfReference::World>::fromRaw(glm::vec3{ 0.0f, 1.0f, 0.0f }),
                    Normal<FrameOfReference::World>::fromRaw(glm::vec3{ 0.0f, 1.0f, 0.0f }),
                    Direction<FrameOfReference::World>::fromRaw(glm::vec3{ 1.0f, 0.0f, 0.0f }),
                    0,
                    glm::zero<glm::vec2>(),
                    0.0f,
                    Handle<Material>{ mat.get() } };

    const auto bsdf = mat->evaluate(std::monostate{}, hit);
    testBSDF(name, hit.shadingNormal, bsdf, specular);

    // negative
    hit.geometryNormal = -hit.geometryNormal;
    const auto bsdfNeg = mat->evaluate(std::monostate{}, hit);
    testBSDF(name, hit.shadingNormal, bsdfNeg, specular);

    const auto matRGB = getStaticFactory().make<Material<RSSRGB>>(parseJSONConfigNodeFromStr(config, {}));
    const auto matSpectral = getStaticFactory().make<Material<RSSSpectral>>(parseJSONConfigNodeFromStr(config, {}));

    // albedo test

    /*
    const auto albedo1 = mat->estimateAlbedo(hit);
    const auto albedo2 = matRGB->estimateAlbedo(hit);
    const auto albedo3 = matSpectral->estimateAlbedo(hit);

    const auto lum1 = luminance(albedo1, {}), lum2 = luminance(albedo2, {}), lum3 = luminance(albedo3, {});

    constexpr auto tolerance = 1e-3;

    ASSERT_NEAR(lum1, lum2, tolerance);
    ASSERT_NEAR(lum1, lum3, tolerance);
    ASSERT_NEAR(lum2, lum3, tolerance);

    const auto diff = glm::abs(albedo2.raw() - albedo3.raw());
    const auto maxDiff = std::fmax(diff.x, std::fmax(diff.y, diff.z));

    ASSERT_LE(maxDiff, tolerance);
    */

    FloatingPointExceptionProbe::off();
}

TEST(BSDF, Diffuse) {
    testBSDF("Diffuse", R"(
{
    "Type": "Diffuse",
    "Reflectance": {
        "Type": "MonoSpectrumTexture",
        "Value": 1.0
    }
}
)");
}

TEST(BSDF, DielectricSmooth) {
    testBSDF("DielectricSmooth", R"(
{
    "Type": "Dielectric",
    "Material": "GlassBK7",
    "Roughness": 0.0
}
)",
             true);
}

TEST(BSDF, DielectricAnisotropicRoughness) {
    testBSDF("DielectricAnisotropicRoughness", R"(
{
    "Type": "Dielectric",
    "Material": "GlassBK7",
    "RoughnessU": 0.3,
    "RoughnessV": 0.5,
    "RemapRoughness": true
}
)");
}

TEST(BSDF, DielectricIsotropicRoughness) {
    testBSDF("DielectricIsotropicRoughness", R"(
{
    "Type": "Dielectric",
    "Material": "GlassBK7",
    "Roughness": 0.3,
    "RemapRoughness": true
}
)");
}

TEST(BSDF, ConductorSmooth) {
    testBSDF("ConductorSmooth", R"(
{
    "Type": "Conductor",
    "Material": "Cu",
    "Roughness": 0.0
}
)",
             true);
}

TEST(BSDF, ConductorAnisotropicRoughness) {
    testBSDF("ConductorAnisotropicRoughness", R"(
{
    "Type": "Conductor",
    "Material": "Cu",
    "RoughnessU": 0.3,
    "RoughnessV": 0.5,
    "RemapRoughness": true
}
)");
}

TEST(BSDF, ConductorIsotropicRoughness) {
    testBSDF("ConductorIsotropicRoughness", R"(
{
    "Type": "Conductor",
    "Material": "Cu",
    "Roughness": 0.3,
    "RemapRoughness": true
}
)");
}

PIPER_NAMESPACE_END
