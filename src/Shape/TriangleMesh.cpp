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
#pragma warning(push, 0)
// NOTE: assimp -> Irrlicht.dll -> opengl32.dll will cause memory leak.
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#pragma warning(pop)

PIPER_NAMESPACE_BEGIN

class TriangleMesh final : public Shape {
    Ref<PrimitiveGroup> mPrimitiveGroup;
    std::pmr::vector<glm::uvec3> mIndices{ context().globalAllocator };
    std::pmr::vector<Normal<FrameOfReference::Object>> mNormals{ context().globalAllocator };
    std::pmr::vector<Normal<FrameOfReference::Object>> mTangents{ context().globalAllocator };
    std::pmr::vector<TexCoord> mTexCoords{ context().globalAllocator };
    Ref<MaterialBase> mSurface;

public:
    explicit TriangleMesh(const Ref<ConfigNode>& node) {
        const auto path = node->get("Path"sv)->as<std::string_view>();

        Assimp::Importer importer;
        const auto* scene =
            importer.ReadFile(path.data(),
                              aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_GenSmoothNormals |
                                  aiProcess_FixInfacingNormals | aiProcess_ImproveCacheLocality | aiProcess_CalcTangentSpace);
        if(!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE)
            fatal(std::format("Failed to load scene {}: {}", path, importer.GetErrorString()));
        if(scene->mNumMeshes != 1)
            fatal("Only one mesh is supported.");

        const auto* mesh = scene->mMeshes[0];
        const auto verticesCount = mesh->mNumVertices;
        const auto trianglesCount = mesh->mNumFaces;

        mIndices.resize(trianglesCount);
        for(uint32_t idx = 0; idx < trianglesCount; ++idx) {
            static_assert(sizeof(glm::uvec3) == 3 * sizeof(uint32_t));
            mIndices[idx] = *reinterpret_cast<glm::uvec3*>(mesh->mFaces[idx].mIndices);
        }

        static_assert(sizeof(aiVector3D) == 3 * sizeof(Float));
        static_assert(sizeof(Normal<FrameOfReference::Object>) == 3 * sizeof(Float));
        mNormals.resize(verticesCount);
        memcpy(mNormals.data(), mesh->mNormals, verticesCount * sizeof(aiVector3D));
        mTangents.resize(verticesCount);
        memcpy(mTangents.data(), mesh->mTangents, verticesCount * sizeof(aiVector3D));
        mTexCoords.resize(verticesCount);
        for(uint32_t idx = 0; idx < verticesCount; ++idx) {
            const auto texCoord = mesh->mTextureCoords[0][idx];
            mTexCoords[idx] = { texCoord.x, texCoord.y };
        }

        const auto& builder = RenderGlobalSetting::get().accelerationBuilder;
        mPrimitiveGroup = builder->buildFromTriangleMesh(
            verticesCount, trianglesCount,
            [&](void* verticesBuffer, void* indicesBuffer) {
                static_assert(sizeof(aiVector3D) == 3 * sizeof(Float));
                memcpy(verticesBuffer, mesh->mVertices, verticesCount * sizeof(aiVector3D));
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
        // Please refer to https://github.com/embree/embree/blob/master/doc/src/api/RTC_GEOMETRY_TYPE_TRIANGLE.md
        const auto wu = 1.0f - barycentric.x - barycentric.y, wv = barycentric.x, ww = barycentric.y;
        const auto lerp3 = [&](auto u, auto v, auto w) { return u * wu + v * wv + w * ww; };

        const auto texCoord = lerp3(mTexCoords[iu], mTexCoords[iv], mTexCoords[iw]);
        const auto normalizedTexCoord = texCoord - glm::floor(texCoord);

        auto lerpNormal = transform(
            Normal<FrameOfReference::Object>::fromRaw(glm::normalize(lerp3(mNormals[iu].raw(), mNormals[iv].raw(), mNormals[iw].raw()))));
        auto lerpTangent = transform(Normal<FrameOfReference::Object>::fromRaw(
            glm::normalize(lerp3(mTangents[iu].raw(), mTangents[iv].raw(), mTangents[iw].raw()))));
        if(dot(lerpNormal, geometryNormal) < 0.0f) {
            lerpNormal = -lerpNormal;
            lerpTangent = -lerpTangent;
        }

        // TODO: front/back?

        const auto normal = lerpNormal.raw();
        const auto tangent = lerpTangent.raw();
        const auto biTangent = glm::cross(normal, tangent);

        const auto matTBN = glm::mat3{ tangent, biTangent, normal };
        const AffineTransform<FrameOfReference::Object, FrameOfReference::Shading> frame{ glm::mat4{
                                                                                              glm::transpose(matTBN),
                                                                                          },
                                                                                          glm::mat4{ matTBN } };

        return SurfaceHit{ ray.origin + ray.direction * hitDistance,
                           hitDistance,
                           geometryNormal,
                           lerpNormal,
                           primitiveIndex,
                           normalizedTexCoord,
                           transform.inverse() * frame,
                           Handle<Material>{ mSurface.get() } };
    }
};

PIPER_REGISTER_CLASS(TriangleMesh, Shape);

PIPER_NAMESPACE_END
