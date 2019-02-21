#include "Go.h"

#include <cassert>

#include "GoComponent.h"

GameObject::GameObject(GameObject &&rhs) : id_(rhs.id_), num_components_(rhs.num_components_) {
    for (int i = 0; i < num_components_; i++) {
        components_[i] = rhs.components_[i];
        components_[i]->set_owner(this);
        rhs.components_[i] = nullptr;
    }
    rhs.num_components_ = 0;
}

GameObject &GameObject::operator=(GameObject &&rhs) {
    id_ = rhs.id_;
    num_components_ = rhs.num_components_;
    for (int i = 0; i < num_components_; i++) {
        components_[i] = rhs.components_[i];
        components_[i]->set_owner(this);
        rhs.components_[i] = nullptr;
    }
    rhs.num_components_ = 0;

    return *this;
}

GameObject::~GameObject() {
    ClearComponents();
    id_ = GoID{ 0 };
}

GoComponent *GameObject::GetComponent(const GoID &id) const {
    for (int i = 0; i < num_components_; i++) {
        if (components_[i]->id() == id) {
            return components_[i];
        }
    }
    return nullptr;
}

void GameObject::AddComponent(GoComponent *c) {
    assert(GetComponent(c->id()) == nullptr && "Component added twice!");
    assert(num_components_ < MAX_GO_COMPONENTS);
    c->set_owner(this);
    components_[num_components_++] = c;
}

void GameObject::ClearComponents() {
    for (int i = num_components_ - 1; i >= 0; i--) {
        delete components_[i];
    }
    num_components_ = 0;
}