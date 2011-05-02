//
// Urho3D Engine
// Copyright (c) 2008-2011 Lasse ��rni
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Precompiled.h"
#include "Batch.h"
#include "Camera.h"
#include "Context.h"
#include "Geometry.h"
#include "Log.h"
#include "Material.h"
#include "Model.h"
#include "OcclusionBuffer.h"
#include "OctreeQuery.h"
#include "Profiler.h"
#include "ResourceCache.h"
#include "ResourceEvents.h"
#include "StaticModel.h"
#include "XMLElement.h"

#include "DebugNew.h"

OBJECTTYPESTATIC(StaticModel);

StaticModel::StaticModel(Context* context) :
    Drawable(context),
    softwareLodLevel_(M_MAX_UNSIGNED)
{
    drawableFlags_ = DRAWABLE_GEOMETRY;
}

StaticModel::~StaticModel()
{
}

void StaticModel::RegisterObject(Context* context)
{
    context->RegisterFactory<StaticModel>();
    context->CopyBaseAttributes<Drawable, StaticModel>();
    
    ATTRIBUTE(StaticModel, VAR_RESOURCEREF, "Model", model_, ResourceRef(Model::GetTypeStatic()));
    ATTRIBUTE(StaticModel, VAR_RESOURCEREFLIST, "Materials", materials_, ResourceRefList(Material::GetTypeStatic()));
    ATTRIBUTE(StaticModel, VAR_INT, "Raycast/Occlusion LOD Level", softwareLodLevel_, M_MAX_UNSIGNED);
}

void StaticModel::OnSetAttribute(const AttributeInfo& attr, const Variant& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    
    switch (attr.offset_)
    {
    case offsetof(StaticModel, model_):
        SetModel(cache->GetResource<Model>(value.GetResourceRef().id_));
        break;
        
    case offsetof(StaticModel, materials_):
        {
            const ResourceRefList& refs = value.GetResourceRefList();
            for (unsigned i = 0; i < refs.ids_.size(); ++i)
                SetMaterial(i, cache->GetResource<Material>(refs.ids_[i]));
        }
        break;
        
    default:
        Serializable::OnSetAttribute(attr, value);
        break;
    }
}

Variant StaticModel::OnGetAttribute(const AttributeInfo& attr)
{
    switch (attr.offset_)
    {
    case offsetof(StaticModel, model_):
        return GetResourceRef(model_, Model::GetTypeStatic());
        break;
        
    case offsetof(StaticModel, materials_):
        return GetResourceRefList(materials_);
        break;
    
    default:
        return Serializable::OnGetAttribute(attr);
    }
}


void StaticModel::ProcessRayQuery(RayOctreeQuery& query, float initialDistance)
{
    PROFILE(RaycastStaticModel);
    
    RayQueryLevel level = query.level_;
    
    switch (level)
    {
    case RAY_AABB_NOSUBOBJECTS:
    case RAY_AABB:
        {
            RayQueryResult result;
            result.drawable_ = this;
            result.node_ = GetNode();
            result.distance_ = initialDistance;
            query.result_.push_back(result);
        }
        break;
        
    case RAY_OBB:
        {
            Matrix4x3 inverse(GetWorldTransform().GetInverse());
            Ray localRay(inverse * query.ray_.origin_, inverse * Vector4(query.ray_.direction_, 0.0f));
            float distance = boundingBox_.GetDistance(localRay);
            if (distance < query.maxDistance_)
            {
                RayQueryResult result;
                result.drawable_ = this;
                result.node_ = GetNode();
                result.distance_ = distance;
                query.result_.push_back(result);
            }
        }
        break;
        
    case RAY_TRIANGLE:
        {
            // Do a pretest using the OBB
            Matrix4x3 inverse(GetWorldTransform().GetInverse());
            Ray localRay(inverse * query.ray_.origin_, inverse * Vector4(query.ray_.direction_, 0.0f));
            float distance = boundingBox_.GetDistance(localRay);
            if (distance < query.maxDistance_)
            {
                // Then the actual test using triangle geometry
                for (unsigned i = 0; i < geometries_.size(); ++i)
                {
                    unsigned lodLevel;
                    // Check whether to use same LOD as visible, or a specific LOD
                    if (softwareLodLevel_ == M_MAX_UNSIGNED)
                        lodLevel = lodLevels_[i];
                    else
                        lodLevel = Clamp(softwareLodLevel_, 0, geometries_[i].size());
                    
                    Geometry* geom = geometries_[i][lodLevel];
                    if (geom)
                    {
                        distance = geom->GetDistance(localRay);
                        if (distance < query.maxDistance_)
                        {
                            RayQueryResult result;
                            result.drawable_ = this;
                            result.node_ = GetNode();
                            result.distance_ = distance;
                            query.result_.push_back(result);
                            break;
                        }
                    }
                }
            }
        }
        break;
    }
}

void StaticModel::UpdateGeometry(const FrameUpdate& frame)
{
    if (lodLevelsDirty_)
        CalculateLodLevels();
}

