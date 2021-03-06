//
// Copyright (c) 2008-2013 the Urho3D project.
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
#include "HashSet.h"
#include "Vector3.h"

#include <LinearMath/btIDebugDraw.h>

class btCollisionConfiguration;
class btBroadphaseInterface;
class btConstraintSolver;
class btDiscreteDynamicsWorld;
class btDispatcher;
class btDynamicsWorld;
class btPersistentManifold;

namespace Urho3D
{

class CollisionShape;
class Deserializer;
class Constraint;
class Node;
class Ray;
class RigidBody;
class Scene;
class Serializer;
class XMLElement;

struct CollisionGeometryData;

/// Physics raycast hit.
struct PhysicsRaycastResult
{
    PhysicsRaycastResult() :
        body_(0)
    {
    }

    /// Hit position.
    Vector3 position_;
    /// Hit normal.
    Vector3 normal_;
    /// Hit distance from ray origin.
    float distance_;
    /// Rigid body that was hit.
    RigidBody* body_;
};

/// Delayed world transform assignment for parented rigidbodies.
struct DelayedWorldTransform
{
    /// Rigid body.
    RigidBody* rigidBody_;
    /// Parent rigid body.
    RigidBody* parentRigidBody_;
    /// New world position.
    Vector3 worldPosition_;
    /// New world rotation.
    Quaternion worldRotation_;
};

static const float DEFAULT_MAX_NETWORK_ANGULAR_VELOCITY = 100.0f;

/// Physics simulation world component. Should be added only to the root scene node.
class PhysicsWorld : public Component, public btIDebugDraw
{
    friend void InternalPreTickCallback(btDynamicsWorld *world, btScalar timeStep);
    friend void InternalTickCallback(btDynamicsWorld *world, btScalar timeStep);

