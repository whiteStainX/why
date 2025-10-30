# Event-Driven Architecture for Animation Handling

This document outlines a proposed architectural improvement for the `why` audio visualizer, transitioning the animation system from a polling-based model to an event-driven one. This change will significantly improve efficiency, flexibility, and code modularity.

## 1. Current Architecture: Polling

In the current system, a central `AnimationManager` holds a list of all animations. On every frame, it iterates through this entire list and checks a set of hard-coded trigger conditions (like beat strength or band energy) for each animation to decide if it should be active.

### AS-IS Diagram

```
+-----------+   passes   +----------------+   calls    +--------------------+   iterates & checks   +-----------------+
| Main Loop |----------->|  render_frame  |----------->| AnimationManager   |---------------------->| Animation 1     |
+-----------+   data     +----------------+            | (update_all)       |                       | (is beat > 0.5?)|
                                                       +--------------------+                       +-----------------+
                                                               |
                                                               | iterates & checks   +-----------------+
                                                               +-------------------->| Animation 2     |
                                                               |                     | (is band[3] > 0.8?)|
                                                               |                     +-----------------+
                                                               |
                                                               | iterates & checks   +-----------------+
                                                               +-------------------->| Animation N     |
                                                                                     | (is beat > 0.2?)|
                                                                                     +-----------------+
```

### Weaknesses of the Polling Model

*   **Inefficient:** The trigger conditions for *every* animation are checked on *every* frame, even if no relevant audio event has occurred.
*   **Tightly Coupled:** The `AnimationManager` must know the specific trigger logic for every type of animation. Adding a new animation or a new trigger type requires modifying this central manager.
*   **Not Scalable:** As more animations are added, the iteration and checking process in the main loop becomes increasingly burdensome.

---

## 2. Proposed Architecture: Event-Driven

The proposed design introduces an **Event Bus** that decouples components. Instead of a central manager checking on everyone, components "emit" events, and other components "subscribe" to the events they care about.

### Key Components

*   **Events:** Simple data structures that represent something that has happened (e.g., a beat was detected).
*   **Emitters (Publishers):** Components that create and publish events to the Event Bus. For example, the `DspEngine` would publish a `BeatDetected` event.
*   **Event Bus (Dispatcher):** A central hub that receives events from emitters and dispatches them to any subscribers. It doesn't know what the events mean.
*   **Subscribers (Listeners):** Components that register with the Event Bus to be notified of specific events. Animations will be subscribers.

### TO-BE Diagram

```
                                     +--------------------+
                                     | Animation 1        |
                                     | (Subscribes to     |
+-----------+       publishes        |  BeatDetected)     |
| DspEngine |--------------------->  +--------------------+
| (Emitter) |       BeatDetected     |                    |
+-----------+                        |                    |
                                     |     Event Bus      |
+-----------+       publishes        |                    |
| Main Loop |--------------------->  |                    |
| (Emitter) |       FrameUpdate      |                    |
+-----------+                        +--------------------+-----> notifies
                                     | Animation 2        |
                                     | (Subscribes to     |
                                     |  FrameUpdate)      |
                                     +--------------------+

                                     +--------------------+
                                     | Animation N        |
                                     | (Subscribes to     |
                                     |  Beat, UserInput)  |
                                     +--------------------+
```

### Advantages of the Event-Driven Model

*   **Efficient:** Code is only executed when a relevant event occurs.
*   **Decoupled:** Emitters and subscribers don't know about each other. The `DspEngine` doesn't know what animations exist, and animations don't know where the beat events come from.
*   **Flexible & Extensible:** New animations can be added without modifying any central manager. They simply subscribe to the events they need. New event types (e.g., user input) can be added just as easily.

---

## 3. Event Flow Example: Beat Detection

1.  The `DspEngine` processes an audio buffer and detects a beat.
2.  It creates a `BeatDetectedEvent` object containing the beat's strength.
3.  It publishes this event to the global `EventBus`.
4.  The `EventBus` looks at its list of subscribers for the `BeatDetectedEvent`.
5.  It finds that `LightningWaveAnimation` and `AsciiMatrixAnimation` have subscribed.
6.  It calls the `onEvent(event)` method for each of those animations, passing them the `BeatDetectedEvent` data.
7.  The animations use this data to activate themselves or trigger a visual effect.
8.  Animations that didn't subscribe (e.g., a smoothly scrolling background) are not affected and their code is not executed.

---

## 4. C++ Implementation Sketch

Here is a simplified example of how the Event Bus and a subscribing animation could be implemented.

#### EventBus.h
```cpp
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Base event struct
struct Event {
    virtual ~Event() = default;
};

// Example of a specific event
struct BeatDetectedEvent : public Event {
    float strength;
};

using Subscriber = std::function<void(const Event&)>;

class EventBus {
public:
    // Subscribe to a specific event type
    template<typename T>
    void subscribe(Subscriber subscriber) {
        subscribers_[typeid(T).name()].push_back(subscriber);
    }

    // Publish an event to all relevant subscribers
    void publish(const Event& event) {
        const char* type_name = typeid(event).name();
        if (subscribers_.count(type_name)) {
            for (const auto& subscriber : subscribers_.at(type_name)) {
                subscriber(event);
            }
        }
    }

private:
    std::map<std::string, std::vector<Subscriber>> subscribers_;
};
```

#### LightningWaveAnimation.h
```cpp
#include "Animation.h"
#include "EventBus.h"

class LightningWaveAnimation : public Animation {
public:
    LightningWaveAnimation(EventBus& event_bus) {
        // Subscribe to the BeatDetectedEvent
        event_bus.subscribe<BeatDetectedEvent>([this](const Event& event) {
            this->onBeat(static_cast<const BeatDetectedEvent&>(event));
        });
    }

    void onBeat(const BeatDetectedEvent& event) {
        if (event.strength > trigger_threshold_) {
            // Activate the lightning effect
            activate();
        }
    }

    // ... other animation methods (update, render)
};
```
