#define NOMINMAX
#include "StackTrace/Utilities.h"
#include "StackTrace/ErrorHandlers.h"
#include "StackTrace/StackTrace.h"
#include "StackTrace/Utilities.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <typeinfo>

#ifdef USE_TIMER
    #include "MemoryApp.h"
#endif

#ifdef USE_GCOV
extern "C" void __gcov_dump();
#endif


#define perr std::cerr


// Detect the OS
// clang-format off
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || defined( _MSC_VER )
    #define USE_WINDOWS
#elif defined( __APPLE__ )
    #define USE_MAC
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
    #define USE_LINUX
    #define USE_NM
#else
    #error Unknown OS
#endif
// clang-format on


// Include system dependent headers
// clang-format off
#ifdef USE_WINDOWS
    #include <process.h>
    #include <stdio.h>
    #include <tchar.h>
    #include <windows.h>
    #include <Psapi.h>  // Must be after windows.h
#else
    #include <dlfcn.h>
    #include <execinfo.h>
    #include <sched.h>
    #include <sys/time.h>
    #include <ctime>
    #include <unistd.h>
#endif
#ifdef USE_LINUX
    #include <malloc.h>
#endif
#ifdef USE_MAC
    #include <mach/mach.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
#endif
// clang-format on


#ifdef __GNUC__
    #define USE_ABI
    #include <cxxabi.h>
#endif


