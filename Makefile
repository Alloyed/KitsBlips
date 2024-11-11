.PHONY: *
all: build

prep:
	mkdir -p daisy/build
	mkdir -p vcvrack/build
	mkdir -p juce/build

configure:
	cmake -B daisy/build daisy -DCMAKE_BUILD_TYPE=MinSizeRel
	cmake -B vcvrack/build vcvrack -DCMAKE_BUILD_TYPE=MinSizeRel
	cmake -B juce/build juce -DCMAKE_BUILD_TYPE=MinSizeRel

build:
	cmake --build daisy/build --config MinSizeRel
	cmake --build vcvrack/build --config MinSizeRel
	cmake --build juce/build --config MinSizeRel

install:
	cmake --install vcvrack/build --prefix vcvrack/build/dist
	
clean:
	rm -rf daisy/build
	rm -rf vcvrack/build
	rm -rf juce/build