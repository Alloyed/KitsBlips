#include <kitgui/gfx/image.h>

// IWYU pragma: begin_keep
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// IWYU pragma: end_keep

#include <Corrade/Containers/Array.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>
#include "fileContext.h"
#include "gfx/importerManager.h"
#include "kitgui/context.h"
#include "gfx/imageUtil.h"

using namespace Corrade;
using namespace Magnum;

namespace kitgui {

struct Image::Impl {
    explicit Impl(kitgui::Context& context) : mContext(context) {}
    ~Impl() = default;
    void Load(std::string_view path) {
        if (mLoaded || mLoadError) {
            return;
        }
        Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer =
            ImporterManager().loadAndInstantiate("AnyImageImporter");
        assert(!!importer);

        const auto fileCallback = [](const std::string& filename, Magnum::InputFileCallbackPolicy policy,
                                     void* ctx) -> Containers::Optional<Containers::ArrayView<const char>> {
            std::string* fileData = ((kitgui::FileContext*)ctx)->GetOrLoadFileByName(filename, policy);
            if (fileData == nullptr) {
                return {};
            }
            return Containers::ArrayView<const char>(fileData->data(), fileData->size());
        };

        importer->setFileCallback(fileCallback, (void*)mContext.GetFileContext());
        bool success = importer->openFile({path.data(), path.size()});

        if (!success) {
            mLoadError = true;
            return;
        }

        auto imageData = std::optional<Trade::ImageData2D>{importer->image2D(0)};
        if (!imageData) {
            /*
            kitgui::log::error(fmt::format("cannot load image {}, {} ", textureData->image(),
                                           importer.image2DName(textureData->image())));
            */
            mLoadError = true;
            return;
        }

        /* Configure the texture */
        //texture.setMagnificationFilter(textureData->magnificationFilter())
        //    .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
        //    .setWrapping(textureData->wrapping().xy());

        LoadImageToTexture(*imageData, mTexture);
        mLoaded = true;
    }
    ImTextureID GetId() { return Magnum::ImGuiIntegration::textureId(mTexture); }

    kitgui::Context& mContext;
    Magnum::GL::Texture2D mTexture;
    bool mLoaded = false;
    bool mLoadError = false;
};

Image::Image(kitgui::Context& context) : mImpl(std::make_unique<Image::Impl>(context)) {}
Image::~Image() = default;
void Image::Load(std::string_view path) {
    mImpl->Load(path);
}
ImTextureID Image::GetId() {
    return mImpl->GetId();
}

}  // namespace kitgui