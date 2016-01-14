#include "StackTrace.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#if __cplusplus > 199711L
#include <mutex>
#endif


// Detect the OS and include system dependent headers
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _MSC_VER )
#define USE_WINDOWS
#define NOMINMAX
// clang-format off
#include <windows.h>
#include <dbghelp.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <psapi.h>
#include <iostream>
#include <process.h>
#include <stdio.h>
#include <tchar.h>
// clang-format on
//#pragma comment(lib, psapi.lib) //added
//#pragma comment(linker, /DEFAULTLIB:psapi.lib)
#elif defined( __APPLE__ )
#define USE_MAC
#define USE_NM
#include <dlfcn.h>
#include <execinfo.h>
#include <mach-o/dyld.h>
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
#define USE_NM
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


#ifdef __GNUC__
#define USE_ABI
#include <cxxabi.h>
#endif

#ifndef NULL_USE
#define NULL_USE( variable )                 \
    do {                                     \
        if ( 0 ) {                           \
            char *temp = (char *) &variable; \
            temp++;                          \
        }                                    \
    } while ( 0 )
#endif


// Utility to strip the path from a filename
inline std::string stripPath( const std::string &filename )
{
    if ( filename.empty() ) {
        return std::string();
    }
    int i = 0;
    for ( i = (int) filename.size() - 1; i >= 0 && filename[i] != 47 && filename[i] != 92; i-- ) {
    }
    i = std::max( 0, i + 1 );
    return filename.substr( i );
}


// Inline function to subtract two addresses returning the absolute difference
inline void *subtractAddress( void *a, void *b )
{
    return reinterpret_cast<void *>(
        std::abs( reinterpret_cast<long long int>( a ) - reinterpret_cast<long long int>( b ) ) );
}


/****************************************************************************
*  stack_info                                                               *
****************************************************************************/
std::string StackTrace::stack_info::print() const
{
    char tmp[32];
    sprintf( tmp, "0x%016llx:  ", reinterpret_cast<unsigned long long int>( address ) );
    std::string stack( tmp );
    sprintf( tmp, "%i", line );
    std::string line_str( tmp );
    stack += stripPath( object );
    stack.resize( std::max<size_t>( stack.size(), 38 ), ' ' );
    stack += "  " + function;
    if ( !filename.empty() && line > 0 ) {
        stack.resize( std::max<size_t>( stack.size(), 70 ), ' ' );
        stack += "  " + stripPath( filename ) + ":" + line_str;
    } else if ( !filename.empty() ) {
        stack.resize( std::max<size_t>( stack.size(), 70 ), ' ' );
        stack += "  " + stripPath( filename );
    } else if ( line > 0 ) {
        stack += " : " + line_str;
    }
    return stack;
}


