CC=emcc
INC=-Isrc/zlib/
LIBS=
ARGS=-s USE_SDL=2 -s USE_ZLIB=1 -s WASM=1
PARGS=--preload-file data
BPATH=build
BNAME=cs3v2.html
TARGET=$(BPATH)/$(BNAME)

all: $(TARGET)

$(BPATH)/runtime.o: src/runtime.cpp
	$(CC) -c src/runtime.cpp $(INC) -o $@

$(BPATH)/gamemixin.o: src/gamemixin.cpp
	$(CC) -c src/gamemixin.cpp $(INC) -o $@

$(BPATH)/maparch.o: src/maparch.cpp
	$(CC) -c src/maparch.cpp $(INC) -o $@

$(BPATH)/game.o: src/game.cpp
	$(CC) -c src/game.cpp $(INC) -o $@

$(BPATH)/tilesdata.o: src/tilesdata.cpp
	$(CC) -c src/tilesdata.cpp $(INC) -o $@

$(BPATH)/animator.o: src/animator.cpp
	$(CC) -c src/animator.cpp $(INC) -o $@

$(BPATH)/level.o: src/level.cpp
	$(CC) -c src/level.cpp $(INC) -o $@

$(BPATH)/actor.o: src/actor.cpp
	$(CC) -c src/actor.cpp $(INC) -o $@

$(BPATH)/map.o: src/map.cpp
	$(CC) -c src/map.cpp $(INC) -o $@

$(BPATH)/FrameSet.o: src/shared/FrameSet.cpp
	$(CC) -c src/shared/FrameSet.cpp $(INC) -o $@

$(BPATH)/Frame.o: src/shared/Frame.cpp
	$(CC) -c src/shared/Frame.cpp $(INC) -o $@

$(BPATH)/DotArray.o: src/shared/DotArray.cpp
	$(CC) -c src/shared/DotArray.cpp $(INC) -o $@

$(BPATH)/helper.o: src/shared/helper.cpp
	$(CC) -c src/shared/helper.cpp $(INC) -o $@

$(BPATH)/PngMagic.o: src/shared/PngMagic.cpp
	$(CC) -c src/shared/PngMagic.cpp $(INC) -o $@

$(BPATH)/FileWrap.o: src/shared/FileWrap.cpp
	$(CC) -c src/shared/FileWrap.cpp $(INC) -o $@

$(TARGET): src/main.cpp $(BPATH)/runtime.o $(BPATH)/gamemixin.o $(BPATH)/maparch.o $(BPATH)/game.o $(BPATH)/tilesdata.o $(BPATH)/animator.o $(BPATH)/level.o $(BPATH)/actor.o $(BPATH)/map.o $(BPATH)/FrameSet.o $(BPATH)/Frame.o $(BPATH)/DotArray.o $(BPATH)/helper.o $(BPATH)/PngMagic.o $(BPATH)/FileWrap.o
	$(CC) $(ARGS) src/main.cpp $(BPATH)/runtime.o $(BPATH)/gamemixin.o $(BPATH)/maparch.o $(BPATH)/game.o $(BPATH)/tilesdata.o $(BPATH)/animator.o $(BPATH)/level.o $(BPATH)/actor.o $(BPATH)/map.o $(BPATH)/FrameSet.o $(BPATH)/Frame.o $(BPATH)/DotArray.o $(BPATH)/helper.o $(BPATH)/PngMagic.o $(BPATH)/FileWrap.o $(LIBS) $(PARGS) -o $@

clean:
	rm -f $(BPATH)/*