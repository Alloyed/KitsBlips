#include <etl/vector.h>

#include "clapApi/basePlugin.h"

struct StereoAudioBuffer
{
    etl::vector_ext<float> left;
    etl::vector_ext<float> right;
    bool isLeftConstant;
    bool isRightConstant;
};

/* pre-configured for simple stereo effects */
class EffectPlugin: public BasePlugin {
    public:
    virtual void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) = 0;

    // impl
    void ProcessEvent(const clap_event_header_t& event);
    void ProcessRaw(const clap_process_t *process) override;
    const ExtensionMap& GetExtensions() const override; 
};