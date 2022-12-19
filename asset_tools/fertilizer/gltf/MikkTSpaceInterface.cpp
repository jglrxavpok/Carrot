//
// Created by jglrxavpok on 19/12/2022.
//

#include "MikkTSpaceInterface.h"

namespace Fertilizer {
    namespace {
        struct Context {
            ExpandedMesh& mesh;

            int getNumFaces() {
                return mesh.vertices.size() / 3;
            }

/*            int getNumVerticesOfFace(const int iFace) {
                return 3; // we support only triangles
            }*/

            std::size_t index(const int iFace, const int iVert) {
                return iFace * 3 + iVert;
            }

            void getPosition(float fvPosOut[], const int iFace, const int iVert) {
                const glm::vec4& hPos = mesh.vertices[index(iFace, iVert)].vertex.pos;
                for (int i = 0; i < 3; ++i) {
                    fvPosOut[i] = hPos[i];
                }
            }

            void getNormal(float fvNormOut[], const int iFace, const int iVert) {
                const glm::vec3& normal = mesh.vertices[index(iFace, iVert)].vertex.normal;
                for (int i = 0; i < 3; ++i) {
                    fvNormOut[i] = normal[i];
                }
            }

            void getTexCoord(float fvTexcOut[], const int iFace, const int iVert) {
                const glm::vec2& normal = mesh.vertices[index(iFace, iVert)].vertex.uv;
                for (int i = 0; i < 2; ++i) {
                    fvTexcOut[i] = normal[i];
                }
            }

            void setTSpaceBasic(const float fvTangent[], const float fSign, const int iFace, const int iVert) {
                glm::vec4& tangent = mesh.vertices[index(iFace, iVert)].vertex.tangent;
                for (int i = 0; i < 3; ++i) {
                    tangent[i] = fvTangent[i];
                }
                tangent.w = fSign;
            }
        };
    }

    bool generateTangents(ExpandedMesh& mesh) {
        Context context{ mesh };

        SMikkTSpaceInterface interface {
            .m_getNumFaces = [](const SMikkTSpaceContext* pContext) {
                return ((Context*)pContext->m_pUserData)->getNumFaces();
            },

            .m_getNumVerticesOfFace = [](const SMikkTSpaceContext* pContext, const int iFace) {
                return 3;
            },

            .m_getPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) {
                return ((Context*)pContext->m_pUserData)->getPosition(fvPosOut, iFace, iVert);
            },

            .m_getNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) {
                return ((Context*)pContext->m_pUserData)->getNormal(fvNormOut, iFace, iVert);
            },

            .m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) {
                return ((Context*)pContext->m_pUserData)->getTexCoord(fvTexcOut, iFace, iVert);
            },

            .m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
                return ((Context*)pContext->m_pUserData)->setTSpaceBasic(fvTangent, fSign, iFace, iVert);
            },

            .m_setTSpace = nullptr,
        };
        SMikkTSpaceContext mikkContext {
            .m_pInterface = &interface,
            .m_pUserData = &context,
        };
        bool result = genTangSpaceDefault(&mikkContext);
        return result;
    }
}