/****************************************************************************
*  Function to find an entry                                                *
****************************************************************************/
template <class TYPE>
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
* Function to get symbols for the executable from nm (if availible)         *
* Note: this function maintains an internal cached copy to prevent          *
*    exccessive calls to nm.  This function also uses a lock to ensure      *
*    thread safety.                                                         *
****************************************************************************/
#if __cplusplus <= 199711L
class mutex_class
{
public:
    void lock() {}
    void unlock() {}
};
mutex_class getSymbols_mutex;
#else
std::mutex getSymbols_mutex;
#endif
struct global_symbols_struct {
    std::vector<void *> address;
    std::vector<char> type;
    std::vector<std::string> obj;
    int error;
} global_symbols;
std::string StackTrace::getExecutable()
{
    std::string exe;
    try {
#ifdef USE_LINUX
        char *buf = new char[0x10000];
        int len   = ::readlink( "/proc/self/exe", buf, 0x10000 );
        if ( len != -1 ) {
            buf[len] = '\0';
            exe      = std::string( buf );
        }
        delete[] buf;
#elif defined( USE_MAC )
        uint32_t size = 0x10000;
        char *buf     = new char[size];
        memset( buf, 0, size );
        if ( _NSGetExecutablePath( buf, &size ) == 0 )
            exe = std::string( buf );
        delete[] buf;
#elif defined( USE_WINDOWS )
        DWORD size = 0x10000;
        char *buf  = new char[size];
        memset( buf, 0, size );
        GetModuleFileName( NULL, buf, size );
        exe = std::string( buf );
        delete[] buf;
#endif
    } catch ( ... ) {
    }
    return exe;
}
std::string global_exe_name = StackTrace::getExecutable();
static const global_symbols_struct &getSymbols2()
{
    static bool loaded = false;
    static global_symbols_struct data;
    // Load the symbol tables if they have not been loaded
    if ( !loaded ) {
        getSymbols_mutex.lock();
        if ( !loaded ) {
            loaded = true;
#ifdef USE_NM
            try {
                char cmd[1024];
#ifdef USE_LINUX
                sprintf( cmd, "nm -n --demangle %s", global_exe_name.c_str() );
#elif defined( USE_MAC )
                sprintf( cmd, "nm -n %s | c++filt", global_exe_name.c_str() );
#else
#error Unknown OS using nm
#endif
                FILE *in = popen( cmd, "r" );
                if ( in == nullptr ) {
                    data.error = -2;
                    return data;
                }
                char *buf = new char[0x100000];
                while ( fgets( buf, 0xFFFFF, in ) != nullptr ) {
                    if ( buf[0] == ' ' )
                        continue;
                    char *a = buf;
                    char *b = strchr( a, ' ' );
                    if ( b == nullptr )
                        continue;
                    b[0] = 0;
                    b++;
                    char *c = strchr( b, ' ' );
                    if ( c == nullptr )
                        continue;
                    c[0] = 0;
                    c++;
                    char *d = strchr( c, '\n' );
                    if ( d )
                        d[0]   = 0;
                    size_t add = strtoul( a, nullptr, 16 );
                    data.address.push_back( reinterpret_cast<void *>( add ) );
                    data.type.push_back( b[0] );
                    data.obj.push_back( std::string( c ) );
                }
                pclose( in );
                delete[] buf;
            } catch ( ... ) {
                data.error = -3;
            }
            data.error = 0;
#else
            data.error = -1;
#endif
        }
        getSymbols_mutex.unlock();
    }
    return data;
}
int StackTrace::getSymbols( std::vector<void *> &address,
                            std::vector<char> &type,
                            std::vector<std::string> &obj )
{
    const global_symbols_struct &data = getSymbols2();
    address                           = data.address;
    type                              = data.type;
    obj                               = data.obj;
    return data.error;
}


/****************************************************************************
*  Function to get the current call stack                                   *
****************************************************************************/
static void getFileAndLine( StackTrace::stack_info &info )
{
#if defined( USE_LINUX )
    void *address = info.address;
    if ( info.object.find( ".so" ) != std::string::npos )
        address = info.address2;
    char buf[4096];
    sprintf( buf,
             "addr2line -C -e %s -f -i %lx 2> /dev/null",
             info.object.c_str(),
             reinterpret_cast<unsigned long int>( address ) );
    FILE *f = popen( buf, "r" );
    if ( f == nullptr )
        return;
    buf[4095] = 0;
    // get function name
    char *rtn = fgets( buf, 4095, f );
    if ( info.function.empty() && rtn == buf ) {
        info.function = std::string( buf );
        info.function.resize( std::max<size_t>( info.function.size(), 1 ) - 1 );
    }
    // get file and line
    rtn = fgets( buf, 4095, f );
    if ( buf[0] != '?' && buf[0] != 0 && rtn == buf ) {
        size_t i = 0;
        for ( i = 0; i < 4095 && buf[i] != ':'; i++ ) {
        }
        info.filename = std::string( buf, i );
        info.line     = atoi( &buf[i + 1] );
    }
    pclose( f );
#elif defined( USE_MAC ) && 0
/*void *address = info.address;
if ( info.object.find( ".so" ) != std::string::npos )
    address = info.address2;
char buf[4096];
sprintf( buf, "atos -o %s %lx 2> /dev/null", info.object.c_str(),
    reinterpret_cast<unsigned long int>( address ) );
FILE *f = popen( buf, "r" );
if ( f == nullptr )
    return;
buf[4095] = 0;
// get function name
char *rtn = fgets( buf, 4095, f );
if ( info.function.empty() && rtn == buf ) {
    info.function = std::string( buf );
    info.function.resize( std::max<size_t>( info.function.size(), 1 ) - 1 );
}
// get file and line
rtn = fgets( buf, 4095, f );
if ( buf[0] != '?' && buf[0] != 0 && rtn == buf ) {
    size_t i = 0;
    for ( i = 0; i < 4095 && buf[i] != ':'; i++ ) {
    }
    info.filename = std::string( buf, i );
    info.line     = atoi( &buf[i + 1] );
}
pclose( f );*/
#endif
}

