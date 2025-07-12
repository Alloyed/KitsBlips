#pragma once

/*
 * Ok this one is a bit ambitious: single file, header only signals, as they exist in solid.js and other web libraries.
 */

#include <algorithm>
#include <functional>
#include <memory>

namespace signals {
class Scope;

using Callback = std::function<void(Scope&)>;

class Trackable : std::enable_shared_from_this<Trackable> {};
class Tracker {
   public:
    virtual ~Tracker() = default;
    std::vector<std::shared_ptr<Trackable>> mDependencies;
};

template <typename T>
class Signal : public Trackable {
   public:
    Signal(T initialValue) : mValue(initialValue) {}
    ~Signal() = default;

    void Set(Scope& ctx, T value);
    T Get(Scope& ctx);

   private:
    T mValue;
};

class BaseMemo : public Trackable, public Tracker {
   public:
    virtual ~BaseMemo() = 0;
    void MarkDirty() const;

   protected:
    mutable bool mDirty = true;
};

template <typename T>
class Memo : public BaseMemo {
    using Fn = std::function<T(Scope& ctx, T lastValue)>;
    Memo(T initialValue, Fn mMemoCallback) : mValue(initialValue), mMemoCallback(mMemoCallback) {}
    ~Memo() override = default;

    T Get(Scope& ctx) const;

   private:
    const Fn mMemoCallback;
    mutable T mValue;
};

/* TODO: you cannot create effects inside of effects, yet.
 * The main problem is cleanup; re-running an effect should destroy effects created last time
 */
class Effect : public Tracker {
    friend Scope;

   public:
    Effect(Callback mCallback) : mCallback(mCallback) {}
    ~Effect() override = default;

    void CreateCleanup(Callback cleanup);

   private:
    void Run(Scope& ctx);
    void Cleanup(Scope& ctx);

    Callback mCallback;
    std::vector<Callback> mCleanup;
};

class UntrackScope {
   public:
    UntrackScope(Scope& ctx);
    ~UntrackScope();

   private:
    Scope& ctx;
};

class Scope {
    friend UntrackScope;

   public:
    Scope() = default;
    ~Scope() {
        for (auto& effect : mAllEffects) {
            effect->Cleanup(*this);
        }
    }

    template <typename T>
    std::shared_ptr<Signal<T>> CreateSignal(T initialValue);

    template <typename T>
    std::shared_ptr<Memo<T>> CreateMemo(Memo<T>::Callback callback);

    void CreateEffect(Callback effectCallback);

    //  TODO: hide impl
    void Track(std::shared_ptr<Trackable> trackable);

    void MarkChanged(std::shared_ptr<Trackable> trackable);

    void ApplyChanges();

    void PushTracker(Tracker* effect);
    void PopTracker();
    void StopTracking(Effect* tracker);

    void StopTracking(BaseMemo* tracker);

   private:
    std::vector<std::unique_ptr<Effect>> mAllEffects;
    std::vector<std::weak_ptr<BaseMemo>> mAllMemos;
    std::vector<Tracker*> mRunningTracker;
    std::vector<Effect*> mScheduledEffects;
    int32_t mNumUntrackScopes = 0;
};

// implementations

// signal

template <typename T>
inline std::shared_ptr<Signal<T>> Scope::CreateSignal(T initialValue) {
    return std::make_shared<Signal<T>>(initialValue);
}

template <typename T>
inline T Signal<T>::Get(Scope& ctx) {
    ctx.Track(shared_from_this());
    return mValue;
}

template <typename T>
inline void Signal<T>::Set(Scope& ctx, T value) {
    mValue = value;
    ctx.MarkChanged(shared_from_this());
}

// memo
template <typename T>
inline std::shared_ptr<Memo<T>> Scope::CreateMemo(Memo<T>::Callback callback) {
    auto memo = std::make_shared<Memo<T>>(callback);
    mAllMemos.emplace_back(memo);
    return memo;
}

template <typename T>
inline T Memo<T>::Get(Scope& ctx) const {
    if (mDirty) {
        mDependencies.clear();
        ctx.PushTracker(this);
        mValue = mMemoCallback(ctx, mValue);
        ctx.PopTracker();
    }
    return mValue;
}

inline void BaseMemo::MarkDirty() const {
    mDirty = true;
}

// effect
inline void Scope::CreateEffect(Callback effectCallback) {
    mAllEffects.push_back(std::make_unique<Effect>(effectCallback));
    mAllEffects.back()->Run(*this);
}

inline void Effect::Run(Scope& ctx) {
    Cleanup(ctx);
    ctx.PushTracker(this);
    mCallback(ctx);
    ctx.PopTracker();
}

inline void Effect::Cleanup(Scope& ctx) {
    for (const auto& cleanup : mCleanup) {
        cleanup(ctx);
    }
    mCleanup.clear();
    mDependencies.clear();
}

inline void Effect::CreateCleanup(Callback cleanup) {
    mCleanup.push_back(cleanup);
}

// context

inline void Scope::Track(std::shared_ptr<Trackable> trackable) {
    if (mNumUntrackScopes == 0 && !mRunningTracker.empty()) {
        mRunningTracker.back()->mDependencies.push_back(trackable);
    }
}

inline void Scope::MarkChanged(std::shared_ptr<Trackable> trackable) {
    // "real" implementations probably store a weak pointer list of the dependents in the trackable. whatever.
    for (auto& effect : mAllEffects) {
        const auto& deps = effect->mDependencies;
        if (std::find(deps.begin(), deps.end(), trackable) != deps.end()) {
            mScheduledEffects.push_back(effect.get());
        }
    }
    for (auto& memoRef : mAllMemos) {
        if (const auto memo = memoRef.lock()) {
            const auto& deps = memo->mDependencies;
            if (std::find(deps.begin(), deps.end(), trackable) != deps.end()) {
                memo->MarkDirty();
            }
        }
    }
}

inline void Scope::ApplyChanges() {
    while (!mScheduledEffects.empty()) {
        std::vector<Effect*> scheduledEffectsCopy = mScheduledEffects;
        for (auto effect : scheduledEffectsCopy) {
            effect->Run(*this);
        }
    }
}

inline void Scope::PushTracker(Tracker* effect) {
    mRunningTracker.push_back(effect);
}

inline void Scope::PopTracker() {
    mRunningTracker.pop_back();
}

inline void Scope::StopTracking(Effect* tracker) {
    tracker->Cleanup(*this);
    mScheduledEffects.erase(
        std::remove(mScheduledEffects.begin(), mScheduledEffects.end(), static_cast<Effect*>(tracker)));

    mRunningTracker.erase(std::remove(mRunningTracker.begin(), mRunningTracker.end(), tracker));

    mAllEffects.erase(
        std::remove_if(mAllEffects.begin(), mAllEffects.end(), [&](auto& e) { return e.get() == tracker; }));
}

inline void Scope::StopTracking(BaseMemo* tracker) {
    mRunningTracker.erase(std::remove(mRunningTracker.begin(), mRunningTracker.end(), tracker));
}

inline UntrackScope::UntrackScope(Scope& ctx) : ctx(ctx) {
    ctx.mNumUntrackScopes++;
}

inline UntrackScope::~UntrackScope() {
    ctx.mNumUntrackScopes--;
}

}  // namespace signals