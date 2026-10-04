#include "enemy_movement.hpp"
#include "enemy.hpp"
#include "world.hpp"
#include "logger.hpp"

namespace Voidfall {

void EnemyMovement::update(Enemy& enemy, const World& world, float deltaTime) {
    // Existing movement logic
    // ...

    // Check for wall-climbing and ceiling-crawling conditions
    if (enemy.isTouchingWall() || enemy.isTouchingCeiling()) {
        applyWallClimbingAnimation(enemy);
    } else {
        applyNormalAnimation(enemy);
    }
}

void EnemyMovement::applyWallClimbingAnimation(Enemy& enemy) {
    // Implement wall-climbing animation logic
    // Example: Change the enemy's animation state to "climbing"
    enemy.setAnimationState(AnimationState::Climbing);
    // Update the enemy's position based on wall-climbing physics
    // Example: Move the enemy along the wall or ceiling
    enemy.moveAlongSurface(deltaTime);
}

void EnemyMovement::applyNormalAnimation(Enemy& enemy) {
    // Implement normal movement animation logic
    // Example: Change the enemy's animation state to "walking"
    enemy.setAnimationState(AnimationState::Walking);
    // Update the enemy's position based on normal movement physics
    // Example: Move the enemy forward
    enemy.moveForward(deltaTime);
}

} // namespace Voidfall
