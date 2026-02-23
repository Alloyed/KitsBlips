#pragma once

#include <etl/span.h>
#include <string>
#include "clap/ext/params.h"

namespace clapeze {
/**
 * Parameters are values that affect the sound of your plugin.
 * They are accessible from the audio thread, main thread, and
 * the DAW, if it provides a UI for it. They are also automatically
 * serialized by the built-in state systems.
 */
struct BaseParam {
   public:
    virtual ~BaseParam() = default;

    // used in clap_param_info_t
    /** Returns a set of associated flags with the parameter. */
    virtual clap_param_info_flags GetFlags() const = 0;
    /** Returns the mininum raw value for this parameter. */
    virtual double GetRawMin() const = 0;
    /** Returns the maximum raw value for this parameter. */
    virtual double GetRawMax() const = 0;
    /** Returns the default raw value for this parameter. */
    virtual double GetRawDefault() const = 0;
    /** Return a display name for the parameter. */
    virtual const std::string& GetName() const = 0;
    /** Return the inner module/category that the parameter belongs to. Rarely used by daws. */
    virtual const std::string& GetModule() const {
        static const std::string kEmpty{};
        return kEmpty;
    }

    // used in framework
    /** Turn a raw value into a textual representation */
    virtual bool ToText(double rawValue, etl::span<char>& outTextBuf) const = 0;
    /** Take a textual representation and return the value that it represents. */
    virtual bool FromText(std::string_view text, double& outRawValue) const = 0;
    /** Return a stable string key representation of the parameter. Used for serialization. */
    virtual const std::string& GetKey() const = 0;
    /** Return short description of the parameter (for tooltips!). Not used by daws. */
    virtual const std::string& GetTooltip() const {
        static const std::string kEmpty{};
        return kEmpty;
    }
    // TODO: if set, also trigger an appropriate rescan
    // virtual bool IsHidden() const = 0;
    // virtual bool SetHidden(bool) = 0;
    // virtual void SetName(std::string_view) = 0;
};
}  // namespace clapeze
