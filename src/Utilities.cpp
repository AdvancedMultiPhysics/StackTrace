#define NOMINMAX
#include "Utilities.h"
#include "StackTrace.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string.h>

#ifdef USE_MPI
#include "mpi.h"
#endif

// Detect the OS and include system dependent headers
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _MSC_VER )
// Note: windows has not been testeds
#define USE_WINDOWS
#include <DbgHelp.h>
#include <iostream>
#include <process.h>
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#define mkdir( path, mode ) _mkdir( path )
//#pragma comment(lib, psapi.lib) //added
//#pragma comment(linker, /DEFAULTLIB:psapi.lib)
#elif defined( __APPLE__ )
#define USE_MAC
#include <dlfcn.h>
#include <execinfo.h>
#include <mach/mach.h>
#include <sched.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined( __linux ) || defined( __unix ) || defined( __posix )
#define USE_LINUX
#include <dlfcn.h>
#include <execinfo.h>
#include <malloc.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#else
#error Unknown OS
#endif


/****************************************************************************
*  Function to terminate the program                                        *
****************************************************************************/
static bool abort_printMemory    = true;
static bool abort_printStack     = true;
static bool abort_throwException = false;
static int force_exit            = 0;
void Utilities::setAbortBehavior( bool printMemory, bool printStack, bool throwException )
{
    abort_printMemory    = printMemory;
    abort_printStack     = printStack;
    abort_throwException = throwException;
}
void Utilities::abort( const std::string &message, const std::string &filename, const int line )
{
    std::stringstream msg;
    msg << "Program abort called in file `" << filename << "' at line " << line << std::endl;
    // Add the memory usage and call stack to the error message
    if ( abort_printMemory ) {
        size_t N_bytes = Utilities::getMemoryUsage();
        msg << "Bytes used = " << N_bytes << std::endl;
    }
    if ( abort_printStack ) {
        std::vector<StackTrace::stack_info> stack = StackTrace::getCallStack();
        msg << std::endl;
        msg << "Stack Trace:\n";
        for ( size_t i = 0; i < stack.size(); i++ )
            msg << "   " << stack[i].print() << std::endl;
    }
    msg << std::endl << message << std::endl;
    // Print the message and abort
    if ( force_exit > 1 ) {
        exit( -1 );
    } else if ( !abort_throwException ) {
        // Use MPI_abort (will terminate all processes)
        force_exit = 2;
        std::cerr << msg.str();
#if defined( USE_MPI ) || defined( HAVE_MPI )
        int initialized = 0, finalized = 0;
        MPI_Initialized( &initialized );
        MPI_Finalized( &finalized );
        if ( initialized != 0 && finalized == 0 )
            MPI_Abort( MPI_COMM_WORLD, -1 );
#endif
        exit( -1 );
    } else if ( force_exit > 0 ) {
        exit( -1 );
    } else {
        // Throw and standard exception (allows the use of try, catch)
        throw std::logic_error( msg.str() );
    }
}


/****************************************************************************
*  Function to handle MPI errors                                            *
****************************************************************************/
/*#if defined(USE_MPI) || defined(HAVE_MPI)
MPI_Errhandler mpierr;
void MPI_error_handler_fun( MPI_Comm *comm, int *err, ... )
{
    if ( *err==MPI_ERR_COMM && *comm==MPI_COMM_WORLD ) {
        // Special error handling for an invalid MPI_COMM_WORLD
        std::cerr << "Error invalid MPI_COMM_WORLD";
        exit(-1);
    }
    int msg_len=0;
    char message[1000];
    MPI_Error_string( *err, message, &msg_len );
    if ( msg_len <= 0 )
         abort("Unkown error in MPI");
    abort( "Error calling MPI routine:\n" + std::string(message) );
}
#endif*/


