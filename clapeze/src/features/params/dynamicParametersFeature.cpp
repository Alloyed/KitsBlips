#include "clapeze/features/params/dynamicParametersFeature.h"

namespace clapeze::params {

DynamicProcessorHandle::DynamicProcessorHandle(std::vector<std::unique_ptr<BaseParam>>& ref,
                                               size_t numParams,
                                               Queue& mainToAudio,
                                               Queue& audioToMain)
    : mParamsRef(ref),
      mValues(numParams, 0.0f),
      mModulations(numParams, 0.0f),
      mMainToAudio(mainToAudio),
      mAudioToMain(audioToMain) {}

bool DynamicProcessorHandle::ProcessEvent(const clap_event_header_t& event) {
    if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
        switch (event.type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const auto& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                SetRawValue(paramChange.param_id, paramChange.value);
                return true;
            }
            case CLAP_EVENT_PARAM_MOD: {
                const auto& paramChange = reinterpret_cast<const clap_event_param_mod_t&>(event);
                SetRawModulation(paramChange.param_id, paramChange.amount);
                return true;
            }
            default: {
                break;
            }
        }
    }
    return false;
}

void DynamicProcessorHandle::FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) {
    (void)processor;
    // Send events queued from us to the host
    // Since these all happened on an independent thread, they do not have sample-accurate timing; we'll just
    // send them at the front of the queue.
    Change change;
    while (mMainToAudio.pop(change)) {
        switch (change.type) {
            case ChangeType::StartGesture:
            case ChangeType::StopGesture: {
                clap_event_param_gesture_t event = {};
                event.header.size = sizeof(event);
                event.header.time = 0;
                event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                event.header.type =
                    static_cast<uint16_t>(change.type == ChangeType::StartGesture ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                                  : CLAP_EVENT_PARAM_GESTURE_END);
                event.header.flags = 0;
                event.param_id = change.id;
                out->try_push(out, &event.header);
                break;
            }
            case ChangeType::SetValue: {
                clap_id index = change.id;
                mValues[index] = change.value;

                clap_event_param_value_t event = {};
                event.header.size = sizeof(event);
                event.header.time = 0;
                event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                event.header.type = static_cast<uint16_t>(CLAP_EVENT_PARAM_VALUE);
                event.header.flags = 0;
                event.param_id = index;
                event.cookie = nullptr;
                event.note_id = -1;
                event.port_index = -1;
                event.channel = -1;
                event.key = -1;
                event.value = change.value;

                out->try_push(out, &event.header);
                break;
            }
            case ChangeType::SetModulation: {
                // this is if the plugin sets the modulation, i don't think this happens??
                assert(false);
                break;
            }
        }
    }
}
double DynamicProcessorHandle::GetRawValue(clap_id id) const {
    if (id < mValues.size()) {
        return mValues[id];
    }
    return 0.0f;
}
void DynamicProcessorHandle::SetRawValue(clap_id id, double newValue) {
    if (id < mValues.size()) {
        mValues[id] = newValue;
        mAudioToMain.push({ChangeType::SetValue, id, newValue});
    }
}

double DynamicProcessorHandle::GetRawModulation(clap_id id) const {
    if (id < mModulations.size()) {
        return mModulations[id];
    }
    return 0.0f;
}
void DynamicProcessorHandle::SetRawModulation(clap_id id, double newModulation) {
    if (id < mModulations.size()) {
        mModulations[id] = newModulation;
        mAudioToMain.push({ChangeType::SetModulation, id, newModulation});
    }
}

DynamicMainHandle::DynamicMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain)
    : mValues(numParams, 0.0f), mModulations(numParams, 0.0f), mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}

double DynamicMainHandle::GetRawValue(clap_id id) const {
    if (id >= mValues.size()) {
        return 0.0f;
    }
    return mValues[id];
}

void DynamicMainHandle::SetRawValue(clap_id id, double newValue) {
    if (id < mValues.size()) {
        mValues[id] = newValue;
        mMainToAudio.push({ChangeType::SetValue, id, newValue});
    }
}

void DynamicMainHandle::StartGesture(clap_id id) {
    mMainToAudio.push({ChangeType::StartGesture, id, 0.0});
}

void DynamicMainHandle::StopGesture(clap_id id) {
    mMainToAudio.push({ChangeType::StopGesture, id, 0.0});
}

void DynamicMainHandle::FlushFromAudio() {
    Change change;
    while (mAudioToMain.pop(change)) {
        clap_id index = change.id;
        switch (change.type) {
            case ChangeType::SetValue: {
                mValues[index] = change.value;
                break;
            }
            case ChangeType::SetModulation: {
                mModulations[index] = change.value;
                break;
            }
            case ChangeType::StartGesture:
            case ChangeType::StopGesture: {
                break;
            }
        }
    }
}

}  // namespace clapeze::params