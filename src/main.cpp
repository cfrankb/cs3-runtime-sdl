#include <unistd.h>
#include <stdio.h>
#include "runtime.h"
#include "maparch.h"
#include <emscripten.h>

void loop_handler(void *arg)
{
	CRuntime * runtime = reinterpret_cast<CRuntime*>(arg);
	usleep(1000/24*1000);
	runtime->doInput();
	runtime->paint();
	runtime->run();
}

int main( int argc, char* args[] )
{
	CRuntime runtime;
	CMapArch maparch;
	if (!maparch.read("data/levels.mapz")) {
		printf("failed to read maparch: %s\n", maparch.lastError());
	}

	runtime.init(&maparch, 0);
	runtime.SDLInit();
	runtime.paint();
 	emscripten_set_main_loop_arg(loop_handler, &runtime, -1, 1);

	return 0;
}
