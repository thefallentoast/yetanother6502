#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#ifdef _WIN32
    #include <windows.h>
    static inline void sleep_ms(int ms) {
        Sleep(ms);
    }
#else
    #include <unistd.h>
    static inline void sleep_ms(int ms) {
        usleep(ms * 1000);
    }
#endif

#endif /* PLATFORM_TIME_H */