    OBJECT(PhysicsWorld);

public:
    /// Construct.
    PhysicsWorld(Context* scontext);
    /// Destruct.
    virtual ~PhysicsWorld();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Check if an AABB is visible for debug drawing.
    virtual bool isVisible(const btVector3& aabbMin, const btVector3& aabbMax);
    /// Draw a physics debug line.
    virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color);
    /// Log warning from the physics engine.
    virtual void reportErrorWarning(const char* warningString);
    /// Draw a physics debug contact point. Not implemented.
    virtual void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) {}
    /// Draw physics debug 3D text. Not implemented.
    virtual void draw3dText(const btVector3& location,const char* textString) {}
    /// Set debug draw flags.
    virtual void setDebugMode(int debugMode) { debugMode_ = debugMode; }
    /// Return debug draw flags.
    virtual int getDebugMode() const { return debugMode_; }
    /// Visualize the component as debug geometry.
    virtual void DrawDebugGeometry(DebugRenderer* debug, bool depthTest);

    /// Step the simulation forward.
    void Update(float timeStep);
    /// Refresh collisions only without updating dynamics.
    void UpdateCollisions();
    /// Set simulation steps per second.
    void SetFps(int fps);
    /// Set gravity.
    void SetGravity(Vector3 gravity);
    /// Set whether to interpolate between simulation steps.
    void SetInterpolation(bool enable);
    /// Set maximum angular velocity for network replication.
    void SetMaxNetworkAngularVelocity(float velocity);
    /// Perform a physics world raycast and return all hits.
    void Raycast(PODVector<PhysicsRaycastResult>& result, const Ray& ray, float maxDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world raycast and return the closest hit.
    void RaycastSingle(PhysicsRaycastResult& result, const Ray& ray, float maxDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world swept sphere test and return the closest hit.
    void SphereCast(PhysicsRaycastResult& result, const Ray& ray, float radius, float maxDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid bodies by a sphere query.
    void GetRigidBodies(PODVector<RigidBody*>& result, const Sphere& sphere, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid bodies by a box query.
    void GetRigidBodies(PODVector<RigidBody*>& result, const BoundingBox& box, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid bodies that have been in collision with a specific body on the last simulation step.
    void GetRigidBodies(PODVector<RigidBody*>& result, const RigidBody* body);
    /// Return gravity.
    Vector3 GetGravity() const;
    /// Return whether interpolation between simulation steps is enabled.
    bool GetInterpolation() const { return interpolation_; }
    /// Return simulation steps per second.
    int GetFps() const { return fps_; }
    /// Return maximum angular velocity for network replication.
    float GetMaxNetworkAngularVelocity() const { return maxNetworkAngularVelocity_; }

    /// Add a rigid body to keep track of. Called by RigidBody.
    void AddRigidBody(RigidBody* body);
    /// Remove a rigid body. Called by RigidBody.
    void RemoveRigidBody(RigidBody* body);
    /// Add a collision shape to keep track of. Called by CollisionShape.
    void AddCollisionShape(CollisionShape* shape);
    /// Remove a collision shape. Called by CollisionShape.
    void RemoveCollisionShape(CollisionShape* shape);
    /// Add a constraint to keep track of. Called by Constraint.
    void AddConstraint(Constraint* joint);
    /// Remove a constraint. Called by Constraint.
    void RemoveConstraint(Constraint* joint);
    /// Add a delayed world transform assignment. Called by RigidBody.
    void AddDelayedWorldTransform(const DelayedWorldTransform& transform);
    /// Add debug geometry to the debug renderer.
    void DrawDebugGeometry(bool depthTest);
    /// Set debug renderer to use. Called both by PhysicsWorld itself and physics components.
    void SetDebugRenderer(DebugRenderer* debug);
    /// Set debug geometry depth test mode. Called both by PhysicsWorld itself and physics components.
    void SetDebugDepthTest(bool enable);

    /// Return the Bullet physics world.
    btDiscreteDynamicsWorld* GetWorld() { return world_; }
    /// Clean up the geometry cache.
    void CleanupGeometryCache();
    /// Return the collision geometry cache.
    HashMap<String, SharedPtr<CollisionGeometryData> >& GetGeometryCache() { return geometryCache_; }
    /// Set node dirtying to be disregarded.
    void SetApplyingTransforms(bool enable) { applyingTransforms_ = enable; }
    /// Return whether node dirtying should be disregarded.
    bool IsApplyingTransforms() const { return applyingTransforms_; }

protected:
    /// Handle node being assigned.
    virtual void OnNodeSet(Node* node);

private:
    /// Handle the scene subsystem update event, step simulation here.
    void HandleSceneSubsystemUpdate(StringHash eventType, VariantMap& eventData);
    /// Trigger update before each physics simulation step.
    void PreStep(float timeStep);
    /// Trigger update after ecah physics simulation step.
    void PostStep(float timeStep);
    /// Send accumulated collision events.
    void SendCollisionEvents();

    /// Bullet collision configuration.
    btCollisionConfiguration* collisionConfiguration_;
    /// Bullet collision dispatcher.
    btDispatcher* collisionDispatcher_;
    /// Bullet collision broadphase.
    btBroadphaseInterface* broadphase_;
    /// Bullet constraint solver.
    btConstraintSolver* solver_;
    /// Bullet physics world.
    btDiscreteDynamicsWorld* world_;
    /// Extra weak pointer to scene to allow for cleanup in case the world is destroyed before other components.
    WeakPtr<Scene> scene_;
    /// Rigid bodies in the world.
    PODVector<RigidBody*> rigidBodies_;
    /// Collision shapes in the world.
    PODVector<CollisionShape*> collisionShapes_;
    /// Constraints in the world.
    PODVector<Constraint*> constraints_;
    /// Collision pairs on this frame.
    HashMap<Pair<WeakPtr<RigidBody>, WeakPtr<RigidBody> >, btPersistentManifold* > currentCollisions_;
    /// Collision pairs on the previous frame. Used to check if a collision is "new." Manifolds are not guaranteed to exist anymore.
    HashMap<Pair<WeakPtr<RigidBody>, WeakPtr<RigidBody> >, btPersistentManifold* > previousCollisions_;
    /// Delayed (parented) world transform assignments.
    HashMap<RigidBody*, DelayedWorldTransform> delayedWorldTransforms_;
    /// Cache for collision geometry data.
    HashMap<String, SharedPtr<CollisionGeometryData> > geometryCache_;
    /// Simulation steps per second.
    unsigned fps_;
    /// Time accumulator for non-interpolated mode.
    float timeAcc_;
    /// Maximum angular velocity for network replication.
    float maxNetworkAngularVelocity_;
    /// Interpolation flag.
    bool interpolation_;
    /// Applying transforms flag.
    bool applyingTransforms_;
    /// Debug renderer.
    DebugRenderer* debugRenderer_;
    /// Debug draw flags.
    int debugMode_;
    /// Debug draw depth test mode.
    bool debugDepthTest_;
};

/// Register Physics library objects.
void RegisterPhysicsLibrary(Context* context);

}
