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
    std::pmr::vector<Direction<FrameOfReference::Object>> mTangents{ context().globalAllocator };
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

        uint32_t verticesCount = 0;
        uint32_t trianglesCount = 0;
        for(uint32_t k = 0; k < scene->mNumMeshes; ++k) {
            verticesCount += scene->mMeshes[k]->mNumVertices;
            trianglesCount += scene->mMeshes[k]->mNumFaces;
        }

        static_assert(sizeof(aiVector3D) == 3 * sizeof(Float));
        static_assert(sizeof(Normal<FrameOfReference::Object>) == 3 * sizeof(Float));
        static_assert(sizeof(Direction<FrameOfReference::Object>) == 3 * sizeof(Float));
        static_assert(sizeof(glm::uvec3) == 3 * sizeof(uint32_t));

        mIndices.resize(trianglesCount);
        mNormals.resize(verticesCount);
        mTangents.resize(verticesCount);
        mTexCoords.resize(verticesCount);

        {
            uint32_t faceOffset = 0;
            uint32_t indexOffset = 0;
            for(uint32_t k = 0; k < scene->mNumMeshes; ++k) {
                const auto mesh = scene->mMeshes[k];
                for(uint32_t idx = 0; idx < mesh->mNumFaces; ++idx)
                    mIndices[faceOffset + idx] = indexOffset + *reinterpret_cast<glm::uvec3*>(mesh->mFaces[idx].mIndices);

                memcpy(mNormals.data() + indexOffset, mesh->mNormals, mesh->mNumVertices * sizeof(aiVector3D));

                if(mesh->HasTangentsAndBitangents()) {
                    memcpy(mTangents.data() + indexOffset, mesh->mTangents, mesh->mNumVertices * sizeof(aiVector3D));
                } else {
                    constexpr glm::vec3 refA{ 1.0, 0.0, 0.0 };
                    constexpr glm::vec3 refB{ 0.0, 1.0, 0.0 };
                    for(uint32_t idx = 0; idx < mesh->mNumVertices; ++idx) {
                        const auto normal = mNormals[indexOffset + idx].raw();
                        const auto dotA = std::fabs(glm::dot(refA, normal));
                        const auto dotB = std::fabs(glm::dot(refB, normal));
                        const auto ref = dotA < dotB ? refA : refB;
                        mTangents[indexOffset + idx] = Direction<FrameOfReference::Object>::fromRaw(glm::cross(normal, ref));
                    }
                }

                if(mesh->HasTextureCoords(0)) {
                    for(uint32_t idx = 0; idx < mesh->mNumVertices; ++idx) {
                        const auto texCoord = mesh->mTextureCoords[0][idx];
                        mTexCoords[indexOffset + idx] = { texCoord.x, texCoord.y };
                    }
                }

                indexOffset += mesh->mNumVertices;
                faceOffset += mesh->mNumFaces;
            }
        }

        const auto& builder = RenderGlobalSetting::get().accelerationBuilder;
        mPrimitiveGroup = builder->buildFromTriangleMesh(
            verticesCount, trianglesCount,
            [&](void* verticesBuffer, void* indicesBuffer) {
                uint32_t indexOffset = 0;
                for(uint32_t k = 0; k < scene->mNumMeshes; ++k) {
                    const auto mesh = scene->mMeshes[k];
                    memcpy(static_cast<aiVector3D*>(verticesBuffer) + indexOffset, mesh->mVertices,
                           mesh->mNumVertices * sizeof(aiVector3D));
                    indexOffset += mesh->mNumVertices;
                }

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
        const auto normalizedTexCoord = texCoord - glm::floor(texCoord);  // TODO: reduce normalization?

        const auto lerpNormal = transform(
            Normal<FrameOfReference::Object>::fromRaw(glm::normalize(lerp3(mNormals[iu].raw(), mNormals[iv].raw(), mNormals[iw].raw()))));
        const auto lerpTangent = transform(Direction<FrameOfReference::Object>::fromRaw(
            glm::normalize(lerp3(mTangents[iu].raw(), mTangents[iv].raw(), mTangents[iw].raw()))));

        return SurfaceHit{ ray.origin + ray.direction * hitDistance, hitDistance, geometryNormal, lerpNormal, lerpTangent, primitiveIndex,
                           normalizedTexCoord, ray.t,
                           // transform.inverse(),
                           Handle<Material>{ mSurface.get() } };
    }
};

PIPER_REGISTER_CLASS(TriangleMesh, Shape);

PIPER_NAMESPACE_END
