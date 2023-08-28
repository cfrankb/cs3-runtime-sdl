#include "gamemixin.h"
#include <SDL2/SDL.h>

class CRuntime: public CGameMixin
{
public:
    CRuntime();
    virtual ~CRuntime();

    void paint();
    void run();
    bool SDLInit();
    void doInput();

protected:
    static void cleanup();

    typedef struct {
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_Texture * texture;
    } App;

    App m_app;    

};