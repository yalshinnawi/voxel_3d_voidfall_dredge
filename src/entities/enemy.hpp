#pragma once

#include "world.hpp"
#include "animation_state.hpp"

namespace Voidfall {

class Enemy {
public:
    Enemy(const glm::vec3& position, AnimationState initialAnimationState);

    void update(const World& world, float deltaTime);
    void setAnimationState(AnimationState state);
    AnimationState getAnimationState() const;

    bool isTouchingWall() const;
    bool isTouchingCeiling() const;
    void moveAlongSurface(float deltaTime);
    void moveForward(float deltaTime);

private:
    glm::vec3 m_position;
    AnimationState m_animationState;
};

} // namespace Voidfall
