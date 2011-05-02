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
#include "Camera.h"
#include "Context.h"
#include "DebugRenderer.h"
#include "Light.h"
#include "ResourceCache.h"
#include "StringUtils.h"
#include "Texture2D.h"
#include "TextureCube.h"
#include "XMLElement.h"

#include "DebugNew.h"

static const LightType DEFAULT_LIGHTTYPE = LIGHT_POINT;
static const float DEFAULT_FOV = 30.0f;
static const float DEFAULT_CONSTANTBIAS = 0.0001f;
static const float DEFAULT_SLOPESCALEDBIAS = 0.5f;
static const float DEFAULT_LAMBDA = 0.5f;
static const float DEFAULT_SHADOWFADERANGE = 0.2f;
static const float DEFAULT_SHADOWQUANTIZE = 0.5f;
static const float DEFAULT_SHADOWMINVIEW = 3.0f;
static const float DEFAULT_SHADOWNEARFARRATIO = 0.002f;

static const char* typeNames[] =
{
    "directional",
    "spot",
    "point",
    "splitpoint",
    0
};

void BiasParameters::Validate()
{
    constantBias_ = Clamp(constantBias_, 0.0f, 1.0f);
    slopeScaledBias_ = Clamp(slopeScaledBias_, 0.0f, 16.0f);
}

void CascadeParameters::Validate()
{
    splits_ = Max(splits_, 1);
    lambda_ = Clamp(lambda_, 0.0f, 1.0f);
    splitFadeRange_ = Clamp(splitFadeRange_, M_EPSILON, 0.5f);
    shadowRange_ = Max(shadowRange_, M_EPSILON);
}

void FocusParameters::Validate()
{
    quantize_ = Max(quantize_, SHADOW_MIN_QUANTIZE);
    minView_ = Max(minView_, SHADOW_MIN_VIEW);
}

OBJECTTYPESTATIC(Light);

Light::Light(Context* context) :
    Drawable(context),
    lightType_(DEFAULT_LIGHTTYPE),
    specularIntensity_(0.0f),
    range_(0.0f),
    fov_(DEFAULT_FOV),
    aspectRatio_(1.0f),
    fadeDistance_(0.0f),
    shadowFadeDistance_(0.0f),
    shadowBias_(BiasParameters(DEFAULT_CONSTANTBIAS, DEFAULT_SLOPESCALEDBIAS)),
    shadowCascade_(CascadeParameters(1, DEFAULT_LAMBDA, DEFAULT_SHADOWFADERANGE, M_LARGE_VALUE)),
    shadowFocus_(FocusParameters(true, true, true, DEFAULT_SHADOWQUANTIZE, DEFAULT_SHADOWMINVIEW)),
    shadowIntensity_(0.0f),
    shadowResolution_(1.0f),
    shadowNearFarRatio_(DEFAULT_SHADOWNEARFARRATIO),
    nearSplit_(0.0f),
    farSplit_(M_LARGE_VALUE),
    nearFadeRange_(M_EPSILON),
    farFadeRange_(M_EPSILON),
    shadowCamera_(0),
    shadowMap_(0),
    originalLight_(0)
{
    drawableFlags_ =  DRAWABLE_LIGHT;
}

Light::~Light()
{
}

