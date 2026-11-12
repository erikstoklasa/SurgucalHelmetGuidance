#include "scene.hpp"
#include <algorithm>

uint32_t Scene::addBody(const std::string& name)
{
    uint32_t id = nextId++;
    bodies.push_back({id, name});
    return id;
}

bool Scene::removeBody(uint32_t id)
{
    auto it = std::find_if(
        bodies.begin(), bodies.end(),
        [id](const RigidBody& body) {
            return body.id == id;
        }
    );

    if (it == bodies.end())
        return false;

    bodies.erase(it);
    return true;
}

RigidBody* Scene::findBody(uint32_t id)
{
    auto it = std::find_if(
        bodies.begin(), bodies.end(),
        [id](const RigidBody& body) {
            return body.id == id;
        }
    );

    return it != bodies.end() ? &(*it) : nullptr;
}