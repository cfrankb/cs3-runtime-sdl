CC=emcc
INC=
LIBS=
ARGS=-s USE_SDL=2 -s USE_ZLIB=1
PARGS=--preload-file data --emrun -DWASM -O2 -s WASM=1
BPATH=build
BNAME=cs3v2.html
TARGET=$(BPATH)/$(BNAME)
DEPS=$(BPATH)/runtime$(EXT) $(BPATH)/gamemixin$(EXT) $(BPATH)/maparch$(EXT) $(BPATH)/game$(EXT) $(BPATH)/tilesdata$(EXT) $(BPATH)/animator$(EXT) $(BPATH)/level$(EXT) $(BPATH)/actor$(EXT) $(BPATH)/map$(EXT) $(BPATH)/FrameSet$(EXT) $(BPATH)/Frame$(EXT) $(BPATH)/DotArray$(EXT) $(BPATH)/helper$(EXT) $(BPATH)/PngMagic$(EXT) $(BPATH)/FileWrap$(EXT)
EXT=.o

all: $(TARGET)

$(BPATH)/runtime$(EXT): src/runtime.cpp src/runtime.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/gamemixin$(EXT): src/gamemixin.cpp src/gamemixin.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/maparch$(EXT): src/maparch.cpp src/maparch.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/game$(EXT): src/game.cpp src/game.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/tilesdata$(EXT): src/tilesdata.cpp src/tilesdata.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/animator$(EXT): src/animator.cpp src/animator.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/level$(EXT): src/level.cpp src/level.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/actor$(EXT): src/actor.cpp src/actor.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/map$(EXT): src/map.cpp src/map.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/FrameSet$(EXT): src/shared/FrameSet.cpp src/shared/FrameSet.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/Frame$(EXT): src/shared/Frame.cpp src/shared/Frame.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/DotArray$(EXT): src/shared/DotArray.cpp src/shared/DotArray.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/helper$(EXT): src/shared/helper.cpp src/shared/helper.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/PngMagic$(EXT): src/shared/PngMagic.cpp src/shared/PngMagic.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(BPATH)/FileWrap$(EXT): src/shared/FileWrap.cpp src/shared/FileWrap.h
	$(CC) $(ARGS) -c $< $(INC) -o $@

$(TARGET): src/main.cpp $(DEPS)
	$(CC) $(ARGS) src/main.cpp $(DEPS) $(LIBS) $(PARGS) -o $@

clean:
	rm -f $(BPATH)/*