void Light::RegisterObject(Context* context)
{
    context->RegisterFactory<Light>();
    context->CopyBaseAttributes<Drawable, Light>();
    context->RemoveAttribute<Light>("Is Occluder");
    context->RemoveAttribute<Light>("LOD Bias");
    context->RemoveAttribute<Light>("Max Lights");
    
    ENUM_ATTRIBUTE(Light, "Light Type", lightType_, typeNames, DEFAULT_LIGHTTYPE);
    ATTRIBUTE(Light, VAR_COLOR, "Color", color_, Color());
    ATTRIBUTE(Light, VAR_FLOAT, "Specular Intensity", specularIntensity_, 0.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Range", range_, 0.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Spotlight FOV", fov_, DEFAULT_FOV);
    ATTRIBUTE(Light, VAR_FLOAT, "Spotlight Aspect Ratio", aspectRatio_, 1.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Fade Distance", fadeDistance_, 0.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Fade Distance", shadowFadeDistance_, 0.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Intensity", shadowIntensity_, 0.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Map Resolution", shadowResolution_, 1.0f);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Camera Near/Far Ratio", shadowNearFarRatio_, DEFAULT_SHADOWNEARFARRATIO);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Constant Bias", shadowBias_.constantBias_, DEFAULT_CONSTANTBIAS);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Slope-scaled Bias", shadowBias_.slopeScaledBias_, DEFAULT_SLOPESCALEDBIAS);
    ATTRIBUTE(Light, VAR_INT, "Shadow Splits", shadowCascade_.splits_, 1);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Split Lambda", shadowCascade_.lambda_, DEFAULT_LAMBDA);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Split Fade Range", shadowCascade_.splitFadeRange_, DEFAULT_SHADOWFADERANGE);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Split Max Range", shadowCascade_.shadowRange_, M_LARGE_VALUE);
    ATTRIBUTE(Light, VAR_BOOL, "Shadow Focus", shadowFocus_.focus_, true);
    ATTRIBUTE(Light, VAR_BOOL, "Shadow Focus Allow Non-uniform", shadowFocus_.nonUniform_, true);
    ATTRIBUTE(Light, VAR_BOOL, "Shadow Focus Allow Zoom-out", shadowFocus_.zoomOut_, true);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Focus Quantization", shadowFocus_.quantize_, DEFAULT_SHADOWQUANTIZE);
    ATTRIBUTE(Light, VAR_FLOAT, "Shadow Focus Min. View", shadowFocus_.minView_, DEFAULT_SHADOWMINVIEW);
    ATTRIBUTE(Light, VAR_RESOURCEREF, "Attenuation Ramp Texture", rampTexture_, ResourceRef(Texture2D::GetTypeStatic()));
    ATTRIBUTE(Light, VAR_RESOURCEREF, "Light Shape Texture", shapeTexture_, ResourceRef(Texture2D::GetTypeStatic()));
}

void Light::OnSetAttribute(const AttributeInfo& attr, const Variant& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    
    switch (attr.offset_)
    {
    case offsetof(Light, rampTexture_):
        {
            ResourceRef ref = value.GetResourceRef();
            rampTexture_ = static_cast<Texture*>(cache->GetResource(ref.type_, ref.id_));
        }
        break;
        
    case offsetof(Light, shapeTexture_):
        {
            ResourceRef ref = value.GetResourceRef();
            shapeTexture_ = static_cast<Texture*>(cache->GetResource(ref.type_, ref.id_));
        }
        break;
    
    default:
        Serializable::OnSetAttribute(attr, value);
        break;
    }
}

Variant Light::OnGetAttribute(const AttributeInfo& attr)
{
    switch (attr.offset_)
    {
    case offsetof(Light, rampTexture_):
        return GetResourceRef(rampTexture_, Texture2D::GetTypeStatic());
        break;
        
    case offsetof(Light, shapeTexture_):
        return GetResourceRef(shapeTexture_, Texture2D::GetTypeStatic());
        break;
    
    default:
        return Serializable::OnGetAttribute(attr);
    }
}

void Light::UpdateDistance(const FrameUpdate& frame)
{
    switch (lightType_)
    {
    case LIGHT_DIRECTIONAL:
        // Directional light affects the whole scene, so it is always "closest"
        distance_ = 0.0f;
        break;
        
    default:
        distance_ = frame.camera_->GetDistance(GetWorldPosition());
        break;
    }
}

void Light::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    switch (lightType_)
    {
    case LIGHT_SPOT:
        debug->AddFrustum(GetFrustum(), color_, depthTest);
        break;
        
    case LIGHT_POINT:
        debug->AddBoundingBox(GetWorldBoundingBox(), GetColor(), depthTest);
        break;
    }
}

