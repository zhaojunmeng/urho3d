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

#pragma once

#include "BoundingBox.h"
#include "Frustum.h"
#include "Ray.h"
#include "Sphere.h"

#include <vector>

class OcclusionBuffer;
class Drawable;
class Node;

/// Base class for octree queries
class OctreeQuery
{
public:
    /// Construct with result vector, include/exclude flags and whether to get only occluders or shadowcasters
    OctreeQuery(std::vector<Drawable*>& result, unsigned char drawableFlags, bool occludersOnly, bool shadowCastersOnly) :
        result_(result),
        drawableFlags_(drawableFlags),
        occludersOnly_(occludersOnly),
        shadowCastersOnly_(shadowCastersOnly)
    {
    }
    
    /// Destruct
    virtual ~OctreeQuery()
    {
    }
    
    /// Intersection test for an octant
    virtual Intersection TestOctant(const BoundingBox& box, unsigned& mask) const = 0;
    /// Intersection test for a drawable
    virtual Intersection TestDrawable(const BoundingBox& box, unsigned& mask) const = 0;
    
    /// Result vector reference
    std::vector<Drawable*>& result_;
    /// Drawable flags to include
    unsigned char drawableFlags_;
    /// Get occluders only flag
    bool occludersOnly_;
    /// Get shadowcasters only flag
    bool shadowCastersOnly_;
};

/// Point octree query
class PointOctreeQuery : public OctreeQuery
{
public:
    /// Construct with point and query parameters
    PointOctreeQuery(std::vector<Drawable*>& result, const Vector3& point, unsigned char drawableFlags, bool occludersOnly = false,
        bool shadowCastersOnly = false) :
        OctreeQuery(result, drawableFlags, occludersOnly, shadowCastersOnly),
        point_(point)
    {
    }
    
    /// Intersection test for an octant
    virtual Intersection TestOctant(const BoundingBox& box, unsigned& mask) const;
    /// Intersection test for a drawable
    virtual Intersection TestDrawable(const BoundingBox& box, unsigned& mask) const;
    
    /// Point
    Vector3 point_;
};

/// Sphere octree query
class SphereOctreeQuery : public OctreeQuery
{
public:
    /// Construct with sphere and query parameters
    SphereOctreeQuery(std::vector<Drawable*>& result, const Sphere& sphere, unsigned char drawableFlags, bool occludersOnly = false,
        bool shadowCastersOnly = false) :
        OctreeQuery(result, drawableFlags, occludersOnly, shadowCastersOnly),
        sphere_(sphere)
    {
    }
    
    /// Intersection test for an octant
    virtual Intersection TestOctant(const BoundingBox& box, unsigned& mask) const;
    /// Intersection test for a drawable
    virtual Intersection TestDrawable(const BoundingBox& box, unsigned& mask) const;
    
    /// Sphere
    Sphere sphere_;
};

/// Bounding box octree query
class BoxOctreeQuery : public OctreeQuery
{
public:
    /// Construct with bounding box and query parameters
    BoxOctreeQuery(std::vector<Drawable*>& result, const BoundingBox& box, unsigned char drawableFlags, bool occludersOnly = false,
        bool shadowCastersOnly = false) :
        OctreeQuery(result, drawableFlags, occludersOnly, shadowCastersOnly),
        box_(box)
    {
    }
    
    /// Intersection test for an octant
    virtual Intersection TestOctant(const BoundingBox& box, unsigned& mask) const;
    /// Intersection test for a drawable
    virtual Intersection TestDrawable(const BoundingBox& box, unsigned& mask) const;
    
    /// Bounding box
    BoundingBox box_;
};

/// Frustum octree query
class FrustumOctreeQuery : public OctreeQuery
{
public:
    /// Construct with frustum and query parameters
    FrustumOctreeQuery(std::vector<Drawable*>& result, const Frustum& frustum, unsigned char drawableFlags, bool occludersOnly = false,
        bool shadowCastersOnly = false) :
        OctreeQuery(result, drawableFlags, occludersOnly, shadowCastersOnly),
        frustum_(frustum)
    {
    }
    
    /// Intersection test for an octant
    virtual Intersection TestDrawable(const BoundingBox& box, unsigned& mask) const;
    /// Intersection test for a drawable
    virtual Intersection TestOctant(const BoundingBox& box, unsigned& mask) const;
    
    /// Frustum
    Frustum frustum_;
};

/// Frustum octree query with occlusion
class OccludedFrustumOctreeQuery : public OctreeQuery
{
public:
    /// Construct with frustum, occlusion buffer pointer and query parameters
    OccludedFrustumOctreeQuery(std::vector<Drawable*>& result, const Frustum& frustum, OcclusionBuffer* buffer,
            unsigned char drawableFlags, bool occludersOnly = false, bool shadowCastersOnly = false) :
        OctreeQuery(result, drawableFlags, occludersOnly, shadowCastersOnly),
        frustum_(frustum),
        buffer_(buffer)
    {
    }
    
    /// Intersection test for an octant
    virtual Intersection TestOctant(const BoundingBox& box, unsigned& mask) const;
    /// Intersection test for a drawable
    virtual Intersection TestDrawable(const BoundingBox& box, unsigned& mask) const;
    
    /// Frustum
    Frustum frustum_;
    /// Occlusion buffer
    OcclusionBuffer* buffer_;
};

/// Graphics raycast detail level
enum RayQueryLevel
{
    RAY_AABB_NOSUBOBJECTS = 0,
    RAY_AABB,
    RAY_OBB,
    RAY_TRIANGLE
};

/// Raycast result
struct RayQueryResult
{
    /// Construct
    RayQueryResult() :
        subObject_(M_MAX_UNSIGNED)
    {
    }
    
    /// Drawable
    Drawable* drawable_;
    /// Scene node
    Node* node_;
    /// Distance from ray origin
    float distance_;
    /// Drawable specific subobject if applicable
    unsigned subObject_;
};

/// Ray octree query
class RayOctreeQuery
{
public:
    /// Construct with ray and query parameters
    RayOctreeQuery(std::vector<RayQueryResult>& result, const Ray& ray, unsigned char drawableFlags, bool occludersOnly = false,
        bool shadowCastersOnly = false, float maxDistance = M_INFINITY, RayQueryLevel level = RAY_TRIANGLE) :
        ray_(ray),
        result_(result),
        drawableFlags_(drawableFlags),
        occludersOnly_(occludersOnly),
        shadowCastersOnly_(shadowCastersOnly),
        maxDistance_(maxDistance),
        level_(level)
    {
    }
    
    /// Ray
    Ray ray_;
    /// Result vector reference
    std::vector<RayQueryResult>& result_;
    /// Drawable flags to include
    unsigned char drawableFlags_;
    /// Get occluders only flag
    bool occludersOnly_;
    /// Get shadowcasters only flag
    bool shadowCastersOnly_;
    /// Maximum ray distance
    float maxDistance_;
    /// Raycast detail level
    RayQueryLevel level_;
};