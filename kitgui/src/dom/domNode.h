#pragma once

#include <cassert>
#include <memory>
namespace kitgui {
namespace dom {
// A pared down "dom node" like interface. implements parent-child relationships using an intrusive linked list.
class DomNode : public std::enable_shared_from_this<DomNode> {
   public:
    DomNode* GetParent() const { return mParent.lock().get(); }
    DomNode* GetFirstChild() const { return mFirstChild.get(); }
    DomNode* GetNextSibling() const { return mNextSibling.get(); }
    void Insert(DomNode* newChild, DomNode* childAfter) {
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

        } else {
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

    void Remove(DomNode* node) {
        if (node->mParent.lock().get() == this) {
            node->Detach();
        }
    }

    void Detach() {
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

    /* in your visitor, return true to continue visiting, false to stop. */
    template <typename FN>
    bool Visit(FN visitor) {
        if (!visitor(this)) {
            return false;
        }

        if (mFirstChild) {
            if (!mFirstChild->Visit(visitor)) {
                return false;
            }
        }
        if (mNextSibling) {
            if (!mNextSibling->Visit(visitor)) {
                return false;
            }
        }
    }

   protected:
    void OnAttached() {};
    void OnDetached() {};

   private:
    std::shared_ptr<DomNode> mFirstChild;
    std::shared_ptr<DomNode> mNextSibling;

    std::weak_ptr<DomNode> mParent;
    std::weak_ptr<DomNode> mLastChild;
    std::weak_ptr<DomNode> mPreviousSibling;
};
}  // namespace dom
}  // namespace kitgui