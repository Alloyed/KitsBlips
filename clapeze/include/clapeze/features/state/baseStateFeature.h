#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <clapeze/basePlugin.h>

namespace clapeze {

class BaseStateFeature : public BaseFeature {
   public:
    static constexpr auto NAME = CLAP_EXT_STATE;
    virtual bool Save(std::ostream& out) = 0;
    virtual bool Load(std::istream& in) = 0;
};
}  // namespace clapeze
