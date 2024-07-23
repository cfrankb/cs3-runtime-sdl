CXX=em++
INC=
LIBS=-lopenal -lidbfs.js
CXXFLAGS=-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_ZLIB=1 -O2
PARGS=--preload-file data --emrun -O2 -sWASM=1 -sALLOW_MEMORY_GROWTH -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -sEXPORTED_FUNCTIONS=_mute,_savegame,_main
BPATH=build
BNAME=cs3v2.html
TARGET=$(BPATH)/$(BNAME)
TEMPLATE=--shell-file src/template/body.html
DEPS=$(BPATH)/runtime$(EXT) $(BPATH)/gamemixin$(EXT) $(BPATH)/main$(EXT) $(BPATH)/maparch$(EXT) $(BPATH)/game$(EXT) $(BPATH)/tilesdata$(EXT) $(BPATH)/animator$(EXT) $(BPATH)/level$(EXT) $(BPATH)/actor$(EXT) $(BPATH)/map$(EXT) $(BPATH)/FrameSet$(EXT) $(BPATH)/Frame$(EXT) $(BPATH)/DotArray$(EXT) $(BPATH)/helper$(EXT) $(BPATH)/PngMagic$(EXT) $(BPATH)/FileWrap$(EXT) $(BPATH)/sn_sdl$(EXT) $(BPATH)/mu_sdl$(EXT)
EXT=.o

all: $(TARGET)

$(BPATH)/runtime$(EXT): src/runtime.cpp src/runtime.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/gamemixin$(EXT): src/gamemixin.cpp src/gamemixin.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/main$(EXT): src/main.cpp
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/maparch$(EXT): src/maparch.cpp src/maparch.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/game$(EXT): src/game.cpp src/game.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/tilesdata$(EXT): src/tilesdata.cpp src/tilesdata.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/animator$(EXT): src/animator.cpp src/animator.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/level$(EXT): src/level.cpp src/level.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/actor$(EXT): src/actor.cpp src/actor.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/map$(EXT): src/map.cpp src/map.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/FrameSet$(EXT): src/shared/FrameSet.cpp src/shared/FrameSet.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/Frame$(EXT): src/shared/Frame.cpp src/shared/Frame.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/DotArray$(EXT): src/shared/DotArray.cpp src/shared/DotArray.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/helper$(EXT): src/shared/helper.cpp src/shared/helper.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/PngMagic$(EXT): src/shared/PngMagic.cpp src/shared/PngMagic.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/FileWrap$(EXT): src/shared/FileWrap.cpp src/shared/FileWrap.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/sn_sdl$(EXT): src/shared/implementers/sn_sdl.cpp src/shared/implementers/sn_sdl.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(BPATH)/mu_sdl$(EXT): src/shared/implementers/mu_sdl.cpp src/shared/implementers/mu_sdl.h
	$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@

$(TARGET): $(DEPS)
	$(CXX) $(CXXFLAGS) $(DEPS) $(LIBS) $(PARGS) -o $@ $(TEMPLATE)

clean:
	rm -rf $(BPATH)/*