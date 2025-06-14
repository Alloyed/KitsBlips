.PHONY: *
all: build

prep:
	mkdir -p daisy/build
	mkdir -p vcvrack/build
	mkdir -p clap/build

configure:
	cmake -B daisy/build daisy -DCMAKE_BUILD_TYPE=MinSizeRel
	cmake -B vcvrack/build vcvrack -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake -B clap/build juce -DCMAKE_BUILD_TYPE=RelWithDebInfo

build:
	cmake --build daisy/build --config MinSizeRel
	cmake --build vcvrack/build --config RelWithDebInfo
	cmake --build clap/build --config RelWithDebInfo

install:
	cmake --install vcvrack/build --prefix vcvrack/build/dist
	
clean:
	rm -rf daisy/build
	rm -rf vcvrack/build
	rm -rf clap/build