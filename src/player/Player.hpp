#pragma once

#include "raylib.h"
#include "core/events.hpp"
#include "core/target.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

class Player {
public:
    Vector2 position = {400.0f, 300.0f};
    float radius = 20.0f;
    float speed = 250.0f;

    // Health system
    int health = 100;
    int maxHealth = 100;

    // Prevents damage every single frame while touching an enemy
    float damageCooldown = 0.0f;
    float damageInterval = 0.75f;

    // When set, a damaging touch publishes PlayerHurt, and a killing touch
    // also publishes PlayerDied. Game::Wire is what listens.
    void SetBus(events::Bus* bus) { bus_ = bus; }

    void Update(float deltaTime) {
        // Keep the existing WASD movement
        if (IsKeyDown(KEY_W)) position.y -= speed * deltaTime;
        if (IsKeyDown(KEY_S)) position.y += speed * deltaTime;
        if (IsKeyDown(KEY_A)) position.x -= speed * deltaTime;
        if (IsKeyDown(KEY_D)) position.x += speed * deltaTime;

        // Do not clamp the player to the window size. The camera follows the
        // player through the game world, so screen-sized limits create an
        // invisible box around the starting screen.

        // Update damage cooldown
        if (damageCooldown > 0.0f) {
            damageCooldown -= deltaTime;
        }
    }

    void CheckCollisions(
        std::vector<weapons::ShootTarget>& enemies,
        float deltaTime
    ) {
        (void)deltaTime;

        for (weapons::ShootTarget& enemy : enemies) {
            if (!enemy.alive) {
                continue;
            }

            // Calculate the distance between the player and enemy
            float dx = enemy.position.x - position.x;
            float dy = enemy.position.y - position.y;
            float distanceSquared = dx * dx + dy * dy;

            // Two circles collide when their distance is less
            // than the sum of their radii
            float collisionDistance = radius + enemy.radius;

            if (distanceSquared >= collisionDistance * collisionDistance) {
                continue;
            }

            // Push the enemy outside the player's circular hitbox
            float distance = std::sqrt(distanceSquared);

            if (distance > 0.0001f) {
                float overlap = collisionDistance - distance;

                // Move the enemy away from the player without damaging it.
                enemy.position.x += (dx / distance) * overlap;
                enemy.position.y += (dy / distance) * overlap;
            } else {
                // If both centers overlap exactly, move the enemy to the right.
                enemy.position.x += collisionDistance;
            }

            // Apply player damage only when the cooldown has expired
            if (health > 0 && damageCooldown <= 0.0f) {
                health = std::max(0, health - 10);
                damageCooldown = damageInterval;
                if (bus_) {
                    bus_->Publish(events::Event{events::Type::PlayerHurt, position, 10, health, nullptr});
                    if (health <= 0) {
                        bus_->Publish(events::Event{events::Type::PlayerDied, position, 10, health, nullptr});
                    }
                }
            }
        }
    }

    void Draw() {
        // Flash white when the player is temporarily protected
        Color playerColor = BLUE;

        if (damageCooldown > 0.0f) {
            playerColor = SKYBLUE;
        }

        DrawCircleV(position, radius, playerColor);

        // Draw the health bar above the player
        DrawHealthBar();
    }

    void DrawHealthBar() {
        float barWidth = 50.0f;
        float barHeight = 7.0f;

        float barX = position.x - barWidth / 2.0f;
        float barY = position.y - radius - 15.0f;

        // Background
        DrawRectangle(
            static_cast<int>(barX),
            static_cast<int>(barY),
            static_cast<int>(barWidth),
            static_cast<int>(barHeight),
            DARKGRAY
        );

        // Health fill
        float healthPercentage = static_cast<float>(health) / maxHealth;

        DrawRectangle(
            static_cast<int>(barX),
            static_cast<int>(barY),
            static_cast<int>(barWidth * healthPercentage),
            static_cast<int>(barHeight),
            health > 30 ? GREEN : RED
        );

        // Border
        DrawRectangleLines(
            static_cast<int>(barX),
            static_cast<int>(barY),
            static_cast<int>(barWidth),
            static_cast<int>(barHeight),
            WHITE
        );
    }

private:
    events::Bus* bus_ = nullptr;
};
