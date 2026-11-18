// app/queue_factory.h — build a queue by name. Owner: Architect.
#pragma once
#include <memory>
#include <string>

#include "event_queue.h"

namespace app {

// "heap", "avl", "oracle"; "heap-nopurge" exists only for the adversary's bloat experiment.
// Returns nullptr for an unknown name.
std::unique_ptr<eq::EventQueue> make_queue(const std::string& name);

}  // namespace app
