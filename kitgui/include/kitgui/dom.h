#pragma once

#include <Magnum/Math/Vector2.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace kitgui {
using Vector2 = Magnum::Vector2;  // TODO: replace?

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

    virtual void Update();
    virtual void Draw();

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

struct DomSceneImpl;
class DomScene : public DomNode {
   public:
    struct Props {
        std::string scenePath{};
    };
    static std::shared_ptr<DomScene> Create();
    ~DomScene();

    const Props& GetProps() const;
    void SetProps(const Props& props);
    void Load();
    void Update() override;
    void Draw() override;

   private:
    DomScene();
    Props mProps{};
    std::unique_ptr<DomSceneImpl> mImpl;
};

class DomKnob : public DomNode {
   public:
    struct Props {
        std::optional<Magnum::Vector2> position;
        std::string id{};
        std::string sceneAnchor{};
    };
    static std::shared_ptr<DomKnob> Create();
    ~DomKnob();
    const Props& GetProps() const;
    void SetProps(const Props& props);
    void Update() override;
    void Draw() override;

   private:
    DomKnob();
    Props mProps{};
};

}  // namespace kitgui