unsigned StaticModel::GetNumBatches()
{
    return geometries_.size();
}

void StaticModel::GetBatch(const FrameUpdate& frame, unsigned batchIndex, Batch& batch)
{
    batch.geometry_ = geometries_[batchIndex][lodLevels_[batchIndex]];
    batch.worldTransform_ = &GetWorldTransform();
    batch.material_ = materials_[batchIndex];
}

bool StaticModel::DrawOcclusion(OcclusionBuffer* buffer)
{
    bool success = true;
    
    for (unsigned i = 0; i < geometries_.size(); ++i)
    {
        unsigned lodLevel;
        // Check whether to use same LOD as visible, or a specific LOD
        if (softwareLodLevel_ == M_MAX_UNSIGNED)
            lodLevel = lodLevels_[i];
        else
            lodLevel = Clamp(softwareLodLevel_, 0, geometries_[i].size());
        
        Geometry* geom = geometries_[i][lodLevel];
        if (!geom)
            continue;
        
        // Check that the material is suitable for occlusion (default material always is)
        // and set culling mode
        Material* mat = materials_[i];
        if (mat)
        {
            if (!mat->GetOcclusion())
                continue;
            buffer->SetCullMode(mat->GetCullMode());
        }
        else
            buffer->SetCullMode(CULL_CCW);
        
        const unsigned char* vertexData;
        unsigned vertexSize;
        const unsigned char* indexData;
        unsigned indexSize;
        
        geom->LockRawData(vertexData, vertexSize, indexData, indexSize);
        // Check for valid geometry data
        if ((!vertexData) || (!indexData))
            continue;
        
        unsigned indexStart = geom->GetIndexStart();
        unsigned indexCount = geom->GetIndexCount();
        
        // Draw and check for running out of triangles
        if (!buffer->Draw(GetWorldTransform(), vertexData, vertexSize, indexData, indexSize, indexStart, indexCount))
            success = false;
        
        geom->UnlockRawData();
        
        if (!success)
            break;
    }
    
    return success;
}

void StaticModel::SetModel(Model* model)
{
    if ((!model) || (model == model_))
        return;
    
    // Unsubscribe from the reload event of previous model (if any), then subscribe to the new
    if (model_)
        UnsubscribeFromEvent(model_, E_RELOADFINISHED);
    if (model)
        SubscribeToEvent(model, E_RELOADFINISHED, HANDLER(StaticModel, HandleModelReloadFinished));
    
    model_ = model;
    
    // Copy the subgeometry & LOD level structure
    SetNumGeometries(model->GetNumGeometries());
    const std::vector<std::vector<SharedPtr<Geometry> > >& geometries = model->GetGeometries();
    for (unsigned i = 0; i < geometries.size(); ++i)
        geometries_[i] = geometries[i];
    
    SetBoundingBox(model->GetBoundingBox());
    ResetLodLevels();
}

void StaticModel::SetMaterial(Material* material)
{
    for (unsigned i = 0; i < materials_.size(); ++i)
        materials_[i] = material;
}

bool StaticModel::SetMaterial(unsigned index, Material* material)
{
    if (index >= materials_.size())
    {
        LOGERROR("Illegal material index");
        return false;
    }
    
    materials_[index] = material;
    return true;
}

void StaticModel::SetSoftwareLodLevel(unsigned level)
{
    softwareLodLevel_ = level;
}

Material* StaticModel::GetMaterial(unsigned index) const
{
    return index < materials_.size() ? materials_[index] : (Material*)0;
}

void StaticModel::SetBoundingBox(const BoundingBox& box)
{
    boundingBox_ = box;
    OnMarkedDirty(node_);
}

void StaticModel::SetNumGeometries(unsigned num)
{
    geometries_.resize(num);
    materials_.resize(num);
    ResetLodLevels();
}

void StaticModel::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_ = boundingBox_.GetTransformed(GetWorldTransform());
}

void StaticModel::ResetLodLevels()
{
    // Ensure that each subgeometry has at least one LOD level, and reset the current LOD level
    lodLevels_.resize(geometries_.size());
    for (unsigned i = 0; i < geometries_.size(); ++i)
    {
        if (!geometries_[i].size())
            geometries_[i].resize(1);
        lodLevels_[i] = 0;
    }
    
    // Find out the real LOD levels on next geometry update
    lodLevelsDirty_ = true;
}

void StaticModel::CalculateLodLevels()
{
    for (unsigned i = 0; i < geometries_.size(); ++i)
    {
        unsigned j;
        for (j = 1; j < geometries_[i].size(); ++j)
        {
            if ((geometries_[i][j]) && (lodDistance_ <= geometries_[i][j]->GetLodDistance()))
                break;
        }
        lodLevels_[i] = j - 1;
    }
    
    lodLevelsDirty_ = false;
}

void StaticModel::HandleModelReloadFinished(StringHash eventType, VariantMap& eventData)
{
    Model* currentModel = model_;
    model_ = 0; // Set null to allow to be re-set
    SetModel(currentModel);
}