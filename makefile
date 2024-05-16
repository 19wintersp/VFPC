PROJECT_NAME = vFPC
PROJECT_VERSION = 0.1.0

CC = clang-cl-16
LD = lld-link-16

CC_TEST = clang++
LD_TEST = clang++

ICAO_AIRCRAFT ?= res/ICAO_Aircraft.txt

CFLAGS = \
	-Wno-microsoft --target=i686-pc-windows-msvc /std:c++17 /EHa \
	/I inc /I out /I /opt/curl/include \
	/imsvc /opt/xwin/crt/include /imsvc /opt/xwin/sdk/include/shared \
	/imsvc /opt/xwin/sdk/include/ucrt /imsvc /opt/xwin/sdk/include/um
LDFLAGS = \
	/libpath:/opt/xwin/crt/lib/x86 /libpath:/opt/xwin/sdk/lib/shared/x86 \
	/libpath:/opt/xwin/sdk/lib/ucrt/x86 /libpath:/opt/xwin/sdk/lib/um/x86
LIBRARIES = $(wildcard lib/*) $(wildcard /opt/curl/lib/*)

CFLAGS_TEST = -c -DVFPC_STANDALONE --std=c++17 -I inc -I out

SOURCES = src/check.cpp src/export.cpp src/flightplan.cpp src/plugin.cpp src/source.cpp
HEADERS = src/check.hpp src/flightplan.hpp src/jsonify.hpp src/plugin.hpp src/source.hpp
OBJECTS = $(patsubst src/%.cpp,out/%.obj,$(SOURCES))
DEPENDENTS = $(HEADERS) out/config.h out/ca-bundle.h

SOURCES_TEST = src/check.cpp src/flightplan.cpp src/source.cpp src/test.cpp
HEADERS_TEST = src/check.hpp src/flightplan.hpp src/jsonify.hpp src/source.hpp
OBJECTS_TEST = $(patsubst src/%.cpp,out/%.o,$(SOURCES_TEST))
DEPENDENTS_TEST = $(HEADERS) out/config.h out/icao-aircraft.hpp

out/$(PROJECT_NAME).dll: $(OBJECTS)
	$(LD) /dll /out:$@ $(LDFLAGS) $(LIBRARIES) $^

out/$(PROJECT_NAME): $(OBJECTS_TEST)
	$(LD_TEST) -o $@ $^

out/%.obj: src/%.cpp $(DEPENDENTS)
	$(CC) $(CFLAGS) /c /Fo$@ $<

out/%.o: src/%.cpp $(DEPENDENTS_TEST)
	$(CC_TEST) $(CFLAGS_TEST) -o $@ $<

out/config.h: src/config.h.in
	cp $< $@
	sed -i -e "s/PROJECT_NAME/$(PROJECT_NAME)/g" $@
	sed -i -e "s/PROJECT_VERSION/$(PROJECT_VERSION)/g" $@

out/ca-bundle.h: /opt/curl/bin/curl-ca-bundle.crt
	bash tool/gen-ca-bundle.sh < $< > $@

out/icao-aircraft.hpp: src/icao-aircraft.hpp.in $(ICAO_AIRCRAFT)
	cp $< $@
	bash -c "bash tool/gen-icao-aircraft.sh < $(ICAO_AIRCRAFT) >> $@"

clean:
	rm out/*

test: out/$(PROJECT_NAME)

.PHONY: clean test
