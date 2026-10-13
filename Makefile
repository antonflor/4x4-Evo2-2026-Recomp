# evo2026 renderer — cross-build a 32-bit Windows DLL with MinGW-w64
CXX      = i686-w64-mingw32-g++
WINDRES  = i686-w64-mingw32-windres
CXXFLAGS = -std=c++20 -O2 -DNDEBUG -D_WINDOWS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE \
           -Irenderer -Irenderer/include -Ithird_party -Wno-write-strings
LDFLAGS  = -shared -static -static-libgcc -static-libstdc++ -s
SRC      = $(wildcard renderer/*.cxx)
OBJ      = $(patsubst renderer/%.cxx,build/%.o,$(SRC)) build/res.o
OUT      = build/evo2026.dll

all: $(OUT)
build/%.o: renderer/%.cxx $(wildcard renderer/*.hxx)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/res.o: renderer/App.Resources.rc
	@mkdir -p build
	$(WINDRES) -Irenderer $< -O coff -o $@
$(OUT): $(OBJ) renderer/Renderer.Module.def
	$(CXX) -o $@ $(OBJ) renderer/Renderer.Module.def $(LDFLAGS) -ld3d9
clean:
	rm -rf build
