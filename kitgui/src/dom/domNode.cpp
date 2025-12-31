#include "kitgui/dom.h"

#include <cassert>

namespace kitgui {
DomNode* DomNode::GetParent() const {
    return mParent.lock().get();
}
DomNode* DomNode::GetFirstChild() const {
    return mFirstChild.get();
}
DomNode* DomNode::GetNextSibling() const {
    return mNextSibling.get();
}
void DomNode::Insert(DomNode* newChild, DomNode* childAfter) {
    newChild->Detach();

    if (childAfter) {
        assert(childAfter->GetParent() == this);
        if (childAfter->mPreviousSibling.expired()) {
            // insert to beginning
            newChild->mNextSibling = childAfter->shared_from_this();
            childAfter->mPreviousSibling = newChild->shared_from_this();

            this->mFirstChild = newChild->shared_from_this();
        } else {
            // insert in middle, before reference
            newChild->mPreviousSibling = childAfter->mPreviousSibling.lock();
            newChild->mNextSibling = childAfter->shared_from_this();
            childAfter->mPreviousSibling = newChild->shared_from_this();
        }

    } else if (!this->mLastChild.expired()) {
        // append to end
        std::shared_ptr<DomNode> childBefore = mLastChild.lock();
        childBefore->mNextSibling = newChild->shared_from_this();
        newChild->mPreviousSibling = childBefore;

        this->mLastChild = newChild->shared_from_this();
    }

    // attach to beginning or end if needed
    if (this->mFirstChild == nullptr) {
        this->mFirstChild = newChild->shared_from_this();
    }
    if (this->mLastChild.expired()) {
        this->mLastChild = newChild->shared_from_this();
    }
    newChild->mParent = shared_from_this();

    OnAttached();
}

void DomNode::Remove(DomNode* node) {
    if (node->mParent.lock().get() == this) {
        node->Detach();
    }
}

void DomNode::Detach() {
    std::shared_ptr<DomNode> parent = mParent.lock();
    if (!parent) {
        // nothing to detach
        return;
    }
    std::shared_ptr<DomNode> previousSibling = mPreviousSibling.lock();
    std::shared_ptr<DomNode> nextSibling = mNextSibling;

    if (previousSibling && nextSibling) {
        // being removed from the middle, patch up
        previousSibling->mNextSibling = nextSibling;
        nextSibling->mPreviousSibling = previousSibling;
    } else if (previousSibling) {
        // removing last element
        previousSibling->mNextSibling.reset();
        parent->mLastChild = previousSibling;
    } else if (nextSibling) {
        // removing first element
        nextSibling->mPreviousSibling.reset();
        parent->mFirstChild = nextSibling;
    } else {
        // removing only element
        parent->mFirstChild.reset();
        parent->mLastChild.reset();
    }
    mParent.reset();

    OnDetached();
}

}  // namespace kitgui