namespace StackTrace::Utilities {


/****************************************************************************
 *  Function to find an entry                                                *
 ****************************************************************************/
template<class TYPE>
inline size_t findfirst( const std::vector<TYPE> &X, TYPE Y )
{
    if ( X.empty() )
        return 0;
    size_t lower = 0;
    size_t upper = X.size() - 1;
    if ( X[lower] >= Y )
        return lower;
    if ( X[upper] < Y )
        return upper;
    while ( ( upper - lower ) != 1 ) {
        size_t value = ( upper + lower ) / 2;
        if ( X[value] >= Y )
            upper = value;
        else
            lower = value;
    }
    return upper;
}


/****************************************************************************
 *  Function to terminate the program                                        *
 ****************************************************************************/
static bool abort_throwException = false;
static int force_exit            = 0;
void setAbortBehavior( bool throwException, int stackType )
{
    abort_throwException = throwException;
    StackTrace::setDefaultStackType( static_cast<printStackType>( stackType ) );
}
void abort( const std::string &message, const source_location &source )
{
    abort_error err;
    err.message   = message;
    err.source    = source;
    err.type      = terminateType::abort;
    err.bytes     = getMemoryUsage();
    err.stackType = StackTrace::getDefaultStackType();
    err.stack     = StackTrace::backtrace();
    throw err;
}
static std::mutex terminate_mutex;
[[noreturn]] static inline void callAbort()
{
#ifdef USE_GCOV
    __gcov_dump();
#endif
    terminate_mutex.unlock();
    std::abort();
}
void terminate( const StackTrace::abort_error &err )
{
    // Lock mutex to ensure multiple threads do not try to abort simultaneously
    terminate_mutex.lock();
    // Clear the error handlers
    clearErrorHandler();
    // Print the message and abort
    if ( force_exit > 1 ) {
        callAbort();
    } else if ( !abort_throwException ) {
        // Use MPI_abort (will terminate all processes)
        force_exit = 2;
        perr << err.what();
#ifdef STACKTRACE_USE_MPI
        int initialized = 0, finalized = 0;
        MPI_Initialized( &initialized );
        MPI_Finalized( &finalized );
        if ( initialized != 0 && finalized == 0 ) {
            clearMPIErrorHandler( MPI_COMM_WORLD );
            MPI_Abort( MPI_COMM_WORLD, -1 );
        }
#endif
        callAbort();
    } else {
        perr << err.what();
        perr.flush();
        callAbort();
    }
}


/****************************************************************************
 *  Functions to set the error handler                                       *
 ****************************************************************************/
void setErrorHandlers( std::function<void( StackTrace::abort_error & )> abort )
{
#ifdef STACKTRACE_USE_MPI
    setMPIErrorHandler( MPI_COMM_WORLD );
    setMPIErrorHandler( MPI_COMM_SELF );
#endif
    if ( abort )
        StackTrace::setErrorHandler( abort );
    else
        StackTrace::setErrorHandler( terminate );
}
void clearErrorHandlers()
{
#ifdef STACKTRACE_USE_MPI
    clearMPIErrorHandler( MPI_COMM_WORLD );
    clearMPIErrorHandler( MPI_COMM_SELF );
#endif
}


/****************************************************************************
 *  Function to get the memory usage                                         *
 *  Note: this function should be thread-safe                                *
 ****************************************************************************/
// clang-format off
#if defined( USE_MAC ) || defined( USE_LINUX )
    // Get the page size on mac or linux
    static size_t page_size = static_cast<size_t>( sysconf( _SC_PAGESIZE ) );
#endif
size_t getSystemMemory()
{
    #if defined( USE_LINUX )
        static long pages = sysconf( _SC_PHYS_PAGES );
        size_t N_bytes    = pages * page_size;
    #elif defined( USE_MAC )
        int mib[2]    = { CTL_HW, HW_MEMSIZE };
        u_int namelen = sizeof( mib ) / sizeof( mib[0] );
        uint64_t size;
        size_t len = sizeof( size );
        size_t N_bytes = 0;
        if ( sysctl( mib, namelen, &size, &len, nullptr, 0 ) == 0 )
            N_bytes = size;
    #elif defined( USE_WINDOWS )
        MEMORYSTATUSEX status;
        status.dwLength = sizeof( status );
        GlobalMemoryStatusEx( &status );
        size_t N_bytes = status.ullTotalPhys;
    #else
        #error Unknown OS
    #endif
    return N_bytes;
}
size_t getMemoryUsage()
{
    #ifdef USE_TIMER
        size_t N_bytes = MemoryApp::getTotalMemoryUsage();
    #else
        #if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
            // Windows
            PROCESS_MEMORY_COUNTERS_EX mem;
            ZeroMemory(&mem, sizeof(PROCESS_MEMORY_COUNTERS_EX));
            GetProcessMemoryInfo( GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&mem, sizeof( mem ) );
            size_t N_bytes = mem.WorkingSetSize;
        #elif defined( __APPLE__ )
            // MAC
            struct task_basic_info t_info;
            mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
            kern_return_t rtn =
                task_info( mach_task_self(), TASK_BASIC_INFO, (task_info_t) &t_info, &t_info_count );
            if ( rtn != KERN_SUCCESS )
                return 0;
            size_t N_bytes = t_info.virtual_size;
        #elif defined( HAVE_MALLINFO2 )
            // Linux - mallinfo2
            auto meminfo   = mallinfo2();
            size_t N_bytes = meminfo.hblkhd + meminfo.uordblks;
        #else
            // Linux - Deprecated mallinfo
            auto meminfo = mallinfo();
            size_t size_hblkhd   = static_cast<unsigned int>( meminfo.hblkhd );
            size_t size_uordblks = static_cast<unsigned int>( meminfo.uordblks );
            size_t N_bytes       = size_hblkhd + size_uordblks;
        #endif
    #endif
    return N_bytes;
}
void printMemoryUsage( const char *indent )
{
    using LLUINT = long long unsigned int;
    #if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
        // Windows
        PROCESS_MEMORY_COUNTERS_EX mem;
        ZeroMemory(&mem, sizeof(PROCESS_MEMORY_COUNTERS_EX));
        GetProcessMemoryInfo( GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&mem, sizeof( mem ) );
        printf( "%sPageFaultCount: 0x%llx\n", indent, (LLUINT) mem.PageFaultCount );
        printf( "%sPeakWorkingSetSize: 0x%llx\n", indent, (LLUINT) mem.PeakWorkingSetSize );
        printf( "%sWorkingSetSize: 0x%llx\n", indent, (LLUINT) mem.WorkingSetSize );
        printf( "%sQuotaPeakPagedPoolUsage: 0x%llx\n", indent, (LLUINT) mem.QuotaPeakPagedPoolUsage );
        printf( "%sQuotaPagedPoolUsage: 0x%llx\n", indent, (LLUINT) mem.QuotaPagedPoolUsage );
        printf( "%sQuotaPeakNonPagedPoolUsage: 0x%llx\n", indent, (LLUINT) mem.QuotaPeakNonPagedPoolUsage );
        printf( "%sQuotaNonPagedPoolUsage: 0x%llx\n", indent, (LLUINT) mem.QuotaNonPagedPoolUsage );
        printf( "%sPagefileUsage: 0x%llx\n", indent, (LLUINT) mem.PagefileUsage );
        printf( "%sPeakPagefileUsage: 0x%llx\n", indent, (LLUINT) mem.PeakPagefileUsage );
        printf( "%sPrivateUsage: 0x%llx\n", indent, (LLUINT) mem.PrivateUsage );
    #elif defined( HAVE_MALLINFO2 )
        // Linux - mallinfo2
        auto mem   = mallinfo2();
        printf( "%sarena: 0x%llx\n", indent, (LLUINT) mem.arena );
        printf( "%sordblks: 0x%llx\n", indent, (LLUINT) mem.ordblks );
        printf( "%ssmblks: 0x%llx\n", indent, (LLUINT) mem.smblks );
        printf( "%shblks: 0x%llx\n", indent, (LLUINT) mem.hblks );
        printf( "%shblkhd: 0x%llx\n", indent, (LLUINT) mem.hblkhd );
        printf( "%susmblks: 0x%llx\n", indent, (LLUINT) mem.usmblks );
        printf( "%sfsmblks: 0x%llx\n", indent, (LLUINT) mem.fsmblks );
        printf( "%suordblks: 0x%llx\n", indent, (LLUINT) mem.uordblks );
        printf( "%sfordblks: 0x%llx\n", indent, (LLUINT) mem.fordblks );
        printf( "%skeepcost: 0x%llx\n", indent, (LLUINT) mem.keepcost );
    #endif
}
// clang-format on


/****************************************************************************
 *  Functions to get the time and timer resolution                           *
 ****************************************************************************/
static auto d_t0 = std::chrono::steady_clock::now();
template<class T>
static constexpr int64_t diff_ns( std::chrono::time_point<T> t2, std::chrono::time_point<T> t1 )
{
    using PERIOD = typename std::chrono::time_point<T>::period;
    if constexpr ( std::ratio_equal_v<PERIOD, std::nano> &&
                   sizeof( std::chrono::time_point<T> ) == 8 ) {
        return *reinterpret_cast<const int64_t *>( &t2 ) -
               *reinterpret_cast<const int64_t *>( &t1 );
    } else {
        return std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count();
    }
}
double time() { return 1e-9 * diff_ns( std::chrono::steady_clock::now(), d_t0 ); }
double tick() { throw std::logic_error( "tick is deprecated and will be removed!!!" ); }


/****************************************************************************
 *  Sleep                                                                    *
 ****************************************************************************/
void sleep_ms( int N ) { std::this_thread::sleep_for( std::chrono::milliseconds( N ) ); }
void sleep_s( int N ) { std::this_thread::sleep_for( std::chrono::seconds( N ) ); }


/****************************************************************************
 *  Cause a segfault                                                         *
 ****************************************************************************/
void cause_segfault()
{
    int *ptr = nullptr;
    ptr[0]   = 0;
}


/****************************************************************************
 *  Utility to call system command and return output                         *
 ****************************************************************************/
std::string exec( const std::string &cmd, int &code )
{
    std::string result;
    auto fun = [&result]( const char *line ) { result += line; };
    code     = exec2( cmd.data(), fun );
    return result;
}


/****************************************************************************
 *  Get the type name                                                        *
 ****************************************************************************/
static std::string replace( std::string str, std::string_view x, std::string_view y )
{
    for ( auto pos = str.find( x ); pos != std::string::npos; pos = str.find( x ) )
        str = str.replace( pos, x.size(), y );
    return str;
}
std::string getTypeName( const std::type_info &id )
{
    std::string name = id.name();
#if defined( USE_ABI )
    int status;
    auto tmp = abi::__cxa_demangle( name.c_str(), 0, 0, &status );
    name.assign( tmp );
    free( tmp );
#endif
    name = replace( name, "class ", "" );
    name = replace( name, "struct ", "" );
    return name;
}

/****************************************************************************
 *  Function to set an environemental variable                               *
 ****************************************************************************/
static std::mutex env_mutex;
void setenv( const char *name, const char *value )
{
    env_mutex.lock();
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _MSC_VER )
    bool pass = SetEnvironmentVariable( name, value ) != 0;
#else
    bool pass = false;
    if ( value == nullptr )
        pass = ::unsetenv( name ) == 0;
    else
        pass = ::setenv( name, value, 1 ) == 0;
#endif
    env_mutex.unlock();
    if ( !pass ) {
        char msg[1024];
        if ( value != nullptr )
            sprintf( msg, "Error setting environmental variable: %s=%s", name, value );
        else
            sprintf( msg, "Error clearing environmental variable: %s\n", name );
        throw std::logic_error( msg );
    }
}
std::string getenv( const char *name )
{
    std::string var;
    env_mutex.lock();
    auto tmp = std::getenv( name );
    if ( tmp )
        var = std::string( tmp );
    env_mutex.unlock();
    return var;
}


/****************************************************************************
 *  Check if we are running withing valgrind                                 *
 ****************************************************************************/
// clang-format off
#if __has_include( "valgrind.h" )
    #include "valgrind.h"
    bool running_valgrind() { return RUNNING_ON_VALGRIND; }
#else
    bool running_valgrind()
    {
        auto x = getenv( "LD_PRELOAD" );
        return std::min( x.find("/valgrind/"), x.find("/vgpreload") ) != std::string::npos;
    }
#endif
// clang-format on


} // namespace StackTrace::Utilities