// Try to use the global symbols to decode info about the stack
static void getDataFromGlobalSymbols( StackTrace::stack_info &info )
{
    const global_symbols_struct &data = getSymbols2();
    if ( data.error == 0 ) {
        size_t index = findfirst( global_symbols.address, info.address );
        if ( index > 0 )
            info.object = global_symbols.obj[index - 1];
        else
            info.object = global_exe_name;
    }
}
// clang-format off
StackTrace::stack_info StackTrace::getStackInfo( void *address )
{
    StackTrace::stack_info info;
    info.address = address;
    #if defined(_GNU_SOURCE) || defined(USE_MAC)
        Dl_info dlinfo;
        if ( !dladdr( address, &dlinfo ) ) {
            getDataFromGlobalSymbols( info );
            getFileAndLine( info );
            return info;
        }
        info.address2 = subtractAddress( info.address, dlinfo.dli_fbase );
        info.object   = std::string( dlinfo.dli_fname );
        #if defined( USE_ABI )
            int status;
            char *demangled = abi::__cxa_demangle( dlinfo.dli_sname, nullptr, nullptr, &status );
            if ( status == 0 && demangled != nullptr ) {
                info.function = std::string( demangled );
            } else if ( dlinfo.dli_sname != nullptr ) {
                info.function = std::string( dlinfo.dli_sname );
            }
            free( demangled );
        #else
            if ( dlinfo.dli_sname != NULL )
                info.function = std::string( dlinfo.dli_sname );
        #endif
    #else
        getDataFromGlobalSymbols( info );
    #endif
    // Get the filename / line number
    getFileAndLine( info );
    return info;
}
std::vector<StackTrace::stack_info> StackTrace::getCallStack()
{
    std::vector<StackTrace::stack_info> stack_list;
#if defined( USE_LINUX ) || defined( USE_MAC )
    // Get the trace
    void *trace[100];
    memset( trace, 0, 100 * sizeof( void * ) );
    int trace_size = backtrace( trace, 100 );
    stack_list.reserve( trace_size );
    for ( int i = 0; i < trace_size; ++i )
        stack_list.push_back( getStackInfo( trace[i] ) );
#elif defined( USE_WINDOWS )
#if defined(DBGHELP)
    // Get the search paths for symbols
    std::string paths = getSymPaths();

    // Initialize the symbols
    if ( SymInitialize( GetCurrentProcess(), paths.c_str(), FALSE ) == FALSE )
        OnDbgHelpErr( "SymInitialize", GetLastError(), 0 );
    DWORD symOptions = SymGetOptions();
    symOptions |= SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS;
    symOptions = SymSetOptions( symOptions );
    char buf[STACKWALK_MAX_NAMELEN] = { 0 };
    if ( SymGetSearchPath( GetCurrentProcess(), buf, STACKWALK_MAX_NAMELEN ) == FALSE )
        OnDbgHelpErr( "SymGetSearchPath", GetLastError(), 0 );

    // First try to load modules from toolhelp32
    BOOL loaded = GetModuleListTH32( GetCurrentProcess(), GetCurrentProcessId() );

    // Try to load from Psapi
    if ( !loaded )
        loaded = GetModuleListPSAPI( GetCurrentProcess() );

    ::CONTEXT context
    memset( &context, 0, sizeof( context ) );
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext( &context );

    // init STACKFRAME for first call
    STACKFRAME64 frame; // in/out stackframe
    memset( &frame, 0, sizeof( frame ) );
#ifdef _M_IX86
    // normally, call ImageNtHeader() and use machine info from PE header
    DWORD imageType    = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = context.Eip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode   = AddrModeFlat;
#elif _M_X64
    DWORD imageType    = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = context.Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rsp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;
#elif _M_IA64
    DWORD imageType    = IMAGE_FILE_MACHINE_IA64;
    frame.AddrPC.Offset     = context.StIIP;
    frame.AddrPC.Mode       = AddrModeFlat;
    frame.AddrFrame.Offset  = context.IntSp;
    frame.AddrFrame.Mode    = AddrModeFlat;
    frame.AddrBStore.Offset = context.RsBSP;
    frame.AddrBStore.Mode   = AddrModeFlat;
    frame.AddrStack.Offset  = context.IntSp;
    frame.AddrStack.Mode    = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

    IMAGEHLP_SYMBOL64 pSym[STACKWALK_MAX_NAMELEN];
    memset( pSym, 0, sizeof( pSym ) );
    pSym->SizeOfStruct  = sizeof( IMAGEHLP_SYMBOL64 );
    pSym->MaxNameLength = STACKWALK_MAX_NAMELEN;

    IMAGEHLP_MODULE64 Module;
    memset( &Module, 0, sizeof( Module ) );
    Module.SizeOfStruct = sizeof( Module );

    std::vector<CallstackEntry> stack_list;
    for ( int frameNum = 0, curRecursionCount = 0; ; ++frameNum ) {
        // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
        // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
        // assume that either you are done, or that the stack is so hosed that the next
        // deeper frame could not be found.
        // CONTEXT need not to be suplied if imageTyp is IMAGE_FILE_MACHINE_I386!
        if ( !StackWalk64( imageType, GetCurrentProcess(), GetCurrentThread(), &s, &c,
                 myReadProcMem, SymFunctionTableAccess, SymGetModuleBase64, NULL ) ) {
            // INFO: "StackWalk64" does not set "GetLastError"...
            OnDbgHelpErr( "StackWalk64", 0, s.AddrPC.Offset );
            break;
        }

        CallstackEntry csEntry;
        csEntry.address = reinterpret_cast<void*>( s.AddrPC.Offset );
        if ( s.AddrPC.Offset == s.AddrReturn.Offset ) {
            if ( curRecursionCount > MAX_STACK_DEPTH ) {
                OnDbgHelpErr( "StackWalk64-Endless-Callstack!", 0, s.AddrPC.Offset );
                break;
            }
            curRecursionCount++;
        } else
            curRecursionCount = 0;
        if ( s.AddrPC.Offset != 0 ) {
            // we seem to have a valid PC
            // show procedure info (SymGetSymFromAddr64())
            DWORD64 offsetFromSmybol;
            if ( SymGetSymFromAddr( GetCurrentProcess(), s.AddrPC.Offset, &offsetFromSmybol, pSym ) != FALSE ) {
                char name[8192]={0};
                DWORD rtn = UnDecorateSymbolName( pSym->Name, name, sizeof(name)-1, UNDNAME_COMPLETE );
                if ( rtn == 0 )
                    csEntry.function = std::string(pSym->Name);
                else
                    csEntry.function = std::string(name);
            } else {
                OnDbgHelpErr( "SymGetSymFromAddr64", GetLastError(), s.AddrPC.Offset );
            }

            // show line number info, NT5.0-method (SymGetLineFromAddr64())
            IMAGEHLP_LINE64 Line;
            memset( &Line, 0, sizeof( Line ) );
            Line.SizeOfStruct = sizeof( Line );
            DWORD offsetFromLine;
            if ( SymGetLineFromAddr64( GetCurrentProcess(), s.AddrPC.Offset, &offsetFromLine, &Line ) != FALSE ) {
                csEntry.line     = Line.LineNumber;
                csEntry.filename = std::string( Line.FileName );
            } else {
                csEntry.line     = 0;
                csEntry.filename = std::string();
            }

            // show module info (SymGetModuleInfo64())
            if ( SymGetModuleInfo64( GetCurrentProcess(), s.AddrPC.Offset, &Module ) != FALSE ) {
                //csEntry.object = std::string( Module.ModuleName );
                csEntry.object = std::string( Module.LoadedImageName );
                //csEntry.baseOfImage = Module.BaseOfImage;
            }
        } // we seem to have a valid PC

        CallstackEntryType et = nextEntry;
        if ( frameNum == 0 )
            et           = firstEntry;
        if ( csEntry.address != 0 )
            stack_list.push_back(csEntry);

        if ( s.AddrReturn.Offset == 0 ) {
            SetLastError( ERROR_SUCCESS );
            break;
        }
    }
#endif
#else
    #warning Stack trace is not supported on this compiler/OS
#endif
    return stack_list;
}
// clang-format on
