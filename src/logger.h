#pragma once

#if defined(USE_QT)
#include <QDebug>
#define printf qDebug
#define ILOG(...) qDebug(__VA_ARGS__)
#define WLOG(...) qDebug(__VA_ARGS__)
#define ELOG(...) qDebug(stderr, __VA_ARGS__)
#elif defined(__ANDROID__) && defined(SDL_MAJOR_VERSION)
#define printf(...) SDL_Log(__VA_ARGS__)
#define ILOG(...) SDL_Log(__VA_ARGS__)
#define WLOG(...) SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, __VA_ARGS__)
#define ELOG(...) SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, __VA_ARGS__)
#else
#define ILOG(...) printf(__VA_ARGS__)
#define WLOG(...) printf(__VA_ARGS__)
#define ELOG(...) fprintf(stderr, __VA_ARGS__)
#endif