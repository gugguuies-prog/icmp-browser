/**
 * @file thread_compat.h
 * @brief Portable threading abstraction layer for Windows and POSIX systems
 * 
 * Provides a unified interface for:
 * - Threads (pthread_t / HANDLE)
 * - Mutexes (pthread_mutex_t / CRITICAL_SECTION)
 * - Condition variables (pthread_cond_t / CONDITION_VARIABLE)
 */

#ifndef THREAD_COMPAT_H
#define THREAD_COMPAT_H

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
#endif

/* ============================================================================
 * Thread Types
 * ============================================================================ */

#ifdef _WIN32
    typedef HANDLE thread_t;
    typedef CRITICAL_SECTION mutex_t;
    typedef CONDITION_VARIABLE cond_t;
    typedef unsigned (__stdcall *thread_func_t)(void*);
    #define THREAD_RETURN_TYPE unsigned __stdcall
    #define THREAD_RETURN_VALUE 0
#else
    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;
    typedef pthread_cond_t cond_t;
    typedef void* (*thread_func_t)(void*);
    #define THREAD_RETURN_TYPE void*
    #define THREAD_RETURN_VALUE NULL
#endif

/* ============================================================================
 * Thread Functions
 * ============================================================================ */

/**
 * Create a new thread
 * @param thread Pointer to thread handle
 * @param func Thread function
 * @param arg Argument passed to thread function
 * @return 0 on success, -1 on error
 */
static inline int thread_create(thread_t *thread, thread_func_t func, void *arg) {
#ifdef _WIN32
    *thread = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

/**
 * Wait for thread to terminate
 * @param thread Thread handle
 * @return 0 on success, -1 on error
 */
static inline int thread_join(thread_t thread) {
#ifdef _WIN32
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#else
    return pthread_join(thread, NULL);
#endif
}

/**
 * Detach a thread (runs independently)
 * @param thread Thread handle
 * @return 0 on success, -1 on error
 */
static inline int thread_detach(thread_t thread) {
#ifdef _WIN32
    return CloseHandle(thread) ? 0 : -1;
#else
    return pthread_detach(thread);
#endif
}

/**
 * Exit current thread
 * @param retval Return value (ignored on Windows)
 */
static inline void thread_exit(void *retval) {
#ifdef _WIN32
    _endthreadex(0);
#else
    pthread_exit(retval);
#endif
}

/* ============================================================================
 * Mutex Functions
 * ============================================================================ */

/**
 * Initialize a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on error
 */
static inline int mutex_init(mutex_t *mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

/**
 * Destroy a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on error
 */
static inline int mutex_destroy(mutex_t *mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_destroy(mutex);
#endif
}

/**
 * Lock a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on error
 */
static inline int mutex_lock(mutex_t *mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex);
#endif
}

/**
 * Unlock a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on error
 */
static inline int mutex_unlock(mutex_t *mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

/**
 * Try to lock a mutex (non-blocking)
 * @param mutex Pointer to mutex
 * @return 0 if locked, -1 if already locked
 */
static inline int mutex_trylock(mutex_t *mutex) {
#ifdef _WIN32
    return TryEnterCriticalSection(mutex) ? 0 : -1;
#else
    return pthread_mutex_trylock(mutex);
#endif
}

/* ============================================================================
 * Condition Variable Functions
 * ============================================================================ */

/**
 * Initialize a condition variable
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on error
 */
static inline int cond_init(cond_t *cond) {
#ifdef _WIN32
    InitializeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_init(cond, NULL);
#endif
}

/**
 * Destroy a condition variable
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on error
 */
static inline int cond_destroy(cond_t *cond) {
#ifdef _WIN32
    /* Windows doesn't need explicit destruction */
    (void)cond;
    return 0;
#else
    return pthread_cond_destroy(cond);
#endif
}

/**
 * Wait on a condition variable
 * @param cond Pointer to condition variable
 * @param mutex Pointer to associated mutex
 * @return 0 on success, -1 on error
 */
static inline int cond_wait(cond_t *cond, mutex_t *mutex) {
#ifdef _WIN32
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
#else
    return pthread_cond_wait(cond, mutex);
#endif
}

/**
 * Signal one waiting thread
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on error
 */
static inline int cond_signal(cond_t *cond) {
#ifdef _WIN32
    WakeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_signal(cond);
#endif
}

/**
 * Signal all waiting threads
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on error
 */
static inline int cond_broadcast(cond_t *cond) {
#ifdef _WIN32
    WakeAllConditionVariable(cond);
    return 0;
#else
    return pthread_cond_broadcast(cond);
#endif
}

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * Define a thread function with proper signature
 * Usage: THREAD_FUNC(my_function) { ... }
 */
#define THREAD_FUNC(name) THREAD_RETURN_TYPE name(void *arg)

/**
 * Return from a thread function
 * Usage: THREAD_RETURN;
 */
#define THREAD_RETURN return THREAD_RETURN_VALUE

#endif /* THREAD_COMPAT_H */
