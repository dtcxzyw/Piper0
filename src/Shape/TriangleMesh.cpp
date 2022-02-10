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

#include <Piper/Render/Acceleration.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/Shape.hpp>
#include <tiny_obj_loader.h>

PIPER_NAMESPACE_BEGIN

class TriangleMesh final : public Shape {
    Ref<PrimitiveGroup> mPrimitiveGroup;
    std::pmr::vector<glm::uvec3> mIndices{ context().globalAllocator };
    std::pmr::vector<Normal<FrameOfReference::Object>> mNormals{ context().globalAllocator };
    std::pmr::vector<TexCoord> mTexCoords{ context().globalAllocator };
    Ref<MaterialBase> mSurface;

public:
    explicit TriangleMesh(const Ref<ConfigNode>& node) {
        const auto path = node->get("Path"sv)->as<std::string_view>();
        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig config;
        config.triangulate = true;
        config.vertex_color = false;
        if(!reader.ParseFromFile(std::string{ path }, config))
            fatal(fmt::format("Failed to load mesh {}", path));

        const auto& attr = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();
        const auto& vertices = attr.vertices;
        const auto& normals = attr.normals;
        const auto& texCoords = attr.texcoords;
        if(shapes.size() != 1)
            fatal("The number of shapes for mesh file must be 1.");
        const auto& indices = shapes.front().mesh.indices;

        const auto verticesCount = static_cast<uint32_t>(vertices.size()) / 3;
        const auto indicesCount = static_cast<uint32_t>(indices.size()) / 3;

        mNormals.resize(verticesCount);
        memcpy(mNormals.data(), normals.data(), normals.size() * sizeof(Float));
        mTexCoords.resize(verticesCount);
        memcpy(mTexCoords.data(), texCoords.data(), texCoords.size() * sizeof(Float));
        mIndices.resize(indicesCount);
        for(uint32_t idx = 0; idx < mIndices.size(); ++idx)
            mIndices[idx] = { indices[idx * 3].vertex_index, indices[idx * 3 + 1].vertex_index, indices[idx * 3 + 2].vertex_index };

        const auto& builder = RenderGlobalSetting::get().accelerationBuilder;
        mPrimitiveGroup = builder->buildFromTriangleMesh(
            verticesCount, indicesCount,
            [&](void* verticesBuffer, void* indicesBuffer) {
                memcpy(verticesBuffer, vertices.data(), vertices.size() * sizeof(Float));
                static_assert(sizeof(glm::uvec3) == 3 * sizeof(uint32_t));
                memcpy(indicesBuffer, mIndices.data(), mIndices.size() * sizeof(glm::uvec3));
            },
            *this);

        mSurface = makeVariant<MaterialBase, Material>(node->get("Surface"sv)->as<Ref<ConfigNode>>());
    }

    void updateTransform(const KeyFrames& keyFrames, const TimeInterval timeInterval) override {
        mPrimitiveGroup->updateTransform(
            generateTransform(keyFrames, timeInterval, RenderGlobalSetting::get().accelerationBuilder->maxStepCount()));
        mPrimitiveGroup->commit();
    }

    PrimitiveGroup* primitiveGroup() const noexcept override {
        return mPrimitiveGroup.get();
    }

    Intersection generateIntersection(const Ray& ray, const Distance hitDistance,
                                      const AffineTransform<FrameOfReference::Object, FrameOfReference::World>& transform,
                                      const Normal<FrameOfReference::World>& geometryNormal, const glm::vec2 barycentric,
                                      const uint32_t primitiveIndex) const noexcept override {
        const auto index = mIndices[primitiveIndex];
        const auto iu = index.x, iv = index.y, iw = index.z;
        const auto wu = barycentric.x, wv = barycentric.y, ww = 1.0f - wu - wv;
        const auto lerp3 = [&](auto u, auto v, auto w) { return u * wu + v * wv + w * ww; };

        const auto texCoord = lerp3(mTexCoords[iu], mTexCoords[iv], mTexCoords[iw]);
        const auto lerpNormal = transform(
            Normal<FrameOfReference::Object>::fromRaw(glm::normalize(lerp3(mNormals[iu].raw(), mNormals[iv].raw(), mNormals[iw].raw()))));

        // TODO: front/back?

        const auto normal = lerpNormal.raw();
        constexpr auto ref0 = glm::vec3{ 1.0f, 0.0f, 0.0f }, ref1 = glm::vec3{ 0.0f, 1.0f, 0.0f };
        const auto ref = std::fabs(glm::dot(normal, ref0)) < std::fabs(glm::dot(normal, ref1)) ? ref0 : ref1;
        const auto tangent = glm::cross(normal, ref);
        const auto biTangent = glm::cross(normal, tangent);

        const auto matTBN = glm::mat3{ tangent, biTangent, normal };
        const AffineTransform<FrameOfReference::Object, FrameOfReference::Shading> frame{ glm::mat3x4{ glm::transpose(matTBN) },
                                                                                          glm::mat3x4{ matTBN } };

        return SurfaceHit{ ray.origin + ray.direction * hitDistance,
                           hitDistance,
                           geometryNormal,
                           lerpNormal,
                           primitiveIndex,
                           texCoord,
                           transform.inverse() * frame,
                           Handle<Material>{ mSurface.get() } };
    }
};

PIPER_REGISTER_CLASS(TriangleMesh, Shape);

PIPER_NAMESPACE_END
