#pragma once

#include <memory>

namespace kitgui {

/**
 * A pared down "dom node" like interface. implements parent-child relationships using an intrusive linked list.
 */
class DomNode : public std::enable_shared_from_this<DomNode> {
   public:
    virtual ~DomNode() = default;

    DomNode* GetParent() const;
    DomNode* GetFirstChild() const;
    DomNode* GetNextSibling() const;
    void Insert(DomNode* newChild, DomNode* childAfter);
    void Remove(DomNode* node);
    void Detach();

    /** in your visitor, return true to continue visiting, false to stop. */
    template <typename FN>
    bool Visit(FN visitor);

    virtual void Update() {};
    virtual void Draw() {};

   protected:
    virtual void OnAttached() {};
    virtual void OnDetached() {};

   private:
    std::shared_ptr<DomNode> mFirstChild;
    std::shared_ptr<DomNode> mNextSibling;

    std::weak_ptr<DomNode> mParent;
    std::weak_ptr<DomNode> mLastChild;
    std::weak_ptr<DomNode> mPreviousSibling;
};

template <typename FN>
bool DomNode::Visit(FN visitor) {
    if (!visitor(*this)) {
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
    return true;
}

}  // namespace kitgui