/****************************************************************************
*  Function to handle unhandled exceptions                                  *
****************************************************************************/
bool tried_MPI_Abort = false;
void term_func_abort( int err )
{
    std::cerr << "Exiting due to abort (" << err << ")\n";
    std::vector<StackTrace::stack_info> stack = StackTrace::getCallStack();
    std::string message                       = "Stack Trace:\n";
    for ( size_t i = 0; i < stack.size(); i++ )
        message += "   " + stack[i].print() += "\n";
    message += "\nExiting\n";
    // Print the message and abort
    std::cerr << message;
#ifdef USE_MPI
    if ( !abort_throwException && !tried_MPI_Abort ) {
        tried_MPI_Abort = true;
        MPI_Abort( MPI_COMM_WORLD, -1 );
    }
#endif
    exit( -1 );
}
#if defined( USE_LINUX ) || defined( USE_MAC )
static int tried_throw = 0;
#endif
void term_func()
{
    // Try to re-throw the last error to get the last message
    std::string last_message;
#if defined( USE_LINUX ) || defined( USE_MAC )
    try {
        if ( tried_throw == 0 ) {
            tried_throw = 1;
            throw;
        }
        // No active exception
    } catch ( const std::exception &err ) {
        // Caught a std::runtime_error
        last_message = err.what();
    } catch ( ... ) {
        // Caught an unknown exception
        last_message = "unknown exception occurred.";
    }
#endif
    std::stringstream msg;
    msg << "Unhandled exception:" << std::endl;
    msg << "   " << last_message << std::endl;
    Utilities::abort( msg.str(), __FILE__, __LINE__ );
}


/****************************************************************************
*  Functions to set the error handler                                       *
****************************************************************************/
static void setTerminateErrorHandler()
{
    std::set_terminate( term_func );
    signal( SIGABRT, &term_func_abort );
    signal( SIGFPE, &term_func_abort );
    signal( SIGILL, &term_func_abort );
    signal( SIGINT, &term_func_abort );
    signal( SIGSEGV, &term_func_abort );
    signal( SIGTERM, &term_func_abort );
}
/*#ifdef USE_MPI
    static void setMPIErrorHandler( MPI_Comm mpi )
    {
        if ( mpierr.get()==NULL ) {
            mpierr = boost::shared_ptr<MPI_Errhandler>( new MPI_Errhandler );
            MPI_Comm_create_errhandler( MPI_error_handler_fun, mpierr.get() );
        }
        MPI_Comm_set_errhandler( mpi.getCommunicator(), *mpierr );
        MPI_Comm_set_errhandler( MPI_COMM_WORLD, *mpierr );
    }
    static void clearMPIErrorHandler(  )
    {
        if ( mpierr.get()!=NULL )
            MPI_Errhandler_free( mpierr.get() );    // Delete the error handler
        mpierr.reset();
        MPI_Comm_set_errhandler( MPI_COMM_SELF, MPI_ERRORS_ARE_FATAL );
        MPI_Comm_set_errhandler( MPI_COMM_WORLD, MPI_ERRORS_ARE_FATAL );
}
#endif*/
void Utilities::setErrorHandlers()
{
    // d_use_MPI_Abort = use_MPI_Abort;
    //#ifdef USE_MPI
    //   setMPIErrorHandler( SAMRAI::tbox::SAMRAI_MPI::getSAMRAIWorld() );
    // #endif
    setTerminateErrorHandler();
}


/****************************************************************************
*  Function to set an environemental variable                               *
****************************************************************************/
void Utilities::setenv( const char *name, const char *value )
{
#if defined( USE_LINUX ) || defined( USE_MAC )
    char env[100];
    sprintf( env, "%s=%s", name, value );
    bool pass = false;
    if ( value != NULL )
        pass = ::setenv( name, value, 1 ) == 0;
    else
        pass = ::unsetenv( name ) == 0;
#elif defined( USE_WINDOWS )
    bool pass     = SetEnvironmentVariable( name, value );
#else
#error Unknown OS
#endif
    if ( !pass ) {
        char msg[100];
        if ( value != NULL )
            sprintf( msg, "Error setting enviornmental variable: %s=%s\n", name, value );
        else
            sprintf( msg, "Error clearing enviornmental variable: %s\n", name );
        ERROR( msg );
    }
}


