PROJECT_NAME = vFPC
PROJECT_VERSION = 0.1.0

CC = clang-cl-16
LD = lld-link-16

CFLAGS = \
	-Wno-microsoft --target=i686-pc-windows-msvc /std:c++17 /EHa \
	/I inc /I out /I /opt/curl/include \
	/imsvc /opt/xwin/crt/include /imsvc /opt/xwin/sdk/include/shared \
	/imsvc /opt/xwin/sdk/include/ucrt /imsvc /opt/xwin/sdk/include/um
LDFLAGS = \
	/libpath:/opt/xwin/crt/lib/x86 /libpath:/opt/xwin/sdk/lib/shared/x86 \
	/libpath:/opt/xwin/sdk/lib/ucrt/x86 /libpath:/opt/xwin/sdk/lib/um/x86
LIBRARIES = $(wildcard lib/*) $(wildcard /opt/curl/lib/*)

SOURCES = src/export.cpp src/plugin.cpp src/source.cpp
HEADERS = src/jsonify.hpp src/plugin.hpp src/source.hpp
OBJECTS = $(patsubst src/%.cpp,out/%.obj,$(SOURCES))
DEPENDENTS = $(HEADERS) out/config.h out/ca-bundle.h

out/$(PROJECT_NAME).dll: $(OBJECTS)
	$(LD) /dll /out:$@ $(LDFLAGS) $(LIBRARIES) $^

out/%.obj: src/%.cpp $(DEPENDENTS)
	$(CC) $(CFLAGS) /c /Fo$@ $<

out/config.h: src/config.h.in
	cp $< $@
	sed -i -e "s/PROJECT_NAME/$(PROJECT_NAME)/g" $@
	sed -i -e "s/PROJECT_VERSION/$(PROJECT_VERSION)/g" $@

out/ca-bundle.h: /opt/curl/bin/curl-ca-bundle.crt
	bash tool/gen-ca-bundle.sh < $< > $@

clean:
	rm out/*

.PHONY: clean
