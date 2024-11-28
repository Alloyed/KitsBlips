#include <osdialog.h>
#include "plugin.hpp"

struct Dance : Module {
    enum ParamId { PARAMS_LEN };
    enum InputId {
        PNG_1_INPUT,
        PNG_2_INPUT,
        PNG_3_INPUT,
        PNG_4_INPUT,
        INPUTS_LEN
    };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Dance() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(PNG_1_INPUT, "Trigger png 1");
        configInput(PNG_2_INPUT, "Trigger png 2");
        configInput(PNG_3_INPUT, "Trigger png 3");
        configInput(PNG_4_INPUT, "Trigger png 4");
    }

    void process(const ProcessArgs& args) override {
        for (size_t index = PNG_1_INPUT; index <= PNG_4_INPUT; ++index) {
            if ((inputs[index].getVoltage() * 0.1f) > 0.25f) {
                mLastTrigger = index;
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_t* array = json_array();
        for (size_t index = PNG_1_INPUT; index <= PNG_4_INPUT; ++index) {
            json_array_append_new(array, json_string(mFiles[index].c_str()));
        }
        json_object_set_new(rootJ, "images", array);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* imagesJ = json_object_get(rootJ, "images");
        if (imagesJ) {
            size_t index;
            json_t* value;
            json_array_foreach(imagesJ, index, value) {
                if (index < 4) {
                    mFiles[index] = json_string_value(value);
                    mFilesChanged = true;
                }
            }
        }
    }

    void setImagePath(size_t index, const char* path) {
        mFiles[index] = path;
        mFilesChanged = true;
    }

    std::string mFiles[4]{};
    bool mFilesChanged{true};

    size_t mLastTrigger{0};
};

struct PngViewerWidget : LedDisplay {
    Dance* module;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1 || module == nullptr) {
            return;
        }
        auto& vg = args.vg;

        if (module->mFilesChanged) {
            for (size_t index = 0; index < 4; ++index) {
                nvgDeleteImage(vg, mImageHandles[index]);
                const std::string path = module->mFiles[index];
                if (!path.empty()) {
                    mImageHandles[index] = nvgCreateImage(vg, path.c_str(), 0);
                } else {
                    mImageHandles[index] = 0;
                }
            }
            module->mFilesChanged = false;
        }

        int currentImage = mImageHandles[module->mLastTrigger];
        if (currentImage != 0) {
            // draw png
            int imgw, imgh;
            nvgImageSize(vg, currentImage, &imgw, &imgh);

            float cx = box.size.x * 0.5f;
            float cy = box.size.y * 0.5f;
            float scale = box.size.x / static_cast<float>(imgw);
            float w = imgw * scale;
            float h = imgh * scale;

            auto imgPaint = nvgImagePattern(vg, cx - (w / 2), cy - (h / 2), w,
                                            h, 0.0f, currentImage, 1.0f);
            nvgBeginPath(vg);
            nvgRect(vg, 0.0, 0.0, box.size.x, box.size.y);
            nvgFillPaint(vg, imgPaint);
            nvgFill(vg);
        }
    }

    int mImageHandles[4]{0};
};

struct DanceWidget : ModuleWidget {
    DanceWidget(Dance* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/dance.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(
            createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH,
                                          RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.912, 119.094)),
                                                 module, Dance::PNG_1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(16.184, 119.094)),
                                                 module, Dance::PNG_2_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24.456, 119.094)),
                                                 module, Dance::PNG_3_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32.728, 119.094)),
                                                 module, Dance::PNG_4_INPUT));

        PngViewerWidget* display =
            createWidget<PngViewerWidget>(mm2px(Vec(6.162, 39.24)));
        display->box.size = mm2px(Vec(28.317, 50.021));
        display->module = module;
        addChild(display);
    }

    void loadImage(int index) {
        osdialog_filters* filters = osdialog_filters_parse("Image:png");
        DEFER({ osdialog_filters_free(filters); });

        char* pathC = osdialog_file(OSDIALOG_OPEN,
                                    !mLastDir.empty() ? mLastDir.c_str() : NULL,
                                    NULL, filters);
        if (!pathC) {
            // Fail silently
            return;
        }
        Dance* module = getModule<Dance>();
        module->setImagePath(index, pathC);
        mLastDir = system::getDirectory(pathC);
        std::free(pathC);
    }

    void appendContextMenu(Menu* menu) override {
        Dance* module = getModule<Dance>();

        menu->addChild(new MenuSeparator);

        for (int i = 0; i < 4; ++i) {
            menu->addChild(
                createMenuItem(string::f("Load image %d", i + 1),
                               system::getFilename(module->mFiles[i]),
                               [this, i]() { loadImage(i); }));
        }
    }

    std::string mLastDir{};
};

Model* modelDance = createModel<Dance, DanceWidget>("dance");