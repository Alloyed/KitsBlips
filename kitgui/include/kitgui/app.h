#pragma once

namespace kitgui {
class Context;
class FileContext;
/**
 * Apps are all the configuration points associated with a single window/context. One process can have multiple apps, as
 * well as multiple of the same app. All openGL resources must be unique to a given App, and only can be created/used
 * from an app override point, or a constructor/destructor
 */
class BaseApp {
    friend class Context;
    friend class FileContext;

   public:
    explicit BaseApp(Context& mContext) : mContext(mContext) {}
    virtual ~BaseApp() = default;
    // override to customize
   protected:
    /**
     * Triggers before the first active frame of the underlying context. If a context is active, its window will be
     * visible and update/draw events will be sent.
     */
    virtual void OnActivate() {}
    /**
     * Triggers after the last active frame of the underlying context. contexts can be deactivated by the associated
     * window closing or minimizing.
     */
    virtual void OnDeactivate() {}
    /**
     * Triggers on a regular basis.
     */
    virtual void OnUpdate() {}
    /**
     * Triggers on a regular basis. happens after the regular update
     */
    virtual void OnGuiUpdate() {}
    /**
     * Triggers on a regular basis. use gl to manipulate the visual state.
     */
    virtual void OnDraw() {}

    Context& GetContext() { return mContext; }

   private:
    Context& mContext;
};
}  // namespace kitgui
