#pragma once
// Event bus. Systems announce facts ("an enemy died") instead of calling
// each other. Subscribe while the run is being set up. Publish from the
// system that knows the fact. Handlers run immediately, in the order they
// were subscribed.
//
// Add a new reaction without editing the shooter, the player, or the enemy
// list: subscribe here from app/game.hpp (see Game::Wire).
//
// Do not call Clear() or Unsubscribe() from inside a handler.

#include "raylib.h"

#include <functional>
#include <utility>
#include <vector>

namespace events {

enum class Type {
    EnemyKilled, // position is where the enemy died. amount is the killing hit.
    WeaponFired, // position is the muzzle. name is the weapon name.
    PlayerHurt,  // amount is damage taken. health is HP left after the hit.
    PlayerDied,  // health is 0. The run should stop.
};

struct Event {
    Type type = Type::EnemyKilled;
    Vector2 position{0.0f, 0.0f};
    int amount = 0;
    int health = 0;
    const char* name = nullptr; // weapon name for WeaponFired; otherwise unused
};

class Bus {
public:
    using Handler = std::function<void(const Event&)>;

    // Returns an id that Unsubscribe understands. Most systems never need it.
    int Subscribe(Type type, Handler handler) {
        const int id = nextId_++;
        handlers_.push_back(Slot{id, type, std::move(handler)});
        return id;
    }

    void Unsubscribe(int id) {
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if (it->id == id) {
                handlers_.erase(it);
                return;
            }
        }
    }

    // Handlers already subscribed when Publish starts will run. A handler
    // that Subscribes during Publish does not see this same event.
    void Publish(const Event& event) {
        const int count = static_cast<int>(handlers_.size());
        for (int i = 0; i < count; ++i) {
            if (handlers_[i].type == event.type && handlers_[i].handler) {
                handlers_[i].handler(event);
            }
        }
    }

    // Drops every subscription. Call this before wiring a new run.
    void Clear() { handlers_.clear(); }

private:
    struct Slot {
        int id = 0;
        Type type = Type::EnemyKilled;
        Handler handler;
    };

    std::vector<Slot> handlers_;
    int nextId_ = 1;
};

}  // namespace events
