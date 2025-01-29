#include "StackTrace/StackTrace.h"
#include "StackTrace/StaticVector.h"
#include "StackTrace/Utilities.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <thread>


// Detect the OS
// clang-format off
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || defined( _MSC_VER )
    #define USE_WINDOWS
#elif defined( __APPLE__ )
    #define USE_MAC
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
    #define USE_LINUX
#else
    #error Unknown OS
#endif
// clang-format on


// Include system dependent headers
// clang-format off
// Detect the OS and include system dependent headers
#ifdef USE_WINDOWS
    #include <windows.h>
    #include <dbghelp.h>
    #include <DbgHelp.h>
    #include <TlHelp32.h>
    #include <Psapi.h>
    #include <process.h>
    #include <stdio.h>
    #include <tchar.h>
    #pragma comment( lib, "version.lib" ) // for "VerQueryValue"
#else
    #include <dlfcn.h>
    #include <execinfo.h>
    #include <sched.h>
    #include <sys/time.h>
    #include <ctime>
    #include <unistd.h>
    #include <sys/syscall.h>
#endif
#ifdef USE_MAC
    #include <mach-o/dyld.h>
    #include <mach/mach.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #define SIGRTMIN SIGUSR1
    #define SIGRTMAX SIGUSR2
#endif
// clang-format on


extern int thread_callstack_signal;


