#include "clapeze/features/params/enumParametersFeature.h"

namespace clapeze::params {

EnumMainHandle::EnumMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain)
    : mValues(numParams, 0.0f), mModulations(numParams, 0.0f), mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}

double EnumMainHandle::GetRawValue(clap_id id) const {
    if (id >= mValues.size()) {
        return 0.0f;
    }
    return mValues[id];
}

void EnumMainHandle::SetRawValue(clap_id id, double newValue) {
    if (id < mValues.size()) {
        mValues[id] = newValue;
        mMainToAudio.push({ChangeType::SetValue, id, newValue});
    }
}

void EnumMainHandle::FlushFromAudio() {
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

void EnumMainHandle::StartGesture(clap_id id) {
    mMainToAudio.push({ChangeType::StartGesture, id, 0.0});
}

void EnumMainHandle::StopGesture(clap_id id) {
    mMainToAudio.push({ChangeType::StopGesture, id, 0.0});
}

}  // namespace clapeze::params