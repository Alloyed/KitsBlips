#pragma once

/*
 * Ok this one is a bit ambitious: single file, header only signals, as they exist in solid.js and other web libraries.
 */

#include <algorithm>
#include <functional>
#include <memory>

namespace signals {
class Context;

using Callback = std::function<void(Context&)>;

class Trackable : std::enable_shared_from_this<Trackable> {};
class Tracker {
   public:
    std::vector<std::shared_ptr<Trackable>> mDependencies;
};

template <typename T>
class Signal : public Trackable {
   public:
    Signal(T initialValue) : mValue(initialValue) {}
    void Set(Context& ctx, T value);
    T Get(Context& ctx);

   private:
    T mValue;
};

class BaseMemo : public Trackable, public Tracker {
   public:
    void MarkDirty() const { mDirty = true; }

   protected:
    mutable bool mDirty = true;
};

template <typename T>
class Memo : public BaseMemo {
    using Fn = std::function<T(Context& ctx, T lastValue)>;
    Memo(T initialValue, Fn mMemoCallback) : mValue(initialValue), mMemoCallback(mMemoCallback) {}
    T Get(Context& ctx) const;

   private:
    const Fn mMemoCallback;
    mutable T mValue;
};

/* TODO: you cannot create effects inside of effects, yet.
 * The main problem is cleanup; re-running an effect should destroy effects created last time
 */
class Effect : public Tracker {
    friend Context;

   public:
    Effect(Callback mCallback) : mCallback(mCallback) {}
    ~Effect() {}

    void CreateCleanup(Callback cleanup) { mCleanup.push_back(cleanup); }

   private:
    void Run(Context& ctx);
    void Cleanup(Context& ctx);

    Callback mCallback;
    std::vector<Callback> mCleanup;
};

class UntrackScope {
   public:
    UntrackScope(Context& ctx);
    ~UntrackScope();

   private:
    Context& ctx;
};

class Context {
    friend UntrackScope;

   public:
    ~Context() {
        for (auto& effect : mAllEffects) {
            effect->Cleanup(*this);
        }
    }

    template <typename T>
    std::shared_ptr<Signal<T>> CreateSignal(T initialValue) {
        return std::make_shared<Signal<T>>(initialValue);
    }

    template <typename T>
    std::shared_ptr<Memo<T>> CreateMemo(Memo<T>::Callback callback) {
        auto memo = std::make_shared<Memo<T>>(callback);
        mAllMemos.emplace_back(memo);
        return memo;
    }

    void CreateEffect(Callback effectCallback) {
        mAllEffects.push_back(std::make_unique<Effect>(effectCallback));
        mAllEffects.back()->Run(*this);
    }

    //  TODO: hide impl
    void Track(std::shared_ptr<Trackable> trackable) {
        if (mNumUntrackScopes == 0 && !mRunningTracker.empty()) {
            mRunningTracker.back()->mDependencies.push_back(trackable);
        }
    }

    void MarkChanged(std::shared_ptr<Trackable> trackable) {
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

    void ApplyChanges() {
        while (!mScheduledEffects.empty()) {
            std::vector<Effect*> scheduledEffectsCopy = mScheduledEffects;
            for (auto effect : scheduledEffectsCopy) {
                effect->Run(*this);
            }
        }
    }

    void PushTracker(Tracker* effect) { mRunningTracker.push_back(effect); }
    void PopTracker() { mRunningTracker.pop_back(); }
    void StopTracking(Effect* tracker) {
        tracker->Cleanup(*this);
        mScheduledEffects.erase(
            std::remove(mScheduledEffects.begin(), mScheduledEffects.end(), static_cast<Effect*>(tracker)));

        mRunningTracker.erase(std::remove(mRunningTracker.begin(), mRunningTracker.end(), tracker));

        mAllEffects.erase(
            std::remove_if(mAllEffects.begin(), mAllEffects.end(), [&](auto& e) { return e.get() == tracker; }));
    }

    void StopTracking(BaseMemo* tracker) {
        mRunningTracker.erase(std::remove(mRunningTracker.begin(), mRunningTracker.end(), tracker));
    }

   private:
    std::vector<std::unique_ptr<Effect>> mAllEffects;
    std::vector<std::weak_ptr<BaseMemo>> mAllMemos;
    std::vector<Tracker*> mRunningTracker;
    std::vector<Effect*> mScheduledEffects;
    int32_t mNumUntrackScopes = 0;
};

// impl
template <typename T>
inline T Signal<T>::Get(Context& ctx) {
    ctx.Track(shared_from_this());
    return mValue;
}

template <typename T>
inline void Signal<T>::Set(Context& ctx, T value) {
    mValue = value;
    ctx.MarkChanged(shared_from_this());
}

template <typename T>
inline T Memo<T>::Get(Context& ctx) const {
    if (mDirty) {
        mDependencies.clear();
        ctx.PushTracker(this);
        mValue = mMemoCallback(ctx, mValue);
        ctx.PopTracker();
    }
    return mValue;
}

inline void Effect::Run(Context& ctx) {
    Cleanup(ctx);
    ctx.PushTracker(this);
    mCallback(ctx);
    ctx.PopTracker();
}

inline void Effect::Cleanup(Context& ctx) {
    for (const auto& cleanup : mCleanup) {
        cleanup(ctx);
    }
    mCleanup.clear();
    mDependencies.clear();
}

inline UntrackScope::UntrackScope(Context& ctx) : ctx(ctx) {
    ctx.mNumUntrackScopes++;
}
inline UntrackScope::~UntrackScope() {
    ctx.mNumUntrackScopes--;
}

}  // namespace signals