/****************************************************************************
*  Function to get the memory usage                                         *
*  Note: this function should be thread-safe                                *
****************************************************************************/
#if defined( USE_MAC ) || defined( USE_LINUX )
// Get the page size on mac or linux
static size_t page_size = static_cast<size_t>( sysconf( _SC_PAGESIZE ) );
#endif
static size_t N_bytes_initialization = Utilities::getMemoryUsage();
size_t Utilities::getSystemMemory()
{
    size_t N_bytes = 0;
#if defined( USE_LINUX )
    static long pages = sysconf( _SC_PHYS_PAGES );
    N_bytes           = pages * page_size;
#elif defined( USE_MAC )
    int mib[2]    = { CTL_HW, HW_MEMSIZE };
    u_int namelen = sizeof( mib ) / sizeof( mib[0] );
    uint64_t size;
    size_t len = sizeof( size );
    if ( sysctl( mib, namelen, &size, &len, NULL, 0 ) == 0 )
        N_bytes = size;
#elif defined( USE_WINDOWS )
    MEMORYSTATUSEX status;
    status.dwLength = sizeof( status );
    GlobalMemoryStatusEx( &status );
    N_bytes = status.ullTotalPhys;
#else
#error Unknown OS
#endif
    return N_bytes;
}
size_t Utilities::getMemoryUsage()
{
    size_t N_bytes = 0;
#if defined( USE_LINUX )
    /*std::ifstream stat_stream("/proc/self/stat",std::ios_base::in);
    size_t stats[100];
    memset(stats,0,100*sizeof(size_t));
    for (int i=0; (i<100)&(!stat_stream.eof()); i++) {
        std::string tmp;
        stat_stream >> tmp;
        stats[i] = strtoul(tmp.c_str(),NULL,10);
    }*/
    struct mallinfo meminfo = mallinfo();
    size_t size_hblkhd      = static_cast<unsigned int>( meminfo.hblkhd );
    size_t size_uordblks    = static_cast<unsigned int>( meminfo.uordblks );
    N_bytes                 = size_hblkhd + size_uordblks;
#elif defined( USE_MAC )
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    if ( KERN_SUCCESS !=
         task_info( mach_task_self(), TASK_BASIC_INFO, (task_info_t) &t_info, &t_info_count ) ) {
        return 0;
    }
    N_bytes = t_info.virtual_size;
#elif defined( USE_WINDOWS )
    PROCESS_MEMORY_COUNTERS memCounter;
    GetProcessMemoryInfo( GetCurrentProcess(), &memCounter, sizeof( memCounter ) );
    N_bytes = memCounter.WorkingSetSize;
#else
#error Unknown OS
#endif
    return N_bytes;
}


/****************************************************************************
*  Functions to get the time and timer resolution                           *
****************************************************************************/
#if defined( USE_WINDOWS )
double Utilities::time()
{
    LARGE_INTEGER end, f;
    QueryPerformanceFrequency( &f );
    QueryPerformanceCounter( &end );
    double time = ( (double) end.QuadPart ) / ( (double) f.QuadPart );
    return time;
}
double Utilities::tick()
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency( &f );
    double resolution = ( (double) 1.0 ) / ( (double) f.QuadPart );
    return resolution;
}
#elif defined( USE_LINUX ) || defined( USE_MAC )
double Utilities::time()
{
    timeval current_time;
    gettimeofday( &current_time, NULL );
    double time = ( (double) current_time.tv_sec ) + 1e-6 * ( (double) current_time.tv_usec );
    return time;
}
double Utilities::tick()
{
    timeval start, end;
    gettimeofday( &start, NULL );
    gettimeofday( &end, NULL );
    while ( end.tv_sec == start.tv_sec && end.tv_usec == start.tv_usec )
        gettimeofday( &end, NULL );
    double resolution = ( (double) ( end.tv_sec - start.tv_sec ) ) +
                        1e-6 * ( (double) ( end.tv_usec - start.tv_usec ) );
    return resolution;
}
#else
#error Unknown OS
#endif


/****************************************************************************
*  Functions to sleep for x ms                                              *
****************************************************************************/
void Utilities::sleep_ms( int milliseconds )
{
#ifdef USE_WINDOWS
    Sleep( milliseconds );
#else
#ifdef _POSIX_C_SOURCE
#if _POSIX_C_SOURCE < 199309L
#error Using a really old POSIX?
#endif
#endif
    struct timespec ts;
    ts.tv_sec  = milliseconds / 1000;
    ts.tv_nsec = ( milliseconds % 1000 ) * 1000000;
    nanosleep( &ts, NULL );
#endif
}
void Utilities::sleep_s( int seconds )
{
#ifdef USE_WINDOWS
    Sleep( 1000 * seconds );
#else
    sleep( seconds );
#endif
}


/****************************************************************************
*  Cause a segfault                                                         *
****************************************************************************/
void Utilities::cause_segfault()
{
    int *ptr = NULL;
    ptr[0]   = 0;
}
