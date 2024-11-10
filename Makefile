.PHONY: *
all: build

prep:
	mkdir -p daisy/build
	mkdir -p vcvrack/build
	mkdir -p juce/build

configure:
	cmake -B daisy/build daisy -DCMAKE_BUILD_TYPE=Release
	cmake -B vcvrack/build vcvrack -DCMAKE_BUILD_TYPE=Release -DRACK_SDK_DIR="$(abspath sdk/Rack-SDK-2.5.2-lin-x64)"
	cmake -B juce/build juce -DCMAKE_BUILD_TYPE=Release

build:
	cmake --build daisy/build --config Release
	cmake --build vcvrack/build --config Release
	cmake --build juce/build --config Release

install:
	cmake --install vcvrack/build --prefix vcvrack/build/dist
	
clean:
	rm -rf daisy/build
	rm -rf vcvrack/build
	rm -rf juce/build