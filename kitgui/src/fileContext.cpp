#include "fileContext.h"

#include <optional>
#include "kitgui/context.h"

namespace kitgui {

FileContext::FileContext(Context& context) : mContext(context) {}

std::string* FileContext::getOrLoadFileByName(const std::string& filename, Magnum::InputFileCallbackPolicy policy) {
    // this is a hint from the caller that this file is no longer being used, let's uncache if necessary
    if (policy == Magnum::InputFileCallbackPolicy::Close) {
        auto found = mFiles.find(filename);
        if (found != mFiles.end()) {
            mFiles.erase(found);
        }
        return nullptr;
    }

    // if this file is cached, return that
    auto found = mFiles.find(filename);
    if (found != mFiles.end()) {
        return found->second.get();
    }

    // load file
    if (!mContext.mApp) {
        return nullptr;
    }
    std::optional<std::string> fileData = mContext.mApp->OnFileLoadRequest(filename);
    if (!fileData) {
        return nullptr;
    }

    if (policy == Magnum::InputFileCallbackPolicy::LoadTemporary) {
        // TODO: we should mark this file for later garbage collection
    }
    auto cachedData = std::make_unique<std::string>(std::move(*fileData));
    auto [iter, inserted] = mFiles.emplace(filename, std::move(cachedData));
    return iter->second.get();
}

}  // namespace kitgui
