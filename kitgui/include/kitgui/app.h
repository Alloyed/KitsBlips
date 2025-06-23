#pragma once

struct GladGLContext;

namespace kitgui {
class Context;
/**
 * Apps are all the configuration points associated with a single window/context. One process can have multiple apps, as
 * well as multiple of the same app.
 */
class BaseApp {
    friend class Context;

   public:
    BaseApp(Context& mContext) : mContext(mContext) {}
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
     * Triggers on a regular basis. use gl to manipulate the visual state.
     */
    virtual void OnDraw([[maybe_unused]] const GladGLContext& gl) {}

    Context& GetContext() { return mContext; }

   private:
    Context& mContext;
};
}  // namespace kitgui