namespace StackTrace {


/****************************************************************************
 *  Get the native thread handle                                             *
 ****************************************************************************/
std::thread::native_handle_type thisThread()
{
#if defined( USE_LINUX ) || defined( USE_MAC )
    return pthread_self();
#elif defined( USE_WINDOWS )
    return GetCurrentThread();
#else
    return std::thread::native_handle();
#endif
}


/****************************************************************************
 *  Get a list of all active threads                                         *
 *  Note this uses system calls and may not behave on all systems            *
 ****************************************************************************/
#if defined( USE_LINUX ) || defined( USE_MAC )
static volatile std::thread::native_handle_type thread_handle;
static volatile bool thread_id_finished;
static void _activeThreads_signal_handler( int )
{
    auto handle        = StackTrace::thisThread();
    thread_handle      = handle;
    thread_id_finished = true;
}
#endif
#ifdef USE_LINUX
static int get_tid( int pid, const char* line )
{
    char buf2[128] = { 0 };
    int i1         = 0;
    while ( line[i1] == ' ' ) {
        i1++;
    }
    int i2 = i1;
    while ( line[i2] != ' ' ) {
        i2++;
    }
    memcpy( buf2, &line[i1], i2 - i1 );
    buf2[i2 - i1 + 1] = 0;
    int pid2          = atoi( buf2 );
    if ( pid2 != pid )
        return -1;
    i1 = i2;
    while ( line[i1] == ' ' ) {
        i1++;
    }
    i2 = i1;
    while ( line[i2] != ' ' ) {
        i2++;
    }
    memcpy( buf2, &line[i1], i2 - i1 );
    buf2[i2 - i1 + 1] = 0;
    int tid           = atoi( buf2 );
    return tid;
}
#endif
static std::mutex StackTrace_mutex;
std::vector<std::thread::native_handle_type> activeThreads()
{
    std::vector<std::thread::native_handle_type> threads;
#if defined( USE_LINUX )
    // Get the system thread ids
    int N_tid = 0, tid[1024];
    int pid   = getpid();
    char cmd[128];
    sprintf( cmd, "ps -T -p %i", pid );
    auto fun = [&N_tid, &tid, pid]( const char* line ) {
        int id = get_tid( pid, line );
        if ( id != -1 && N_tid < 1024 )
            tid[N_tid++] = id;
    };
    StackTrace::Utilities::exec2( cmd, fun );
    int myid = syscall( SYS_gettid );
    for ( int i = 0; i < N_tid; i++ ) {
        if ( tid[i] == myid )
            std::swap( tid[i], tid[--N_tid] );
    }
    // Get the thread id using signaling
    StackTrace_mutex.lock();
    auto thread0 = StackTrace::thisThread();
    auto old     = signal( thread_callstack_signal, _activeThreads_signal_handler );
    for ( int i = 0; i < N_tid; i++ ) {
        thread_id_finished = false;
        thread_handle      = thread0;
        syscall( SYS_tgkill, pid, tid[i], thread_callstack_signal );
        auto t1 = std::chrono::high_resolution_clock::now();
        auto t2 = t1;
        while ( !thread_id_finished && std::chrono::duration<double>( t2 - t1 ).count() < 0.1 ) {
            std::this_thread::yield();
            t2 = std::chrono::high_resolution_clock::now();
        }
        if ( thread_handle != thread0 ) {
            std::thread::native_handle_type tmp = thread_handle;
            threads.push_back( tmp );
        }
    }
    signal( thread_callstack_signal, old );
    StackTrace_mutex.unlock();
#elif defined( USE_MAC )
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count = 0;
    task_threads( mach_task_self(), &thread_list, &thread_count );
    auto old = signal( thread_callstack_signal, _activeThreads_signal_handler );
    for ( int i = 0; i < static_cast<int>( thread_count ); i++ ) {
        if ( thread_list[i] == mach_thread_self() )
            continue;
        static bool called = false;
        if ( !called ) {
            called = true;
            std::cerr << "activeThreads not finished for MAC\n";
        }
    }
    signal( thread_callstack_signal, old );
#elif defined( USE_WINDOWS )
    HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if ( hThreadSnap != INVALID_HANDLE_VALUE ) {
        // Fill in the size of the structure before using it
        THREADENTRY32 te32;
        te32.dwSize = sizeof( THREADENTRY32 );
        // Retrieve information about the first thread, and exit if unsuccessful
        if ( !Thread32First( hThreadSnap, &te32 ) ) {
            std::cerr << "Unable to get info about first thread\n";
            CloseHandle( hThreadSnap );            // Must clean up the snapshot object!
            return {};
        }
        // Now walk the thread list of the system
        do {
            // if ( te32.th32OwnerProcessID == dwOwnerPID )
            //     threads.push_back( te32.th32ThreadID );
        } while ( Thread32Next( hThreadSnap, &te32 ) );
        CloseHandle( hThreadSnap ); // Must clean up the snapshot object!
    }
#else
    #warning activeThreads is not yet supported on this compiler/OS
#endif
    // Add the current thread
    threads.push_back( StackTrace::thisThread() );
    // Sort the threads
    std::sort( threads.begin(), threads.end() );
    return threads;
}


/****************************************************************************
 *  Register threads with the StackTrace                                     *
 ****************************************************************************/
static std::mutex globalThreadMutex;
static Utilities::staticVector<std::thread::native_handle_type, 1024> globalRegisteredThreads;
thread_local struct ThreadExiter {
    void registerThread() {};
    ~ThreadExiter()
    {
        auto id = thisThread();
        try {
            unregisterThread( id );
        } catch ( ... ) {
        }
    }
} exiter;
void registerThread()
{
    exiter.registerThread();
    registerThread( thisThread() );
}
void registerThread( std::thread::native_handle_type id )
{
    globalThreadMutex.lock();
    auto i = globalRegisteredThreads.find( id );
    if ( i == globalRegisteredThreads.size() )
        globalRegisteredThreads.push_back( id );
    globalThreadMutex.unlock();
}
void unregisterThread( std::thread::native_handle_type id )
{
    globalThreadMutex.lock();
    auto i = globalRegisteredThreads.find( id );
    if ( i != globalRegisteredThreads.size() ) {
        std::swap( globalRegisteredThreads[i], globalRegisteredThreads.back() );
        globalRegisteredThreads.resize( globalRegisteredThreads.size() - 1 );
    }
    globalThreadMutex.unlock();
}
std::vector<std::thread::native_handle_type> registeredThreads()
{
    return std::vector<std::thread::native_handle_type>( globalRegisteredThreads.begin(),
                                                         globalRegisteredThreads.end() );
}


} // namespace StackTrace
