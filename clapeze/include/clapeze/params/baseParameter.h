#pragma once

#include <etl/span.h>
#include <string>
#include "clap/ext/params.h"
#include "clap/id.h"

namespace clapeze {
struct BaseParam {
   public:
    virtual ~BaseParam() = default;
    virtual bool FillInformation(clap_id id, clap_param_info_t* information) const = 0;
    virtual bool ToText(double rawValue, etl::span<char>& outTextBuf) const = 0;
    virtual bool FromText(std::string_view text, double& outRawValue) const = 0;
    virtual double GetRawDefault() const = 0;
    virtual const std::string& GetName() const = 0;
    virtual const std::string& GetTooltip() const = 0;

    const std::string& GetModule() const { return mModule; }
    void SetModule(std::string_view module) { mModule = module; }
    std::string mModule;
};
}  // namespace clapeze
