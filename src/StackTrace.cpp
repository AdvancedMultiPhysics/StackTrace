#include "StackTrace.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <csignal>
#include <mutex>
#include <map>
#include <stdexcept>


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
#include <iostream>
#include <process.h>
#include <stdio.h>
#include <tchar.h>
// clang-format on
#pragma comment( lib, "version.lib" ) // for "VerQueryValue"
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
#include <mach/mach_types.h>
#include <mach-o/getsect.h>
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


#ifdef USE_WINDOWS
static BOOL __stdcall readProcMem( HANDLE hProcess,
                                   DWORD64 qwBaseAddress,
                                   PVOID lpBuffer,
                                   DWORD nSize,
                                   LPDWORD lpNumberOfBytesRead )
{
    SIZE_T st;
    BOOL bRet = ReadProcessMemory( hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, &st );
    *lpNumberOfBytesRead = (DWORD) st;
    return bRet;
}
static inline std::string getCurrentDirectory()
{
    char temp[1024] = { 0 };
    GetCurrentDirectoryA( sizeof( temp ), temp );
    return temp;
}
namespace StackTrace {
BOOL GetModuleListTH32( HANDLE hProcess, DWORD pid );
BOOL GetModuleListPSAPI( HANDLE hProcess );
DWORD LoadModule( HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size );
void LoadModules( );
};
#endif


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
        stack.resize( std::max<size_t>( stack.size(), 72 ), ' ' );
        stack += "  " + stripPath( filename ) + ":" + line_str;
    } else if ( !filename.empty() ) {
        stack.resize( std::max<size_t>( stack.size(), 72 ), ' ' );
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
std::mutex getSymbols_mutex;
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
#ifdef USE_MAC
static void *loadAddress( const std::string& object )
{
    static std::map<std::string,void*> obj_map;
    if ( obj_map.empty() ) {
        uint32_t numImages = _dyld_image_count();
        for (uint32_t i = 0; i < numImages; i++) {
            const struct mach_header *header = _dyld_get_image_header(i);
            const char *name = _dyld_get_image_name(i);
            const char *p = strrchr(name, '/');
            struct mach_header *address = const_cast<struct mach_header*>(header);
            obj_map.insert( std::pair<std::string,void*>( p+1, address ) );
            //printf("   module=%s, address=%p\n", p + 1, header);
        }
    }
    auto it = obj_map.find( object );
    void *address = 0;
    if ( it != obj_map.end() ) {
        address = it->second;
    } else {
        it = obj_map.find( stripPath( object ) );
        if ( it != obj_map.end() )
            address = it->second;
    }
    //printf("%s: 0x%016llx\n",object.c_str(),address);
    return address;
}
static std::tuple<std::string,std::string,std::string,int> split_atos( const std::string& buf )
{
    if ( buf.empty() )
        return std::tuple<std::string,std::string,std::string,int>();
    // Get the function
    size_t index = buf.find( " (in " );
    if ( index == std::string::npos )
        return std::make_tuple(buf.substr(0,buf.length()-1),std::string(),std::string(),0);
    std::string fun = buf.substr( 0, index );
    std::string tmp = buf.substr( index+5 );
    // Get the object
    index = tmp.find( ')' );
    std::string obj = tmp.substr( 0, index );
    tmp = tmp.substr( index+1 );
    // Get the filename and line number
    size_t p1 = tmp.find( '(' );
    size_t p2 = tmp.find( ')' );
    tmp = tmp.substr( p1+1, p2-p1-1 );
    index = tmp.find( ':' );
    std::string file;
    int line = 0;
    if ( index!=std::string::npos ) {
        file = tmp.substr( 0, index );
        line = std::stoi( tmp.substr( index+1 ) );
    } else if ( p1!=std::string::npos ) {
        file = tmp;
    }
    return std::make_tuple(fun,obj,file,line);
}
#endif
// clang-format off
static void getFileAndLine( StackTrace::stack_info &info )
{
    // Note: we could improve performance by combining multiple calls with the same object
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
    #elif defined( USE_MAC )
        void *load_address = loadAddress( info.object );
        if ( load_address==0 )
            return;
        // Call atos to get the object info
        char buf[4096];
        sprintf( buf, "atos -o %s -l %lx %lx 2> /dev/null", info.object.c_str(),
            reinterpret_cast<unsigned long int>( load_address ),
            reinterpret_cast<unsigned long int>( info.address ) );
        FILE *f = popen( buf, "r" );
        if ( f == nullptr )
            return;
        memset( buf, 0, sizeof(buf) );
        fgets( buf, 4095, f );
        //printf("%0x%016llx-output: %s",info.address,buf);
        // Parse the output for function, file and line info
        auto data = split_atos( buf );
        //printf("%0x%016llx-parsed: %s - %s - %s - %i\n",info.address,std::get<0>(data).c_str(),
        //    std::get<1>(data).c_str(),std::get<2>(data).c_str(),std::get<3>(data));
        if ( info.function.empty() )
            info.function = std::get<0>(data);
        if ( info.object.empty() )
            info.object = std::get<1>(data);
        if ( info.filename.empty() )
            info.filename = std::get<2>(data);
        if ( info.line==0 )
            info.line = std::get<3>(data);
        pclose( f );
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
static void signal_handler( int sig )
{
    printf("Signal caught acquiring stack (%i)\n",sig);
    StackTrace::setErrorHandlers( [](std::string,StackTrace::terminateType) { exit( -1 ); } );
}
StackTrace::stack_info StackTrace::getStackInfo( void *address )
{
    return getStackInfo( std::vector<void*>(1,address) )[0];
}
std::vector<StackTrace::stack_info> StackTrace::getStackInfo( const std::vector<void*>& address )
{
    // Temporarily handle signals to prevent recursion on the stack
    auto prev_handler = signal( SIGINT, signal_handler );
    // Get the detailed stack info
    std::vector<StackTrace::stack_info> info(address.size());
    try {
        #ifdef USE_WINDOWS
            IMAGEHLP_SYMBOL64 pSym[1024];
            memset( pSym, 0, sizeof( pSym ) );
            pSym->SizeOfStruct  = sizeof( IMAGEHLP_SYMBOL64 );
            pSym->MaxNameLength = 1024;

            IMAGEHLP_MODULE64 Module;
            memset( &Module, 0, sizeof( Module ) );
            Module.SizeOfStruct = sizeof( Module );

            HANDLE pid = GetCurrentProcess();

            for (size_t i=0; i<address.size(); i++) {
                info[i].address = address[i];
                DWORD64 address2 = reinterpret_cast<DWORD64>( address[i] );
                DWORD64 offsetFromSmybol;
                if ( SymGetSymFromAddr( pid, address2, &offsetFromSmybol, pSym ) != FALSE ) {
                    char name[8192]={0};
                    DWORD rtn = UnDecorateSymbolName( pSym->Name, name, sizeof(name)-1, UNDNAME_COMPLETE );
                    if ( rtn == 0 )
                        info[i].function = std::string(pSym->Name);
                    else
                        info[i].function = std::string(name);
                } else {
                    printf( "ERROR: SymGetSymFromAddr (%d,%p)\n", GetLastError(), address2 );
                }

                // Get line number
                IMAGEHLP_LINE64 Line;
                memset( &Line, 0, sizeof( Line ) );
                Line.SizeOfStruct = sizeof( Line );
                DWORD offsetFromLine;
                if ( SymGetLineFromAddr64( pid, address2, &offsetFromLine, &Line ) != FALSE ) {
                    info[i].line     = Line.LineNumber;
                    info[i].filename = std::string( Line.FileName );
                } else {
                    info[i].line     = 0;
                    info[i].filename = std::string();
                }

                // Get the object
                if ( SymGetModuleInfo64( pid, address2, &Module ) != FALSE ) {
                    //info[i].object = std::string( Module.ModuleName );
                    info[i].object = std::string( Module.LoadedImageName );
                    //info[i].baseOfImage = Module.BaseOfImage;
                }
            }
        #else
            for (size_t i=0; i<address.size(); i++) {
                info[i].address = address[i];
                #if defined(_GNU_SOURCE) || defined(USE_MAC)
                    Dl_info dlinfo;
                    if ( !dladdr( info[i].address, &dlinfo ) ) {
                        getDataFromGlobalSymbols( info[i] );
                        continue;
                    }
                    info[i].address2 = subtractAddress( info[i].address, dlinfo.dli_fbase );
                    info[i].object   = std::string( dlinfo.dli_fname );
                    #if defined( USE_ABI )
                        int status;
                        char *demangled = abi::__cxa_demangle( dlinfo.dli_sname, nullptr, nullptr, &status );
                        if ( status == 0 && demangled != nullptr ) {
                            info[i].function = std::string( demangled );
                        } else if ( dlinfo.dli_sname != nullptr ) {
                            info[i].function = std::string( dlinfo.dli_sname );
                        }
                        free( demangled );
                    #else
                        if ( dlinfo.dli_sname != NULL )
                            info[i].function = std::string( dlinfo.dli_sname );
                    #endif
                #else
                    getDataFromGlobalSymbols( info[i] );
                #endif
            }
            // Get the filename / line number
            for (size_t i=0; i<address.size(); i++) {
                getFileAndLine( info[i] );
            }
        #endif
    } catch ( ... ) {
    }
    signal( SIGINT, prev_handler ) ;
    return info;
}
std::vector<void*> StackTrace::backtrace()
{
    std::vector<void*> trace;
    #if defined( USE_LINUX ) || defined( USE_MAC )
        // Get the trace
        trace.resize(1000,nullptr);
        int trace_size = ::backtrace( trace.data(), trace.size() );
        trace.resize (trace_size );
    #elif defined( USE_WINDOWS )
        #if defined(DBGHELP)

            // Load the modules for the stack trace
            LoadModules();

            // Initialize stackframe for first call
            ::CONTEXT context;
            memset( &context, 0, sizeof( context ) );
            context.ContextFlags = CONTEXT_FULL;
            RtlCaptureContext( &context );
            STACKFRAME64 frame; // in/out stackframe
            memset( &frame, 0, sizeof( frame ) );
            #ifdef _M_IX86
                DWORD imageType = IMAGE_FILE_MACHINE_I386;
                frame.AddrPC.Offset    = context.Eip;
                frame.AddrPC.Mode      = AddrModeFlat;
                frame.AddrFrame.Offset = context.Ebp;
                frame.AddrFrame.Mode   = AddrModeFlat;
                frame.AddrStack.Offset = context.Esp;
                frame.AddrStack.Mode   = AddrModeFlat;
            #elif _M_X64
                DWORD imageType = IMAGE_FILE_MACHINE_AMD64;
                frame.AddrPC.Offset    = context.Rip;
                frame.AddrPC.Mode      = AddrModeFlat;
                frame.AddrFrame.Offset = context.Rsp;
                frame.AddrFrame.Mode   = AddrModeFlat;
                frame.AddrStack.Offset = context.Rsp;
                frame.AddrStack.Mode   = AddrModeFlat;
            #elif _M_IA64
                DWORD imageType = IMAGE_FILE_MACHINE_IA64;
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

            trace.reserve( 1000 );
            auto pid = GetCurrentProcess();
            auto tid = GetCurrentThread();
            for ( int frameNum = 0; frameNum<1024; ++frameNum ) {
                BOOL rtn = StackWalk64( imageType, pid, tid, &frame, &context, readProcMem,
                                        SymFunctionTableAccess, SymGetModuleBase64, NULL );
                if ( !rtn ) {
                    printf( "ERROR: StackWalk64 (%p)\n", frame.AddrPC.Offset );
                    break;
                }

                if ( frame.AddrPC.Offset != 0 )
                    trace.push_back( reinterpret_cast<void*>( frame.AddrPC.Offset ) );

                if ( frame.AddrReturn.Offset == 0 )
                    break;
            }
            SetLastError( ERROR_SUCCESS );
        #endif
    #else
        #warning Stack trace is not supported on this compiler/OS
    #endif
    return trace;
}
std::vector<StackTrace::stack_info> StackTrace::getCallStack()
{
    std::vector<void*> trace = StackTrace::backtrace();
    return getStackInfo(trace);
}
// clang-format on


/****************************************************************************
*  Function to get system search paths                                      *
****************************************************************************/
std::string StackTrace::getSymPaths()
{
    std::string paths;
#ifdef USE_WINDOWS
    // Create the path list (seperated by ';' )
    paths = std::string( ".;" );
    paths.reserve( 1000 );
    // Add the current directory
    paths += getCurrentDirectory() + ";";
    // Now add the path for the main-module:
    char temp[1024];
    memset( temp, 0, sizeof( temp ) );
    if ( GetModuleFileNameA( NULL, temp, sizeof( temp ) - 1 ) > 0 ) {
        for ( char *p = ( temp + strlen( temp ) - 1 ); p >= temp; --p ) {
            // locate the rightmost path separator
            if ( ( *p == '\\' ) || ( *p == '/' ) || ( *p == ':' ) ) {
                *p = 0;
                break;
            }
        }
        if ( strlen( temp ) > 0 ) {
            paths += temp;
            paths += ";";
        }
    }
    memset( temp, 0, sizeof( temp ) );
    if ( GetEnvironmentVariableA( "_NT_SYMBOL_PATH", temp, sizeof( temp ) - 1 ) > 0 ) {
        paths += temp;
        paths += ";";
    }
    memset( temp, 0, sizeof( temp ) );
    if ( GetEnvironmentVariableA( "_NT_ALTERNATE_SYMBOL_PATH", temp, sizeof( temp ) - 1 ) > 0 ) {
        paths += temp;
        paths += ";";
    }
    memset( temp, 0, sizeof( temp ) );
    if ( GetEnvironmentVariableA( "SYSTEMROOT", temp, sizeof( temp ) - 1 ) > 0 ) {
        paths += temp;
        paths += ";";
        // also add the "system32"-directory:
        paths += temp;
        paths += "\\system32;";
    }
    memset( temp, 0, sizeof( temp ) );
    if ( GetEnvironmentVariableA( "SYSTEMDRIVE", temp, sizeof( temp ) - 1 ) > 0 ) {
        paths += "SRV*;" + std::string( temp ) +
                 "\\websymbols*http://msdl.microsoft.com/download/symbols;";
    } else {
        paths += "SRV*c:\\websymbols*http://msdl.microsoft.com/download/symbols;";
    }
#endif
    return paths;
}


/****************************************************************************
*  Load modules for windows                                                 *
****************************************************************************/
#ifdef USE_WINDOWS
BOOL StackTrace::GetModuleListTH32( HANDLE hProcess, DWORD pid )
{
    // CreateToolhelp32Snapshot()
    typedef HANDLE( __stdcall * tCT32S )( DWORD dwFlags, DWORD th32ProcessID );
    // Module32First()
    typedef BOOL( __stdcall * tM32F )( HANDLE hSnapshot, LPMODULEENTRY32 lpme );
    // Module32Next()
    typedef BOOL( __stdcall * tM32N )( HANDLE hSnapshot, LPMODULEENTRY32 lpme );

    // try both dlls...
    const TCHAR *dllname[] = { _T("kernel32.dll"), _T("tlhelp32.dll") };
    HINSTANCE hToolhelp    = NULL;
    tCT32S pCT32S          = NULL;
    tM32F pM32F            = NULL;
    tM32N pM32N            = NULL;

    HANDLE hSnap;
    MODULEENTRY32 me;
    me.dwSize = sizeof( me );

    for ( size_t i = 0; i < ( sizeof( dllname ) / sizeof( dllname[0] ) ); i++ ) {
        hToolhelp = LoadLibrary( dllname[i] );
        if ( hToolhelp == NULL )
            continue;
        pCT32S = (tCT32S) GetProcAddress( hToolhelp, "CreateToolhelp32Snapshot" );
        pM32F  = (tM32F) GetProcAddress( hToolhelp, "Module32First" );
        pM32N  = (tM32N) GetProcAddress( hToolhelp, "Module32Next" );
        if ( ( pCT32S != NULL ) && ( pM32F != NULL ) && ( pM32N != NULL ) )
            break; // found the functions!
        FreeLibrary( hToolhelp );
        hToolhelp = NULL;
    }

    if ( hToolhelp == NULL )
        return FALSE;

    hSnap = pCT32S( TH32CS_SNAPMODULE, pid );
    if ( hSnap == (HANDLE) -1 ) {
        FreeLibrary( hToolhelp );
        return FALSE;
    }

    bool keepGoing = !!pM32F( hSnap, &me );
    int cnt        = 0;
    while ( keepGoing ) {
        LoadModule( hProcess, me.szExePath, me.szModule, (DWORD64) me.modBaseAddr, me.modBaseSize );
        cnt++;
        keepGoing = !!pM32N( hSnap, &me );
    }
    CloseHandle( hSnap );
    FreeLibrary( hToolhelp );
    if ( cnt <= 0 )
        return FALSE;
    return TRUE;
}
DWORD StackTrace::LoadModule(
    HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size )
{
    CHAR *szImg  = _strdup( img );
    CHAR *szMod  = _strdup( mod );
    DWORD result = ERROR_SUCCESS;
    if ( ( szImg == NULL ) || ( szMod == NULL ) )
        result = ERROR_NOT_ENOUGH_MEMORY;
    else {
        if ( SymLoadModule( hProcess, 0, szImg, szMod, baseAddr, size ) == 0 )
            result = GetLastError();
    }
    ULONGLONG fileVersion = 0;
    if ( szImg != NULL ) {
        // try to retrive the file-version:
        VS_FIXEDFILEINFO *fInfo = NULL;
        DWORD dwHandle;
        DWORD dwSize = GetFileVersionInfoSizeA( szImg, &dwHandle );
        if ( dwSize > 0 ) {
            LPVOID vData = malloc( dwSize );
            if ( vData != NULL ) {
                if ( GetFileVersionInfoA( szImg, dwHandle, dwSize, vData ) != 0 ) {
                    UINT len;
                    TCHAR szSubBlock[] = _T("\\");
                    if ( VerQueryValue( vData, szSubBlock, (LPVOID *) &fInfo, &len ) == 0 )
                        fInfo = NULL;
                    else {
                        fileVersion = ( (ULONGLONG) fInfo->dwFileVersionLS ) +
                                      ( (ULONGLONG) fInfo->dwFileVersionMS << 32 );
                    }
                }
                free( vData );
            }
        }

        // Retrive some additional-infos about the module
        IMAGEHLP_MODULE64 Module;
        Module.SizeOfStruct = sizeof( IMAGEHLP_MODULE64 );
        SymGetModuleInfo64( hProcess, baseAddr, &Module );
        LPCSTR pdbName = Module.LoadedImageName;
        if ( Module.LoadedPdbName[0] != 0 )
            pdbName = Module.LoadedPdbName;
    }
    if ( szImg != NULL )
        free( szImg );
    if ( szMod != NULL )
        free( szMod );
    return result;
}
BOOL StackTrace::GetModuleListPSAPI( HANDLE hProcess )
{
    DWORD cbNeeded;
    HMODULE hMods[1024];
    char tt[8192];
    char tt2[8192];
    if ( !EnumProcessModules( hProcess, hMods, sizeof( hMods ), &cbNeeded ) ) {
        return false;
    }
    if ( cbNeeded > sizeof( hMods ) ) {
        printf( "Insufficient memory allocated in GetModuleListPSAPI\n" );
        return false;
    }
    int cnt = 0;
    for ( DWORD i = 0; i < cbNeeded / sizeof( hMods[0] ); i++ ) {
        // base address, size
        MODULEINFO mi;
        GetModuleInformation( hProcess, hMods[i], &mi, sizeof( mi ) );
        // image file name
        tt[0] = 0;
        GetModuleFileNameExA( hProcess, hMods[i], tt, sizeof( tt ) );
        // module name
        tt2[0] = 0;
        GetModuleBaseNameA( hProcess, hMods[i], tt2, sizeof( tt2 ) );
        DWORD dwRes = LoadModule( hProcess, tt, tt2, (DWORD64) mi.lpBaseOfDll, mi.SizeOfImage );
        if ( dwRes != ERROR_SUCCESS )
            printf( "ERROR: LoadModule (%d)\n", dwRes );
        cnt++;
    }

    return cnt != 0;
}
void StackTrace::LoadModules()
{
    static bool modules_loaded = false;
    if ( !modules_loaded ) {
        modules_loaded = true;

        // Get the search paths for symbols
        std::string paths = StackTrace::getSymPaths();

        // Initialize the symbols
        if ( SymInitialize( GetCurrentProcess(), paths.c_str(), FALSE ) == FALSE )
            printf( "ERROR: SymInitialize (%d)\n", GetLastError() );

        DWORD symOptions = SymGetOptions();
        symOptions |= SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS;
        symOptions = SymSetOptions( symOptions );
        char buf[1024] = { 0 };
        if ( SymGetSearchPath( GetCurrentProcess(), buf, sizeof(buf) ) == FALSE )
            printf( "ERROR: SymGetSearchPath (%d)\n", GetLastError() );

        // First try to load modules from toolhelp32
        BOOL loaded = StackTrace::GetModuleListTH32( GetCurrentProcess(), GetCurrentProcessId() );

        // Try to load from Psapi
        if ( !loaded )
            loaded = StackTrace::GetModuleListPSAPI( GetCurrentProcess() );
    }
}
#endif


/****************************************************************************
*  Set the signal handlers                                                  *
****************************************************************************/
static std::function<void(std::string,StackTrace::terminateType)> abort_fun;
static void term_func_abort( int signal )
{
    std::string msg("Caught signal ");
    if ( signal == SIGABRT )
        msg += "SIGABRT";
    else if ( signal == SIGFPE )
        msg += "SIGFPE";
    else if ( signal == SIGILL )
        msg += "SIGILL";
    else if ( signal == SIGINT )
        msg += "SIGINT";
    else if ( signal == SIGSEGV )
        msg += "SIGSEGV";
    else if ( signal == SIGTERM )
        msg += "SIGTERM";
    abort_fun( msg, StackTrace::terminateType::signal );
}
static void term_func()
{
    // Try to re-throw the last error to get the last message
    std::string last_message;
#ifdef USE_LINUX
    try {
        static int tried_throw = 0;
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
    abort_fun( "Unhandled exception:\n" + last_message, StackTrace::terminateType::signal );
}
void StackTrace::setErrorHandlers( std::function<void(std::string,StackTrace::terminateType)> abort )
{
    abort_fun = abort;
    std::set_terminate( term_func );
    signal( SIGABRT, &term_func_abort );
    signal( SIGFPE,  &term_func_abort );
    signal( SIGILL,  &term_func_abort );
    signal( SIGINT,  &term_func_abort );
    signal( SIGSEGV, &term_func_abort );
    signal( SIGTERM, &term_func_abort );
    std::set_unexpected( term_func );
}

