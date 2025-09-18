#pragma once

#if defined(USE_QT)
#include <QtLogging>
#define printf qDebug
#define LOGI(...) qInfo(__VA_ARGS__)
#define LOGW(...) qWarning(__VA_ARGS__)
#define LOGE(...) qCritical(__VA_ARGS__)
#define LOGF(...) qFatal(__VA_ARGS__)
#elif defined(__ANDROID__)
#include <android/log.h> // For logging
#define printf(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)
#elif defined(__ANDROID__) && defined(SDL_MAJOR_VERSION)
#define printf(...) SDL_Log(__VA_ARGS__)
#define LOGI(...) SDL_Log(__VA_ARGS__)
#define LOGW(...) SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, __VA_ARGS__)
#define LOGE(...) SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, __VA_ARGS__)
#define LOGF(...) SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, __VA_ARGS__)
#else
#define LOGI(...) printf(__VA_ARGS__)
#define LOGW(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#define LOGF(...)                 \
    fprintf(stderr, __VA_ARGS__); \
    exit(1)
#endif
