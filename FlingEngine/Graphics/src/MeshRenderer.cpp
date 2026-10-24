#include "MeshRenderer.h"
#include "MeshRendererGpu.h"
#include "Model.h"
#include "Material.h"
#include "Buffer.h"

#include <entt/entity/helper.hpp>

namespace Fling
{
    MeshRenderer::MeshRenderer() = default;

    MeshRenderer::MeshRenderer(const MeshRenderer& t_Other)
        : m_Model(t_Other.m_Model),
          m_Material(t_Other.m_Material),
          m_UniformBuffer(nullptr),
          m_GpuState(nullptr)
    {
    }

    MeshRenderer& MeshRenderer::operator=(const MeshRenderer& t_Other)
    {
        if (this != &t_Other)
        {
            m_Model = t_Other.m_Model;
            m_Material = t_Other.m_Material;
        }
        return *this;
    }

    MeshRenderer::MeshRenderer(MeshRenderer&& t_Other) noexcept
        : m_Model(t_Other.m_Model),
          m_Material(t_Other.m_Material),
          m_UniformBuffer(t_Other.m_UniformBuffer),
          m_GpuState(t_Other.m_GpuState)
    {
        t_Other.m_Model = nullptr;
        t_Other.m_Material = nullptr;
        t_Other.m_UniformBuffer = nullptr;
        t_Other.m_GpuState = nullptr;
    }

    MeshRenderer& MeshRenderer::operator=(MeshRenderer&& t_Other) noexcept
    {
        if (this != &t_Other)
        {
            m_Model = t_Other.m_Model;
            m_Material = t_Other.m_Material;

            if (t_Other.m_UniformBuffer != nullptr)
            {
                Release();
                m_UniformBuffer = t_Other.m_UniformBuffer;
                m_GpuState = t_Other.m_GpuState;
                t_Other.m_UniformBuffer = nullptr;
                t_Other.m_GpuState = nullptr;
            }

            t_Other.m_Model = nullptr;
            t_Other.m_Material = nullptr;
        }
        return *this;
    }

    MeshRenderer::~MeshRenderer()
    {
        Release();
    }

    std::string MeshRenderer::GetModelGuidString() const
    {
        if (m_Model == nullptr)
        {
            return {};
        }
        return m_Model->GetGuidString();
    }

    std::string MeshRenderer::GetMaterialGuidString() const
    {
        if (m_Material == nullptr)
        {
            return {};
        }
        return m_Material->GetGuidString();
    }

    MeshRenderer::MeshRenderer(const std::string& t_MeshPath)
    {
        LoadModelFromPath(t_MeshPath);
        LoadMaterialFromPath("Materials/Default.mat");
    }

    MeshRenderer::MeshRenderer(const std::string& t_MeshPath, const std::string& t_MaterialPath)
    {
        LoadModelFromPath(t_MeshPath);
        LoadMaterialFromPath(t_MaterialPath);
    }

    MeshRenderer::MeshRenderer(Model* t_Model, Material* t_Mat /** = nullptr */) : m_Model(t_Model), m_Material(t_Mat)
    {
        if (!m_Material)
        {
            LoadMaterialFromPath("Materials/Default.mat");
        }
    }

    void MeshRenderer::Release()
    {
        delete m_UniformBuffer;
        m_UniformBuffer = nullptr;
        delete m_GpuState;
        m_GpuState = nullptr;
    }

    bool MeshRenderer::operator==(const MeshRenderer& other) const
    {
        return m_Model == other.m_Model && m_Material == other.m_Material;
    }

    bool MeshRenderer::operator!=(const MeshRenderer& other) const
    {
        return !(*this == other);
    }

    void MeshRenderer::LoadModelFromPath(const std::string& t_MeshPath)
    {
        // Load the model
        m_Model = Model::Create(Guid{t_MeshPath.c_str()}).get();
        assert(m_Model);
    }

    void MeshRenderer::LoadMaterialFromPath(const std::string& t_MatPath)
    {
        m_Material = Material::Create(Guid{t_MatPath.c_str()}).get();
        assert(m_Material);
    }

    void MeshRenderer::Serialize(JsonArchive& Ar)
    {
        std::string meshPath;
        std::string materialPath;

        if (Ar.IsSaving())
        {
            meshPath = m_Model ? m_Model->GetGuidString() : "";
            materialPath = m_Material ? m_Material->GetGuidString() : "";
        }

        Ar << MakeNVP("mesh", meshPath);
        Ar << MakeNVP("material", materialPath);

        if (Ar.IsLoading())
        {
            if (!meshPath.empty())
            {
                LoadModelFromPath(meshPath);
            }
            if (!materialPath.empty())
            {
                LoadMaterialFromPath(materialPath);
            }
        }
    }
} // namespace Fling