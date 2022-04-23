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

#include <Piper/Render/KeyFrames.hpp>
#include <Piper/Render/Texture.hpp>
#include <magic_enum.hpp>

PIPER_NAMESPACE_BEGIN

template <typename T>
class InterpolatedTexture : public T {
    std::pmr::vector<std::tuple<Float, InterpolationCurve, Ref<T>>> mKeyFrames{ context().globalAllocator };

    [[nodiscard]] std::tuple<T*, T*, Float> select(const Float t) const noexcept {
        auto iter = std::lower_bound(mKeyFrames.cbegin(), mKeyFrames.cend(), t, [](const auto& lhs, const Float rhs) {
            const auto& [lt, type, ref] = lhs;
            return lt < rhs;
        });

        if(iter == mKeyFrames.cend())
            iter = std::prev(mKeyFrames.cend());

        const auto& [bt, bc, bTexture] = *iter;
        if(bc == InterpolationCurve::Hold || std::next(iter) == mKeyFrames.cend())
            return { bTexture.get(), bTexture.get(), 0.0f };
        const auto& [et, ec, eTexture] = *std::next(iter);
        return { bTexture.get(), eTexture.get(), (t - bt) / (et - bt) };
    }

public:
    explicit InterpolatedTexture(const Ref<ConfigNode>& node) {
        auto& factory = getStaticFactory();
        const auto& array = node->get("KeyFrames"sv)->as<ConfigAttr::AttrArray>();
        mKeyFrames.reserve(array.size());
        for(auto& item : array) {
            auto& attr = item->as<Ref<ConfigNode>>();
            const auto t = attr->get("Time"sv)->as<Float>();
            auto curveType = InterpolationCurve::Linear;
            if(const auto ptr = attr->tryGet("InterpolationCurve"sv))
                curveType = magic_enum::enum_cast<InterpolationCurve>((*ptr)->as<std::string_view>()).value();

            mKeyFrames.emplace_back(t, curveType, factory.make<T>(attr->get("Texture"sv)->as<Ref<ConfigNode>>()));
        }
    }

    template <typename Callable, typename Mixer>
    auto evaluateImpl(Callable&& callable, Mixer&& mixer, const Float t) const noexcept {
        auto [a, b, u] = select(t);
        if(a == b)
            return std::invoke(std::forward<Callable>(callable), a);
        return std::invoke(std::forward<Mixer>(mixer), std::invoke(std::forward<Callable>(callable), a),
                           std::invoke(std::forward<Callable>(callable), b), u);
    }

    template <typename Callable>
    auto evaluateImpl(Callable&& callable, const Float t) const noexcept {
        return evaluateImpl(
            std::forward<Callable>(callable), [](const auto& a, const auto& b, const Float u) { return mix(a, b, u); }, t);
    }

    template <typename Callable>
    auto sampleImpl(SampleProvider& sampleProvider, Callable&& callable, const Float t) const noexcept {
        auto [a, b, u] = select(t);
        if(a == b)
            return std::invoke(std::forward<Callable>(callable), a);
        return std::invoke(std::forward<Callable>(callable), sampleProvider.sample() > u ? a : b);
    }

    template <typename Callable>
    auto meanImpl(Callable&& callable) const noexcept {
        using Result = std::invoke_result_t<std::decay_t<Callable>, const T*>;
        Result res = zero<Result>();

        if(mKeyFrames.empty())
            return res;
        if(mKeyFrames.size() == 1)
            return std::invoke(std::forward<Callable>(callable), std::get<Ref<T>>(mKeyFrames.front()).get());
        Float lastTime = 0.0f;
        for(uint32_t idx = 0; idx < mKeyFrames.size(); ++idx) {
            const auto& [t, type, tex] = mKeyFrames[idx];
            const auto val = std::invoke(std::forward<Callable>(callable), tex.get());
            Float currentTime = 0.0f;
            if(idx + 1 < mKeyFrames.size()) {
                const auto nt = std::get<Float>(mKeyFrames[idx + 1]);
                currentTime = nt - t;
            }

            if(type == InterpolationCurve::Hold) {
                res += val * (lastTime + currentTime);
                lastTime = 0.0f;
            } else {
                res += val * (lastTime + currentTime * 0.5f);
                lastTime = currentTime * 0.5f;
            }
        }

        return res;
    }
};

