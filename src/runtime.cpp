#include "runtime.h"
#include "game.h"
#include "shared/Frame.h"
#include <cstring>

CRuntime::CRuntime() : CGameMixin()
{
	memset(&m_app, 0, sizeof(App));
}

CRuntime::~CRuntime()
{
    SDL_DestroyTexture(m_app.texture);
    SDL_DestroyRenderer(m_app.renderer);
	SDL_DestroyWindow( m_app.window );
	SDL_Quit();
}

void CRuntime::paint()
{
    CFrame bitmap(WIDTH, HEIGHT);
    switch (m_game->mode())
    {
    case CGame::MODE_INTRO:
    case CGame::MODE_RESTART:
    case CGame::MODE_GAMEOVER:
        drawLevelIntro(bitmap);
        break;
    case CGame::MODE_LEVEL:
        drawScreen(bitmap);
    }

  	SDL_UpdateTexture(m_app.texture, NULL, bitmap.getRGB(), WIDTH * sizeof(uint32_t));
	//SDL_RenderClear(m_app.renderer);
	SDL_RenderCopy(m_app.renderer, m_app.texture, NULL, NULL);
	SDL_RenderPresent(m_app.renderer);  
}

bool CRuntime::SDLInit()
{
	int rendererFlags = SDL_RENDERER_ACCELERATED;
	int windowFlags = SDL_WINDOW_SHOWN;

	if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
	{
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
		return false;
	}
	else
	{
   		m_app.window = SDL_CreateWindow("CS3v2 Runtime",
        	SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 2*WIDTH, 2*HEIGHT, windowFlags);
		if( m_app.window == NULL )
		{
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
			return false;
		}
		else
		{
			atexit(cleanup);
//			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
			m_app.renderer = SDL_CreateRenderer(m_app.window, -1, rendererFlags);
			if (m_app.renderer == nullptr)
			{
				printf("Failed to create renderer: %s\n", SDL_GetError());
				return false;
			}

            m_app.texture = SDL_CreateTexture(m_app.renderer,
                SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
            if ( m_app.texture == nullptr) {
                printf("Failed to create texture: %s\n", SDL_GetError());
				return false;
            } 
		}
	}
	return true;
}

void CRuntime::cleanup()
{
 
}

void CRuntime::run()
{
    mainLoop();
}

void CRuntime::doInput()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_KEYUP:
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					m_joyState[AIM_UP] = KEY_RELEASED;
					break;
				case SDLK_DOWN:
					m_joyState[AIM_DOWN] = KEY_RELEASED;
					break;
				case SDLK_LEFT:
					m_joyState[AIM_LEFT] = KEY_RELEASED;
					break;
				case SDLK_RIGHT:
					m_joyState[AIM_RIGHT] = KEY_RELEASED;
					break;				
				default:
					break;
				}
			break;

			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					m_joyState[AIM_UP] = KEY_PRESSED;
					break;
				case SDLK_DOWN:
					m_joyState[AIM_DOWN] = KEY_PRESSED;
					break;
				case SDLK_LEFT:
					m_joyState[AIM_LEFT] = KEY_PRESSED;
					break;
				case SDLK_RIGHT:
					m_joyState[AIM_RIGHT] = KEY_PRESSED;
					break;				
				default:
					break;
				}
			break;

			case SDL_QUIT:
				exit(0);
				break;

			default:
				break;
		}
	}
}