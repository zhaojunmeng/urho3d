//
// Urho3D Engine
// Copyright (c) 2008-2012 Lasse ��rni
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

#include "Component.h"
#include "Vector3.h"

/// Supported constraint types.
enum ConstraintType
{
    CONSTRAINT_POINT = 0,
    CONSTRAINT_HINGE,
    CONSTRAINT_SLIDER
};

class PhysicsWorld;
class RigidBody;
class btTypedConstraint;

/// Physics constraint component. Connects two rigid bodies together, or one rigid body to a static point.
class Constraint : public Component
{
    OBJECT(Constraint);
    
    friend class RigidBody;
    
public:
    /// Construct.
    Constraint(Context* context);
    /// Destruct.
    ~Constraint();
    /// Register object factory.
    static void RegisterObject(Context* context);
    
    /// Handle attribute write access.
    virtual void OnSetAttribute(const AttributeInfo& attr, const Variant& src);
    /// Apply attribute changes that can not be applied immediately. Called after scene load or a network update.
    virtual void ApplyAttributes();
    /// Return the depended on nodes to order network updates.
    virtual void GetDependencyNodes(PODVector<Node*>& dest);
    /// Visualize the component as debug geometry.
    virtual void DrawDebugGeometry(DebugRenderer* debug, bool depthTest);
    
    /// %Set constraint type and recreate the constraint.
    void SetConstraintType(ConstraintType type);
    /// %Set other body to connect to. Set to null to connect to the static world.
    void SetOtherBody(RigidBody* body);
    /// %Set constraint position relative to own body.
    void SetPosition(const Vector3& position);
    /// %Set constraint axis relative to own body.
    void SetAxis(const Vector3& axis);
    /// %Set low limit. For hinge joints, this means the minimum (negative) angle.
    void SetLowLimit(float limit);
    /// %Set high limit. For hinge joints, this means the maximum (positive) angle.
    void SetHighLimit(float limit);
    
    /// Return physics world.
    PhysicsWorld* GetPhysicsWorld() const { return physicsWorld_; }
    /// Return Bullet constraint.
    btTypedConstraint* GetConstraint() const { return constraint_; }
    /// Return constraint type.
    ConstraintType GetConstraintType() const { return type_; }
    /// Return rigid body in own scene node.
    RigidBody* GetOwnBody() const { return ownBody_; }
    /// Return the other rigid body. May be null if connected to the static world.
    RigidBody* GetOtherBody() const { return otherBody_; }
    /// Return constraint position relative to own body.
    const Vector3& GetPosition() const { return position_; }
    /// Return constraint axis relative to own body.
    const Vector3& GetAxis() const { return axis_; }
    /// Return low limit.
    float GetLowLimit() const { return lowLimit_; }
    /// Return high limit.
    float GetHighLimit() const { return highLimit_; }
    
    /// Release the constraint.
    void ReleaseConstraint();
    
protected:
    /// Handle node being assigned.
    virtual void OnNodeSet(Node* node);
    /// Handle node transform being dirtied.
    virtual void OnMarkedDirty(Node* node);
    
private:
    /// Create the constraint.
    void CreateConstraint();
    
    /// Physics world.
    WeakPtr<PhysicsWorld> physicsWorld_;
    /// Own rigid body.
    WeakPtr<RigidBody> ownBody_;
    /// Other rigid body.
    WeakPtr<RigidBody> otherBody_;
    /// Bullet constraint.
    btTypedConstraint* constraint_;
    /// Constraint type.
    ConstraintType type_;
    /// Constraint position.
    Vector3 position_;
    /// Constraint axis.
    Vector3 axis_;
    /// Constraint other body position.
    Vector3 otherBodyPosition_;
    /// Constraint other body axis.
    Vector3 otherBodyAxis_;
    /// Cached world scale for determining if the constraint position needs update.
    Vector3 cachedWorldScale_;
    /// Low limit.
    float lowLimit_;
    /// High limit.
    float highLimit_;
    /// Other body node ID for pending constraint recreation.
    int otherBodyNodeID_;
    /// Disable collision between connected bodies flag.
    bool disableCollision_;
    /// Recreate constraint flag.
    bool recreateConstraint_;
    /// Other body position valid flag. Used to indicate that it should be used when recreating the joint.
    bool otherBodyPositionValid_;
};