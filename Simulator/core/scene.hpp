#include "rigid_body.hpp"
#include <vector>

class Scene {
public:
    uint32_t addBody(const std::string& name);
    bool removeBody(uint32_t id);

    RigidBody* findBody(uint32_t id);

    const std::vector<RigidBody>& getBodies() const {
        return bodies;
    }

private:
    std::vector<RigidBody> bodies;
    uint32_t nextId = 1;
};