void Light::SetLightType(LightType type)
{
    lightType_ = type;
    
    // Validate shape texture type: 2D for spot lights, cube for point lights. Change to null if wrong
    if ((lightType_ == LIGHT_SPOT) && (shapeTexture_) && (shapeTexture_->GetType() != Texture2D::GetTypeStatic()))
        shapeTexture_ = 0;
    if ((lightType_ == LIGHT_POINT) && (shapeTexture_) && (shapeTexture_->GetType() != TextureCube::GetTypeStatic()))
        shapeTexture_ = 0;
}

void Light::SetColor(const Color& color)
{
    color_ = color;
}

void Light::SetRange(float range)
{
    range_ = Max(range, 0.0f);
}

void Light::SetFov(float fov)
{
    fov_ = Clamp(fov, 0.0f, M_MAX_FOV);
}

void Light::SetAspectRatio(float aspectRatio)
{
    aspectRatio_ = Max(aspectRatio, M_EPSILON);
}

void Light::SetShadowNearFarRatio(float nearFarRatio)
{
    shadowNearFarRatio_ = Clamp(nearFarRatio, 0.0f, 0.5f);
}

void Light::SetSpecularIntensity(float intensity)
{
    specularIntensity_ = Max(intensity, 0.0f);
}

void Light::SetFadeDistance(float distance)
{
    fadeDistance_ = Max(distance, 0.0f);
}

void Light::SetShadowBias(const BiasParameters& parameters)
{
    shadowBias_ = parameters;
    shadowBias_.Validate();
}

void Light::SetShadowCascade(const CascadeParameters& parameters)
{
    shadowCascade_ = parameters;
    shadowCascade_.Validate();
}

void Light::SetShadowFocus(const FocusParameters& parameters)
{
    shadowFocus_ = parameters;
    shadowFocus_.Validate();
}

void Light::SetShadowFadeDistance(float distance)
{
    shadowFadeDistance_ = Max(distance, 0.0f);
}

void Light::SetShadowIntensity(float intensity)
{
    shadowIntensity_ = Clamp(intensity, 0.0f, 1.0f);
}

void Light::SetShadowResolution(float resolution)
{
    shadowResolution_ = Clamp(resolution, 0.25f, 1.0f);
}

void Light::SetRampTexture(Texture* texture)
{
    rampTexture_ = texture;
}

void Light::SetShapeTexture(Texture* texture)
{
    shapeTexture_ = texture;
}

void Light::copyFrom(Light* original)
{
    node_->SetTransform(original->GetWorldPosition(), original->GetWorldRotation(), Vector3::UNITY);
    castShadows_ = original->castShadows_;
    drawDistance_ = original->drawDistance_;
    shadowDistance_ = original->shadowDistance_;
    viewMask_ = original->viewMask_;
    distance_ = original->distance_;
    lightType_ = original->lightType_;
    range_ = original->range_;
    fov_ = original->fov_;
    aspectRatio_ = original->aspectRatio_;
    color_ = original->color_;
    specularIntensity_ = original->specularIntensity_;
    fadeDistance_ = original->fadeDistance_;
    shadowBias_ = original->shadowBias_;
    shadowCascade_ = original->shadowCascade_;
    shadowFocus_ = original->shadowFocus_;
    shadowFadeDistance_ = original->shadowFadeDistance_;
    shadowIntensity_ = original->shadowIntensity_;
    shadowResolution_ = original->shadowResolution_;
    rampTexture_ = original->rampTexture_;
    shapeTexture_ = original->shapeTexture_;
    originalLight_ = original;
}

Frustum Light::GetFrustum() const
{
    const Matrix4x3& transform = GetWorldTransform();
    Matrix4x3 frustumTransform(transform.GetTranslation(), transform.GetRotation(), 1.0f);
    Frustum ret;
    ret.Define(fov_, aspectRatio_, 1.0f, M_MIN_NEARCLIP, range_, transform);
    return ret;
}

