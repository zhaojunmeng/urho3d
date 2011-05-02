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
#include "Audio.h"
#include "Context.h"
#include "Sound.h"
#include "SoundSource3D.h"
#include "XMLElement.h"

#include "DebugNew.h"

static const float DEFAULT_NEARDISTANCE = 0.0f;
static const float DEFAULT_FARDISTANCE = 100.0f;
static const float DEFAULT_ROLLOFF = 2.0f;
static const float MIN_ROLLOFF = 0.1f;

OBJECTTYPESTATIC(SoundSource3D);

SoundSource3D::SoundSource3D(Context* context) :
    SoundSource(context),
    nearDistance_(DEFAULT_NEARDISTANCE),
    farDistance_(DEFAULT_FARDISTANCE),
    rolloffFactor_(DEFAULT_ROLLOFF)
{
    // Start from zero volume until attenuation properly calculated
    attenuation_ = 0.0f;
}

void SoundSource3D::RegisterObject(Context* context)
{
    context->RegisterFactory<SoundSource3D>();
    context->CopyBaseAttributes<SoundSource, SoundSource3D>();
    
    ATTRIBUTE(SoundSource3D, VAR_FLOAT, "Near Distance", nearDistance_, DEFAULT_NEARDISTANCE);
    ATTRIBUTE(SoundSource3D, VAR_FLOAT, "Far Distance", farDistance_, DEFAULT_FARDISTANCE);
    ATTRIBUTE(SoundSource3D, VAR_FLOAT, "Rolloff Factor", rolloffFactor_, DEFAULT_ROLLOFF);
}

void SoundSource3D::Update(float timeStep)
{
    CalculateAttenuation();
    SoundSource::Update(timeStep);
}

void SoundSource3D::SetDistanceAttenuation(float nearDistance, float farDistance, float rolloffFactor)
{
    nearDistance_ = Max(nearDistance, 0.0f);
    farDistance_ = Max(farDistance, 0.0f);
    rolloffFactor_ = Max(rolloffFactor, MIN_ROLLOFF);
}

void SoundSource3D::SetFarDistance(float distance)
{
    farDistance_ = Max(distance, 0.0f);
}

void SoundSource3D::SetNearDistance(float distance)
{
    nearDistance_ = Max(distance, 0.0f);
}

void SoundSource3D::SetRolloffFactor(float factor)
{
    rolloffFactor_ = Max(factor, MIN_ROLLOFF);
}

void SoundSource3D::CalculateAttenuation()
{
    if (!audio_)
        return;
    
    float interval = farDistance_ - nearDistance_;
    if (interval > 0.0f)
    {
        Vector3 relativePos(audio_->GetListenerRotation().GetInverse() * (GetWorldPosition() - audio_->GetListenerPosition()));
        float distance = Clamp(relativePos.GetLength() - nearDistance_, 0.0f, interval);
        
        attenuation_ = powf(1.0f - distance / interval, rolloffFactor_);
        panning_ = relativePos.GetNormalized().x_;
    }
}