class InterpolatedScalarTexture2D final : public InterpolatedTexture<ScalarTexture2D> {
public:
    explicit InterpolatedScalarTexture2D(const Ref<ConfigNode>& node) : InterpolatedTexture{ node } {}
    Float evaluate(const TextureEvaluateInfo& info) const noexcept override {
        return evaluateImpl([&](const ScalarTexture2D* tex) { return tex->evaluate(info); }, info.t);
    }

    [[nodiscard]] std::pair<bool, Float> evaluateOneWavelength(const TextureEvaluateInfo& info,
                                                               const Float wavelength) const noexcept override {
        return evaluateImpl([&](const ScalarTexture2D* tex) { return tex->evaluateOneWavelength(info, wavelength); },
                            [](const auto& a, const auto& b, const Float u) -> std::pair<bool, Float> {
                                return { a.first | b.first, mix(a.second, b.second, u) };
                            },
                            info.t);
    }
};

PIPER_REGISTER_CLASS_IMPL("InterpolatedTexture", InterpolatedScalarTexture2D, ScalarTexture2D, InterpolatedScalarTexture2D);

template <typename Setting>
class InterpolatedSpectrumTexture2D : public InterpolatedTexture<SpectrumTexture2D<Setting>> {
    PIPER_IMPORT_SETTINGS();

public:
    explicit InterpolatedSpectrumTexture2D(const Ref<ConfigNode>& node) : InterpolatedTexture<SpectrumTexture2D<Setting>>{ node } {}

    Spectrum evaluate(const TextureEvaluateInfo& info, const Wavelength& sampledWavelength) const noexcept override {
        return this->evaluateImpl([&](const SpectrumTexture2D<Setting>* tex) { return tex->evaluate(info, sampledWavelength); }, info.t);
    }

    [[nodiscard]] std::pair<bool, Float> evaluateOneWavelength(const TextureEvaluateInfo& info, Float wavelength) const noexcept override {
        return this->evaluateImpl([&](const SpectrumTexture2D<Setting>* tex) { return tex->evaluateOneWavelength(info, wavelength); },
                                  [](const auto& a, const auto& b, const Float u) -> std::pair<bool, Float> {
                                      return { a.first | b.first, mix(a.second, b.second, u) };
                                  },
                                  info.t);
    }
};

PIPER_REGISTER_VARIANT_IMPL("InterpolatedTexture", InterpolatedSpectrumTexture2D, SpectrumTexture2D, InterpolatedSpectrumTexture2D);

template <typename Setting>
class InterpolatedSphericalTexture : public InterpolatedTexture<SphericalTexture<Setting>> {
    PIPER_IMPORT_SETTINGS();

public:
    explicit InterpolatedSphericalTexture(const Ref<ConfigNode>& node) : InterpolatedTexture<SphericalTexture<Setting>>{ node } {}

    Spectrum evaluate(const TextureEvaluateInfo& info, const Wavelength& sampledWavelength) const noexcept override {
        return this->evaluateImpl([&](const SphericalTexture<Setting>* tex) { return tex->evaluate(info, sampledWavelength); }, info.t);
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return this->meanImpl([](const SphericalTexture<Setting>* tex) { return tex->mean(); });
    }

    TextureSample<Setting> sample(SampleProvider& sampler, const Float t, const Wavelength& sampledWavelength) const noexcept override {
        return this->sampleImpl(
            sampler, [&](const SphericalTexture<Setting>* tex) { return tex->sample(sampler, t, sampledWavelength); }, t);
    }
};

PIPER_REGISTER_VARIANT_IMPL("InterpolatedTexture", InterpolatedSphericalTexture, SphericalTexture, InterpolatedSphericalTexture);

class InterpolatedNormalizedTexture2D : public InterpolatedTexture<NormalizedTexture2D> {
public:
    explicit InterpolatedNormalizedTexture2D(const Ref<ConfigNode>& node) : InterpolatedTexture{ node } {}

    [[nodiscard]] Direction<FrameOfReference::Shading> evaluate(const TextureEvaluateInfo& info) const noexcept override {
        return this->evaluateImpl([&](const NormalizedTexture2D* tex) { return tex->evaluate(info); },
                                  [](const auto& a, const auto& b, const Float u) {
                                      const auto va = a.raw();
                                      const auto vb = b.raw();
                                      const auto vc = glm::mix(va, vb, u);
                                      return Direction<FrameOfReference::Shading>::fromRaw(glm::normalize(vc));
                                  },
                                  info.t);
    }
};

PIPER_REGISTER_CLASS_IMPL("InterpolatedTexture", InterpolatedNormalizedTexture2D, NormalizedTexture2D, InterpolatedNormalizedTexture2D);

PIPER_NAMESPACE_END