float Light::GetVolumeExtent() const
{
    switch (lightType_)
    {
    case LIGHT_POINT:
        return range_ * 1.36f;
        
    case LIGHT_SPOT:
        {
            float safeRange = range_ * 1.001f;
            float yScale = tan(fov_ * M_DEGTORAD * 0.5f) * safeRange;
            float xScale = aspectRatio_ * yScale;
            return sqrtf(xScale * xScale + yScale * yScale + safeRange * safeRange);
        }
        
    case LIGHT_SPLITPOINT:
        {
            float safeRange = range_ * 1.001f;
            return sqrtf(3.0f * safeRange * safeRange);
        }
        
    default:
        return M_LARGE_VALUE;
    }
}

Matrix4x3 Light::GetDirLightTransform(Camera& camera, bool getNearQuad)
{
    Vector3 nearVector, farVector;
    camera.GetFrustumSize(nearVector, farVector);
    float nearClip = camera.GetNearClip();
    float farClip = camera.GetFarClip();
    
    float distance;
    
    if (getNearQuad)
        distance = Max(nearSplit_ - nearFadeRange_, nearClip);
    else
        distance = Min(farSplit_, farClip);
    if (!camera.IsOrthographic())
        farVector *= (distance / farClip);
    else
        farVector.z_ *= (distance / farClip);
    
    return  Matrix4x3(Vector3(0.0f, 0.0f, farVector.z_), Quaternion::IDENTITY, Vector3(farVector.x_, farVector.y_, 1.0f));
}

const Matrix4x3& Light::GetVolumeTransform(Camera& camera)
{
    const Matrix4x3& transform = GetWorldTransform();
    
    switch (lightType_)
    {
    case LIGHT_DIRECTIONAL:
        volumeTransform_ = GetDirLightTransform(camera);
        break;
        
    case LIGHT_POINT:
        volumeTransform_ = Matrix4x3(transform.GetTranslation(), Quaternion::IDENTITY, range_);
        break;
        
    case LIGHT_SPOT:
        {
            float yScale = tan(fov_ * M_DEGTORAD * 0.5f) * range_;
            float xScale = aspectRatio_ * yScale;
            volumeTransform_ = Matrix4x3(transform.GetTranslation(), transform.GetRotation(), Vector3(xScale, yScale, range_));
        }
        break;
        
    case LIGHT_SPLITPOINT:
        volumeTransform_ = Matrix4x3(transform.GetTranslation(), transform.GetRotation(), range_);
        break;
    }
    
    return volumeTransform_;
}

void Light::OnWorldBoundingBoxUpdate()
{
    switch (lightType_)
    {
    case LIGHT_DIRECTIONAL:
        // Directional light always sets humongous bounding box not affected by transform
        worldBoundingBox_.Define(-M_LARGE_VALUE, M_LARGE_VALUE);
        break;
        
    case LIGHT_POINT:
        {
            const Vector3& center = GetWorldPosition();
            Vector3 edge(range_, range_, range_);
            worldBoundingBox_.Define(center - edge, center + edge);
        }
        break;
        
    case LIGHT_SPOT:
    case LIGHT_SPLITPOINT:
        // Frustum is already transformed into world space
        worldBoundingBox_.Define(GetFrustum());
        break;
    }
}

void Light::SetNearSplit(float nearSplit)
{
    nearSplit_ = Max(nearSplit, 0.0f);
}

void Light::SetFarSplit(float farSplit)
{
    farSplit_ = Max(farSplit, 0.0f);
}

void Light::SetNearFadeRange(float range)
{
    nearFadeRange_ = Max(range, M_EPSILON);
}

void Light::SetFarFadeRange(float range)
{
    farFadeRange_ = Max(range, M_EPSILON);
}

void Light::SetShadowCamera(Camera* camera)
{
    shadowCamera_ = camera;
}

void Light::SetShadowMap(Texture2D* shadowMap)
{
    shadowMap_ = shadowMap;
}

void Light::SetIntensitySortValue(const Vector3& position)
{
    // Directional lights are always assumed to be "infinitely" close, while point and spot lights take distance into account
    float invIntensity = 1.0f / color_.GetIntensity();
    if (lightType_ == LIGHT_DIRECTIONAL)
        sortValue_ = invIntensity;
    else
        sortValue_ = invIntensity * (1.0f + (GetWorldPosition() - position).GetLengthFast() / range_);
}