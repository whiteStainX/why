#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace why {
namespace events {

class EventBus {
public:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    template<typename EventT>
    using Handler = std::function<void(const EventT&)>;

    template<typename EventT>
    void subscribe(Handler<EventT> handler) {
        auto wrapper = [handler = std::move(handler)](const void* event_ptr) {
            handler(*static_cast<const EventT*>(event_ptr));
        };
        subscribers_[std::type_index(typeid(EventT))].push_back(std::move(wrapper));
    }

    template<typename EventT>
    void publish(const EventT& event) const {
        auto it = subscribers_.find(std::type_index(typeid(EventT)));
        if (it == subscribers_.end()) {
            return;
        }
        for (const auto& wrapper : it->second) {
            wrapper(&event);
        }
    }

    void reset() {
        subscribers_.clear();
    }

private:
    using HandlerWrapper = std::function<void(const void*)>;
    std::unordered_map<std::type_index, std::vector<HandlerWrapper>> subscribers_;
};

} // namespace events
} // namespace why

