#include "StackTrace/StackTrace.h"
#include "StackTrace/ErrorHandlers.h"
#include "StackTrace/StaticVector.h"
#include "StackTrace/Utilities.h"
#include "StackTrace/Utilities.hpp"
#include "StackTrace/string_view.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>


#define perr std::cerr

// Detect the OS
// clang-format off
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || defined( _MSC_VER )
    #define USE_WINDOWS
    #define NOMINMAX
#elif defined( __APPLE__ )
    #define USE_MAC
    #define USE_NM
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
    #define USE_LINUX
    #define USE_NM
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


#ifdef __GNUC__
    #define USE_ABI
    #include <cxxabi.h>
#endif


using StackTrace::string_view;
using namespace StackTrace::Utilities;


// Mutex for StackTrace opertions that need blocking
static std::mutex StackTrace_mutex;


// Helper thread
static std::shared_ptr<std::thread> globalMonitorThread;


// Function to replace all instances of a string with another
static constexpr size_t replace( char *str, size_t N, size_t pos, size_t len,
                                 const string_view &r ) noexcept
{
    size_t Nr = r.size();
    auto tmp  = str;
    size_t k  = pos;
    for ( size_t i = 0; i < Nr && k < N; i++, k++ )
        str[k] = r[i];
    for ( size_t i = pos + len; i < N && k < N; i++, k++ )
        str[k] = tmp[i];
    for ( size_t m = k; m < N; m++ )
        str[k] = 0;
    return k;
}
template<std::size_t N>
static constexpr size_t replace( std::array<char, N> &str, size_t pos, size_t len,
                                 const string_view &r ) noexcept
{
    return replace( str.data(), N, pos, len, r );
}
static constexpr void strrep( char *str, size_t &N, const string_view &s,
                              const string_view &r ) noexcept
{
    size_t Ns  = s.size();
    size_t pos = string_view( str, N ).find( s );
    while ( pos != std::string::npos ) {
        N   = replace( str, N, pos, Ns, r );
        pos = string_view( str, N ).find( s );
    }
}

static void cleanupFunctionName( char * );


// Functions to hash strings
constexpr uint32_t hashString( const char *s )
{
    uint32_t c    = 0;
    uint32_t hash = 5381;
    while ( ( c = *s++ ) )
        hash = ( ( hash << 5 ) + hash ) ^ c;
    return hash;
}
template<std::size_t N1, std::size_t N2>
static constexpr uint64_t objHash( const std::array<char, N1> &obj,
                                   const std::array<char, N2> &objPath )
{
    uint32_t v1  = hashString( obj.data() );
    uint32_t v2  = hashString( objPath.data() );
    uint64_t key = ( static_cast<uint64_t>( v1 ) << 32 ) + static_cast<uint64_t>( v1 ^ v2 );
    return key;
}


// Inline function to subtract two addresses returning the absolute difference
static inline void *subtractAddress( void *a, void *b ) noexcept
{
    return reinterpret_cast<void *>(
        std::abs( reinterpret_cast<int64_t>( a ) - reinterpret_cast<int64_t>( b ) ) );
}


/****************************************************************************
 *  Windows specific functions                                               *
 ****************************************************************************/
#ifdef USE_WINDOWS
static BOOL __stdcall readProcMem( HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer,
                                   DWORD nSize, LPDWORD lpNumberOfBytesRead )
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
// Create a string with last error message
static std::string getSystemErrorMessage( DWORD error )
{
    if ( error == 0 )
        return ""; // No error
    LPVOID lpMsgBuf;
    DWORD flags =
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD langID = MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT );
    DWORD bufLen = FormatMessage( flags, NULL, error, langID, (LPTSTR) &lpMsgBuf, 0, NULL );
    std::string result;
    const char *whitespaces = " \t\f\v\n\r\0";
    if ( bufLen ) {
        LPCSTR lpMsgStr = (LPCSTR) lpMsgBuf;
        result          = std::string( lpMsgStr, lpMsgStr + bufLen );
        result          = result.substr( 0, result.find_last_not_of( whitespaces ) + 1 );
        LocalFree( lpMsgBuf );
    }
    if ( result.empty() )
        result = std::to_string( static_cast<uint32_t>( error ) );
    return result;
}
namespace StackTrace {
BOOL GetModuleListTH32( HANDLE hProcess, DWORD pid );
BOOL GetModuleListPSAPI( HANDLE hProcess );
DWORD LoadModule( HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size );
void LoadModules();
}; // namespace StackTrace
#endif


/****************************************************************************
 *  Utility to strip the path from a filename                                *
 ****************************************************************************/
static constexpr const char *stripPath( const char *filename ) noexcept
{
    if ( filename == nullptr )
        return nullptr;
    const char *s = filename;
    while ( *s ) {
        if ( *s == 47 || *s == 92 )
            filename = s + 1;
        ++s;
    }
    return filename;
}


/****************************************************************************
 *  Assign a string to a std::array                                          *
 *  Note: this is intended to be a secure copy routine                       *
 ****************************************************************************/
static constexpr size_t strlen2( const char *start ) noexcept
{
    if ( !start )
        return 0;
    const char *end = start;
    while ( *end != '\0' )
        ++end;
    return end - start;
}
template<std::size_t N>
static constexpr size_t strlen2( const std::array<char, N> &s ) noexcept
{
    for ( size_t i = 0; i < N; i++ )
        if ( s[i] == 0 )
            return i;
    return N;
}
template<std::size_t N2>
static constexpr void copy( const char *in, std::array<char, N2> &out ) noexcept
{
    size_t N1 = strlen2( in );
    out.fill( 0 );
    memcpy( out.data(), in, std::min( N1, N2 - 1 ) );
    if ( N1 >= N2 ) {
        out[N2 - 4] = out[N2 - 3] = out[N2 - 2] = '.';
        out[N2 - 1]                             = 0;
    }
}
template<std::size_t N1, std::size_t N2>
static constexpr void copy( const std::array<char, N1> &in, std::array<char, N2> &out ) noexcept
{
    out.fill( 0 );
    size_t N = strlen2( in );
    memcpy( out.data(), in.data(), std::min( N, N2 - 1 ) );
    if ( N >= N2 ) {
        out[N2 - 4] = out[N2 - 3] = out[N2 - 2] = '.';
        out[N2 - 1]                             = 0;
    }
}
template<std::size_t N2, std::size_t N3>
static constexpr void copy( const char *in, std::array<char, N2> &out,
                            std::array<char, N3> &outPath ) noexcept
{
    auto ptr = stripPath( in );
    copy( ptr, out );
    outPath.fill( 0 );
    if ( ptr != in ) {
        size_t N = ptr - in - 1;
        memcpy( outPath.data(), in, std::min( N, N3 - 1 ) );
        if ( N >= N3 ) {
            outPath[N3 - 4] = outPath[N3 - 3] = outPath[N3 - 2] = '.';
            outPath[N3 - 1]                                     = 0;
        }
    }
}
static_assert( strlen2( "" ) == 0 );
static_assert( strlen2( nullptr ) == 0 );
static_assert( stripPath( nullptr ) == nullptr );


/****************************************************************************
 *  Utility to call system command and return output                         *
 ****************************************************************************/
template<std::size_t blockSize>
static void exec2( const char *cmd, staticVector<std::array<char, 512>, blockSize> &out )
{
    out.clear();
    auto fun = [&out]( const char *line ) {
        size_t k = out.size();
        if ( k == blockSize )
            return;
        size_t N = strlen( line );
        N        = std::min<size_t>( N, 512 );
        out.resize( k + 1 );
        out[k].fill( 0 );
        memcpy( out[k].data(), line, N );
        if ( out[k][N - 1] == '\n' )
            out[k][N - 1] = 0;
    };
    exec2( cmd, fun );
}


/****************************************************************************
 *  stack_info                                                               *
 ****************************************************************************/
static_assert( sizeof( StackTrace::stack_info ) <= 512, "Unexpected size for stack_info" );
StackTrace::stack_info::stack_info() { clear(); }
void StackTrace::stack_info::clear()
{
    line     = 0;
    address  = nullptr;
    address2 = nullptr;
    object.fill( 0 );
    objectPath.fill( 0 );
    filename.fill( 0 );
    filenamePath.fill( 0 );
    function.fill( 0 );
}
bool StackTrace::stack_info::operator==( const StackTrace::stack_info &rhs ) const
{
    if ( address == rhs.address )
        return true;
    if ( address2 == rhs.address2 && object == rhs.object )
        return true;
    return false;
}
bool StackTrace::stack_info::operator!=( const StackTrace::stack_info &rhs ) const
{
    return !operator==( rhs );
}
int StackTrace::stack_info::getAddressWidth() const
{
    auto addr = reinterpret_cast<unsigned long long int>( address );
    if ( addr <= 0xFFFF )
        return 4;
    if ( addr <= 0xFFFFFFFF )
        return 8;
    if ( addr <= 0xFFFFFFFFFFFF )
        return 12;
    return 16;
}
std::string StackTrace::stack_info::print( int w1, int w2, int w3 ) const
{
    char out[32 + sizeof( stack_info )];
    print2( out, w1, w2, w3 );
    return std::string( out );
}
void StackTrace::stack_info::print( std::ostream &out, const std::vector<stack_info> &stack,
                                    const std::string &prefix )
{
    char buf[32 + sizeof( stack_info )];
    for ( const auto &tmp : stack ) {
        tmp.print2( buf, 16, 20, 32 );
        out << prefix << buf << std::endl;
    }
}
size_t StackTrace::stack_info::print2( char *out, int w1, int w2, int w3 ) const
{
    char tmp1[32], tmp2[32];
    sprintf( tmp1, "0x%%0%illx:  ", w1 );
    sprintf( tmp2, "%%%is  %%%is", w2, w3 );
    size_t pos = 0;
    pos += sprintf( &out[pos], tmp1, reinterpret_cast<unsigned long long int>( address ) );
    pos += sprintf( &out[pos], tmp2, stripPath( object.data() ), function.data() );
    if ( filename[0] != 0 && line > 0 ) {
        pos += sprintf( &out[pos], "  %s:%u", stripPath( filename.data() ), line );
    } else if ( filename[0] != 0 ) {
        pos += sprintf( &out[pos], "  %s", stripPath( filename.data() ) );
    } else if ( line > 0 ) {
        pos += sprintf( &out[pos], " : %u", line );
    }
    return pos;
}
size_t StackTrace::stack_info::size() const { return sizeof( *this ); }
char *StackTrace::stack_info::pack( char *ptr ) const
{
    memcpy( ptr, this, sizeof( *this ) );
    return ptr + sizeof( *this );
}
const char *StackTrace::stack_info::unpack( const char *ptr )
{
    memcpy( this, ptr, sizeof( *this ) );
    return ptr + sizeof( *this );
}


/****************************************************************************
 *  multi_stack_info                                                         *
 ****************************************************************************/
StackTrace::multi_stack_info::multi_stack_info( const std::vector<stack_info> &rhs )
{
    operator=( rhs );
}
StackTrace::multi_stack_info &
StackTrace::multi_stack_info::operator=( const std::vector<stack_info> &rhs )
{
    clear();
    if ( rhs.empty() )
        return *this;
    N     = 1;
    stack = rhs[0];
    if ( rhs.size() > 1 )
        add( rhs.size() - 1, &rhs[1] );
    return *this;
}
void StackTrace::multi_stack_info::clear()
{
    N = 0;
    stack.clear();
    children.clear();
}
template<class FUN>
void StackTrace::multi_stack_info::print2( int Np, char *prefix, int w[3], bool c, FUN &fun ) const
{
    if ( stack.address != 0 ) {
        prefix[Np] = 0;
        char line[4096];
        int N2 = sprintf( line, "%s[%i] ", prefix, N );
        stack.print2( &line[N2], w[0], w[1], w[2] );
        fun( line );
        prefix[Np++] = c ? '|' : ' ';
        prefix[Np++] = ' ';
    }
    for ( size_t i = 0; i < children.size(); i++ ) {
        bool c2           = children.size() > 1 && i < children.size() - 1 && stack.address != 0;
        const auto &child = children[i];
        child.print2( Np, prefix, w, c2, fun );
    }
}
std::vector<std::string> StackTrace::multi_stack_info::print( const std::string &prefix ) const
{
    std::vector<std::string> text;
    int w[3] = { getAddressWidth(), getObjectWidth(), getFunctionWidth() };
    char prefix2[1024];
    memcpy( prefix2, prefix.data(), prefix.size() );
    auto fun = [&text]( const char *line ) { text.push_back( line ); };
    print2( prefix.size(), prefix2, w, false, fun );
    return text;
}
void StackTrace::multi_stack_info::print( std::ostream &out, const std::string &prefix ) const
{
    int w[3] = { getAddressWidth(), getObjectWidth(), getFunctionWidth() };
    char prefix2[1024];
    memcpy( prefix2, prefix.data(), prefix.size() );
    auto fun = [&out]( const char *line ) { out << line << std::endl; };
    print2( prefix.size(), prefix2, w, false, fun );
}
std::string StackTrace::multi_stack_info::printString( const std::string &prefix ) const
{
    int w[3] = { getAddressWidth(), getObjectWidth(), getFunctionWidth() };
    char prefix2[1024];
    memcpy( prefix2, prefix.data(), prefix.size() );
    std::string out;
    out.reserve( 4096 );
    auto fun = [&out]( const char *line ) {
        out += line;
        out += '\n';
    };
    print2( prefix.size(), prefix2, w, false, fun );
    return out;
}
int StackTrace::multi_stack_info::getAddressWidth() const
{
    int w = stack.getAddressWidth();
    for ( const auto &child : children )
        w = std::max( w, child.getAddressWidth() );
    return w;
}
int StackTrace::multi_stack_info::getObjectWidth() const
{
    int w = std::min<int>( stack.object.size() + 1, 20 );
    for ( const auto &child : children )
        w = std::max( w, child.getObjectWidth() );
    return w;
}
int StackTrace::multi_stack_info::getFunctionWidth() const
{
    int w = std::min<int>( stack.function.size() + 1, 40 );
    for ( const auto &child : children )
        w = std::max( w, child.getFunctionWidth() );
    return w;
}
void StackTrace::multi_stack_info::add( size_t len, const stack_info *stack )
{
    if ( len == 0 )
        return;
    const auto &s = stack[len - 1];
    for ( auto &i : children ) {
        if ( i.stack == s ) {
            i.N++;
            if ( len > 1 )
                i.add( len - 1, stack );
            return;
        }
    }
    children.resize( children.size() + 1 );
    children.back().N     = 1;
    children.back().stack = s;
    if ( len > 1 )
        children.back().add( len - 1, stack );
}
void StackTrace::multi_stack_info::add( const multi_stack_info &rhs )
{
    N += rhs.N;
    for ( const auto &x : rhs.children ) {
        bool found = false;
        for ( auto &tmp : children ) {
            if ( tmp.stack == x.stack ) {
                found = true;
                tmp.add( x );
            }
        }
        if ( !found )
            children.push_back( x );
    }
}
size_t StackTrace::multi_stack_info::size() const
{
    size_t bytes = 2 * sizeof( int ) + stack.size();
    for ( const auto &tmp : children )
        bytes += tmp.size();
    return bytes;
}
char *StackTrace::multi_stack_info::pack( char *ptr ) const
{
    int N2 = N;
    memcpy( ptr, &N2, sizeof( int ) );
    ptr += sizeof( int );
    ptr    = stack.pack( ptr );
    int Nc = children.size();
    memcpy( ptr, &Nc, sizeof( int ) );
    ptr += sizeof( int );
    for ( const auto &tmp : children )
        ptr = tmp.pack( ptr );
    return ptr;
}
const char *StackTrace::multi_stack_info::unpack( const char *ptr )
{
    int N2, Nc;
    memcpy( &N2, ptr, sizeof( int ) );
    ptr += sizeof( int );
    N   = N2;
    ptr = stack.unpack( ptr );
    memcpy( &Nc, ptr, sizeof( int ) );
    ptr += sizeof( int );
    children.resize( Nc );
    for ( auto &tmp : children )
        ptr = tmp.unpack( ptr );
    return ptr;
}


/****************************************************************************
 *  Function to get the executable name                                      *
 ****************************************************************************/
static std::array<char, 1000> getExecutableName()
{
    std::array<char, 1000> exe = { 0 };
    try {
#ifdef USE_LINUX
        char buf[0x10000] = { 0 };
        int len           = ::readlink( "/proc/self/exe", buf, 0x10000 );
        if ( len != -1 ) {
            buf[len] = '\0';
            copy( buf, exe );
        }
#elif defined( USE_MAC )
        uint32_t size     = 0x10000;
        char buf[0x10000] = { 0 };
        if ( _NSGetExecutablePath( buf, &size ) == 0 )
            copy( buf, exe );
#elif defined( USE_WINDOWS )
        DWORD size        = 0x10000;
        char buf[0x10000] = { 0 };
        GetModuleFileName( nullptr, buf, size );
        copy( buf, exe );
#endif
    } catch ( ... ) {
    }
    return exe;
}
static const char *getExecutable2()
{
    static auto execname = getExecutableName();
    return execname.data();
}
std::string StackTrace::getExecutable() { return std::string( getExecutable2() ); }


/****************************************************************************
 * Function to get symbols for the executable from nm (if availible)         *
 * Note: this function maintains an internal cached copy to prevent          *
 *    exccessive calls to nm.  This function also uses a lock to ensure      *
 *    thread safety.                                                         *
 ****************************************************************************/
static_assert( sizeof( StackTrace::symbols_struct ) <= 128, "Unexpected size for symbols_struct" );
std::vector<StackTrace::symbols_struct> global_symbols_data;
static bool global_symbols_loaded = false;
static std::vector<StackTrace::symbols_struct> getSymbolData()
{
    std::vector<StackTrace::symbols_struct> data;
#ifdef USE_NM
    try {
        char cmd[1024];
    #ifdef USE_LINUX
        sprintf( cmd, "nm -n --demangle %s", getExecutable2() );
    #elif defined( USE_MAC )
        sprintf( cmd, "nm -n %s | c++filt", getExecutable2() );
    #else
        #error Unknown OS using nm
    #endif
        // Function to process a line of nm output
        auto fun = [&data]( char *line ) {
            if ( line[0] == ' ' )
                return;
            auto *a = line;
            char *b = strchr( a, ' ' );
            if ( b == nullptr )
                return;
            b[0] = 0;
            b++;
            char *c = strchr( b, ' ' );
            if ( c == nullptr )
                return;
            c[0] = 0;
            c++;
            char *d = strchr( c, '\n' );
            if ( d )
                d[0] = 0;
            size_t add = strtoul( a, nullptr, 16 );
            size_t k   = data.size();
            data.resize( k + 1 );
            data[k].address = reinterpret_cast<void *>( add );
            data[k].type    = b[0];
            copy( c, data[k].obj, data[k].objPath );
        };
        // Call nm
        StackTrace::Utilities::exec2( cmd, fun );
    } catch ( ... ) {
    }
#endif
    return data;
}
std::vector<StackTrace::symbols_struct> StackTrace::getSymbols()
{
    StackTrace_mutex.lock();
    if ( !global_symbols_loaded ) {
        global_symbols_data   = getSymbolData();
        global_symbols_loaded = true;
    }
    auto data = global_symbols_data;
    StackTrace_mutex.unlock();
    return data;
}
void StackTrace::clearSymbols()
{
    StackTrace_mutex.lock();
    if ( global_symbols_loaded ) {
        global_symbols_data   = std::vector<StackTrace::symbols_struct>();
        global_symbols_loaded = false;
    }
    StackTrace_mutex.unlock();
}


/****************************************************************************
 *  Function to get call stack info                                          *
 ****************************************************************************/
#ifdef USE_MAC
static void *loadAddress( const uint32_t &obj_hash )
{
    static std::map<uint32_t, void *> obj_map;
    if ( obj_map.empty() ) {
        uint32_t numImages = _dyld_image_count();
        for ( uint32_t i = 0; i < numImages; i++ ) {
            auto header  = _dyld_get_image_header( i );
            auto name    = _dyld_get_image_name( i );
            auto p       = strrchr( name, '/' );
            auto address = const_cast<struct mach_header *>( header );
            auto hash    = hashString( p + 1 );
            obj_map.insert( std::make_pair( hash, address ) );
        }
    }
    auto it       = obj_map.find( obj_hash );
    void *address = 0;
    if ( it != obj_map.end() ) {
        address = it->second;
    } else {
        it = obj_map.find( obj_hash );
        if ( it != obj_map.end() )
            address = it->second;
    }
    return address;
}
static auto split_atos( const std::string &buf )
{
    int line = 0;
    std::array<char, 2048> fun;
    std::array<char, 64> obj, file, objPath, filePath;
    if ( buf.empty() )
        return std::tie( fun, obj, objPath, file, filePath, line );
    // Get the function
    size_t index = buf.find( " (in " );
    if ( index == std::string::npos ) {
        copy( buf.c_str(), fun );
        cleanupFunctionName( fun.data() );
        return std::tie( fun, obj, objPath, file, filePath, line );
    }
    copy( buf.substr( 0, index ).c_str(), fun );
    cleanupFunctionName( fun.data() );
    std::string tmp = buf.substr( index + 5 );
    // Get the object
    index = tmp.find( ')' );
    copy( tmp.substr( 0, index ).c_str(), obj, objPath );
    tmp = tmp.substr( index + 1 );
    // Get the filename and line number
    size_t p1 = tmp.find( '(' );
    size_t p2 = tmp.find( ')' );
    tmp       = tmp.substr( p1 + 1, p2 - p1 - 1 );
    index     = tmp.find( ':' );
    if ( index != std::string::npos ) {
        copy( tmp.substr( 0, index ).c_str(), file, filePath );
        line = std::stoi( tmp.substr( index + 1 ) );
    } else if ( p1 != std::string::npos ) {
        copy( tmp.c_str(), file, filePath );
    }
    return std::tie( fun, obj, objPath, file, filePath, line );
}
#endif
template<std::size_t blockSize>
static void getFileAndLineObject( staticVector<StackTrace::stack_info *, blockSize> &info )
{
    if ( info.empty() )
        return;
// This gets the file and line numbers for multiple stack lines in the same object
#if defined( USE_LINUX )
    // Create the call command
    uint32_t N;
    char cmd[4096];
    static_assert( sizeof( unsigned long ) == sizeof( size_t ), "Unxpected size for ul" );
    if ( info[0]->objectPath[0] == 0 )
        N = sprintf( cmd, "addr2line -C -e %s -f", info[0]->object.data() );
    else
        N = sprintf( cmd, "addr2line -C -e %s/%s -f", info[0]->objectPath.data(),
                     info[0]->object.data() );
    for ( size_t i = 0; i < info.size() && N < sizeof( cmd ) - 32; i++ ) {
        N += sprintf( &cmd[N], " %lx %lx", reinterpret_cast<unsigned long>( info[i]->address ),
                      reinterpret_cast<unsigned long>( info[i]->address2 ) );
    }
    N += sprintf( &cmd[N], " 2> /dev/null" );
    // Get the function/line/file
    staticVector<std::array<char, 512>, 4 * blockSize> output;
    exec2( cmd, output );
    if ( output.size() != 4 * info.size() )
        return;
    // Add the results to info
    for ( size_t i = 0; i < info.size(); i++ ) {
        char *tmp1 = output[4 * i + 0].data();
        char *tmp2 = output[4 * i + 1].data();
        if ( tmp1[0] == '?' && tmp1[1] == '?' ) {
            tmp1 = output[4 * i + 2].data();
            tmp2 = output[4 * i + 3].data();
        }
        if ( tmp1[0] == '?' && tmp1[1] == '?' ) {
            continue;
        }
        // get function name
        if ( info[i]->function.empty() ) {
            cleanupFunctionName( tmp1 );
            copy( tmp1, info[i]->function );
        }
        // get file and line
        char *buf = tmp2;
        if ( buf[0] != '?' && buf[0] != 0 ) {
            size_t j = 0;
            for ( j = 0; j < 4095 && buf[j] != ':'; j++ ) {}
            buf[j] = 0;
            copy( buf, info[i]->filename, info[i]->filenamePath );
            info[i]->line = atoi( &buf[j + 1] );
        }
    }
#elif defined( USE_MAC )
    // Create the call command
    void *load_address = loadAddress( hashString( info[0]->object.data() ) );
    if ( load_address == nullptr )
        return;
    // Call atos to get the object info
    uint32_t N;
    char cmd[4096];
    static_assert( sizeof( unsigned long ) == sizeof( size_t ), "Unxpected size for ul" );
    auto addr = reinterpret_cast<unsigned long>( load_address );
    if ( info[0]->objectPath[0] == 0 )
        N = sprintf( cmd, "atos -o %s -f -l %lx", info[0]->object.data(), addr );
    else
        N = sprintf( cmd, "atos -o %s/%s -f -l %lx", info[0]->objectPath.data(),
                     info[0]->object.data(), addr );
    for ( size_t i = 0; i < info.size() && N < sizeof( cmd ) - 32; i++ )
        N += sprintf( &cmd[N], " %lx", reinterpret_cast<unsigned long>( info[i]->address ) );
    N += sprintf( &cmd[N], " 2> /dev/null" );
    // Get the function/line/file
    staticVector<std::array<char, 512>, blockSize> output;
    exec2( cmd, output );
    if ( output.size() != info.size() )
        return;
    // Parse the output for function, file and line info
    for ( size_t i = 0; i < info.size(); i++ ) {
        auto data = split_atos( output[2 * i].data() );
        if ( info[i]->function.empty() )
            copy( std::get<0>( data ), info[i]->function );
        if ( info[i]->object.empty() ) {
            copy( std::get<1>( data ), info[i]->object );
            copy( std::get<2>( data ), info[i]->objectPath );
        }
        if ( info[i]->filename.empty() ) {
            copy( std::get<3>( data ), info[i]->filename );
            copy( std::get<4>( data ), info[i]->filenamePath );
        }
        if ( info[i]->line == 0 )
            info[i]->line = std::get<5>( data );
    }
#endif
}
static void getFileAndLine( size_t N, StackTrace::stack_info *info )
{
    // Limit block size to prevent blowing out the stack
    constexpr size_t blockSize = 256;
    // Operate on blocks
    size_t i0 = 0;
    while ( i0 < N ) {
        // Get a list of objects
        staticVector<uint64_t, blockSize> objectHash;
        for ( size_t i = i0; i < N && i - i0 < blockSize; i++ )
            objectHash.insert( objHash( info[i].object, info[i].objectPath ) );
        // For each object, get the file/line numbers for all entries
        for ( const auto &hash : objectHash ) {
            staticVector<StackTrace::stack_info *, blockSize> list;
            for ( size_t i = i0; i < N && i - i0 < blockSize; i++ ) {
                if ( objHash( info[i].object, info[i].objectPath ) == hash )
                    list.push_back( &info[i] );
            }
            getFileAndLineObject( list );
        }
        i0 = std::min( N, i0 + blockSize );
    }
}
// Try to use the global symbols to decode info about the stack
static void getDataFromGlobalSymbols( StackTrace::stack_info &info )
{
    if ( !global_symbols_loaded ) {
        global_symbols_data   = getSymbolData();
        global_symbols_loaded = true;
    }
    const auto &data = global_symbols_data;
    if ( !data.empty() ) {
        // Find the closest address
        size_t lower = 0;
        size_t upper = data.size() - 1;
        while ( ( upper - lower ) != 1 ) {
            size_t value = ( upper + lower ) / 2;
            if ( data[value].address >= info.address )
                upper = value;
            else
                lower = value; // Create a string with last error message
        }
        if ( upper > 0 ) {
            copy( data[lower].obj, info.object );
            copy( data[lower].objPath, info.objectPath );
        } else {
            copy( getExecutable2(), info.object, info.objectPath );
        }
    }
}
static void signal_handler( int sig )
{
    printf( "Signal caught acquiring stack (%i)\n", sig );
    StackTrace::setErrorHandler( []( const StackTrace::abort_error &err ) {
        std::cerr << err.what();
        exit( -1 );
    } );
}
#ifdef USE_WINDOWS
// Get the stack info for a single address
static void getStackInfo( StackTrace::stack_info &info )
{
    // Allocate a buffer large enough to hold the symbol information on the stack and get
    // a pointer to the buffer.  We also have to set the size of the symbol structure itself
    // and the number of bytes reserved for the name.
    constexpr uint64_t nameMaxSize = 2048;
    constexpr uint64_t bufferSize  = ( sizeof( SYMBOL_INFO ) + nameMaxSize + 7 ) / 8;
    uint64_t buffer[bufferSize]    = { 0 };
    SYMBOL_INFO *pSym              = (SYMBOL_INFO *) buffer;
    pSym->SizeOfStruct             = sizeof( SYMBOL_INFO );
    pSym->MaxNameLen               = nameMaxSize;
    HANDLE pid                     = GetCurrentProcess();
    DWORD64 address2 = reinterpret_cast<DWORD64>( info.address );
    DWORD64 offsetFromSymbol = 0;
    if ( SymFromAddr( pid, address2, &offsetFromSymbol, pSym ) != FALSE ) {
        char name[2048] = { 0 };
        DWORD rtn = UnDecorateSymbolName( pSym->Name, name, sizeof( name ) - 1, UNDNAME_COMPLETE );
        if ( rtn != 0 ) {
            cleanupFunctionName( pSym->Name );
            copy( pSym->Name, info.function );
        }
    }

    // Get line number
    IMAGEHLP_LINE64 Line;
    memset( &Line, 0, sizeof( Line ) );
    Line.SizeOfStruct = sizeof( Line );
    DWORD offsetFromLine;
    if ( SymGetLineFromAddr64( pid, address2, &offsetFromLine, &Line ) != FALSE ) {
        info.line = Line.LineNumber;
        copy( Line.FileName, info.filename, info.filenamePath );
    } else {
        info.line = 0;
        copy( nullptr, info.filename, info.filenamePath );
    }

    // Get the object
    IMAGEHLP_MODULE64 Module;
    memset( &Module, 0, sizeof( Module ) );
    Module.SizeOfStruct = sizeof( Module );
    if ( SymGetModuleInfo64( pid, address2, &Module ) != FALSE ) {
        copy( Module.LoadedImageName, info.object, info.objectPath );
    }
}
#elif defined( _GNU_SOURCE ) || defined( USE_MAC )
// Get the stack info for a single address
static void getStackInfo( StackTrace::stack_info &info )
{
    Dl_info dlinfo;
    if ( !dladdr( info.address, &dlinfo ) ) {
        getDataFromGlobalSymbols( info );
        return;
    }
    info.address2 = subtractAddress( info.address, dlinfo.dli_fbase );
    copy( dlinfo.dli_fname, info.object, info.objectPath );
    // clang-format off
    #if defined( USE_ABI )
        int status;
        char *demangled = abi::__cxa_demangle( dlinfo.dli_sname, nullptr, nullptr, &status );
        if ( status == 0 && demangled != nullptr ) {
            cleanupFunctionName( demangled );
            copy( demangled, info.function );
        } else if ( dlinfo.dli_sname != nullptr ) {
            copy( dlinfo.dli_sname, info.function );
        }
        free( demangled );
    #endif
    // clang-format on
    if ( dlinfo.dli_sname != nullptr && info.function[0] == 0 ) {
        std::array<char, 4096> tmp;
        copy( dlinfo.dli_sname, tmp );
        cleanupFunctionName( tmp.data() );
        copy( tmp, info.function );
    }
}
#else
// Get the stack info for a single address
static void getStackInfo( StackTrace::stack_info &info ) { getDataFromGlobalSymbols( info ); }
#endif
// Get the call stack info for the address stack
static void getStackInfo2( size_t N, void *const *address, StackTrace::stack_info *info )
{
    // Temporarily handle signals to prevent recursion on the stack
    auto prev_handler = signal( SIGINT, signal_handler );
    // Get the detailed stack info
    for ( size_t i = 0; i < N; i++ ) {
        info[i].clear();
        info[i].address = address[i];
        try {
            getStackInfo( info[i] );
        } catch ( ... ) {
        }
    }
    // Get the filename / line numbers for each item on the stack
    getFileAndLine( N, info );
    signal( SIGINT, prev_handler );
}
StackTrace::stack_info StackTrace::getStackInfo( void *address )
{
    StackTrace::stack_info info;
    getStackInfo2( 1, &address, &info );
    return info;
}
std::vector<StackTrace::stack_info> StackTrace::getStackInfo( const std::vector<void *> &address )
{
    std::vector<StackTrace::stack_info> info( address.size() );
    getStackInfo2( address.size(), address.data(), info.data() );
    return info;
}


/****************************************************************************
 *  Helper functions for controlling interal signals                         *
 ****************************************************************************/
static int backtrace_thread( const std::thread::native_handle_type &, void **, size_t );
#if defined( USE_LINUX ) || defined( USE_MAC )
static int global_thread_backtrace_count;
static void *global_thread_backtrace[1000];
static void _callstack_signal_handler( int, siginfo_t *, void * )
{
    global_thread_backtrace_count =
        backtrace_thread( StackTrace::thisThread(), global_thread_backtrace, 1000 );
}
static int get_thread_callstack_signal()
{
    if ( 39 >= SIGRTMIN && 39 <= SIGRTMAX )
        return 39;
    return std::min<int>( SIGRTMIN + 4, SIGRTMAX );
}
int thread_callstack_signal = get_thread_callstack_signal();
#else
int thread_callstack_signal = 0;
#endif


/****************************************************************************
 *  Function to get the backtrace                                            *
 ****************************************************************************/
static int backtrace_thread( const std::thread::native_handle_type &tid, void **buffer,
                             size_t size )
{
    int count = 0;
#if defined( USE_LINUX ) || defined( USE_MAC )
    // Get the trace
    if ( tid == pthread_self() ) {
        count = ::backtrace( buffer, size );
    } else {
        // Note: this will get the backtrace, but terminates the thread in the process!!!
        StackTrace_mutex.lock();
        struct sigaction sa;
        sigfillset( &sa.sa_mask );
        sa.sa_flags     = SA_SIGINFO;
        sa.sa_sigaction = _callstack_signal_handler;
        sigaction( thread_callstack_signal, &sa, nullptr );
        global_thread_backtrace_count = -1;
        pthread_kill( tid, thread_callstack_signal );
        auto t1 = std::chrono::high_resolution_clock::now();
        auto t2 = std::chrono::high_resolution_clock::now();
        while ( global_thread_backtrace_count == -1 &&
                std::chrono::duration<double>( t2 - t1 ).count() < 0.15 ) {
            std::this_thread::yield();
            t2 = std::chrono::high_resolution_clock::now();
        }
        count = std::max( global_thread_backtrace_count, 0 );
        memcpy( buffer, global_thread_backtrace, count * sizeof( void * ) );
        global_thread_backtrace_count = -1;
        StackTrace_mutex.unlock();
    }
#elif defined( USE_WINDOWS )
    #if defined( DBGHELP )

    // Load the modules for the stack trace
    StackTrace::LoadModules();

    // Initialize stackframe for first call
    ::CONTEXT context;
    memset( &context, 0, sizeof( context ) );
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext( &context );
    STACKFRAME64 frame; // in/out stackframe
    memset( &frame, 0, sizeof( frame ) );
        #ifdef _M_IX86
    DWORD imageType        = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = context.Eip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode   = AddrModeFlat;
        #elif _M_X64
    DWORD imageType        = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = context.Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rsp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;
        #elif _M_IA64
    DWORD imageType         = IMAGE_FILE_MACHINE_IA64;
    frame.AddrPC.Offset     = context.StIIP;
    frame.AddrPC.Mode       = AddrModeFlat;
    frame.AddrFrame.Offset  = context.IntSp;
    frame.AddrFrame.Mode    = AddrModeFlat;
    frame.AddrBStore.Offset = context.RsBSP;
    frame.AddrBStore.Mode   = AddrModeFlat;
    frame.AddrStack.Offset  = context.IntSp;
    frame.AddrStack.Mode    = AddrModeFlat;
        #else
    static bool print = true;
    if ( print ) {
        std::cerr << "Stack trace is not supported on Windows with this architecture\n";
        print = false;
    }
        #endif
    auto pid = GetCurrentProcess();
    for ( int frameNum = 0; frameNum < 1024; ++frameNum ) {
        BOOL rtn = StackWalk64( imageType, pid, tid, &frame, &context, readProcMem,
                                SymFunctionTableAccess, SymGetModuleBase64, NULL );
        if ( !rtn ) {
            printf( "ERROR: StackWalk64 (%p)\n", (void *) frame.AddrPC.Offset );
            break;
        }
        if ( frame.AddrPC.Offset != 0 ) {
            buffer[count] = reinterpret_cast<void *>( frame.AddrPC.Offset );
            count++;
        }
        if ( frame.AddrReturn.Offset == 0 )
            break;
    }
    SetLastError( ERROR_SUCCESS );
    #endif
#else
    static bool print = true;
    if ( print ) {
        std::cerr << "Stack trace is not supported on this compiler/OS\n";
        print = false;
    }
#endif
    return count;
}
std::vector<void *> StackTrace::backtrace( std::thread::native_handle_type tid )
{
    std::vector<void *> trace( 1000, nullptr );
    size_t count = backtrace_thread( tid, trace.data(), trace.size() );
    trace.resize( count );
    return trace;
}
std::vector<void *> StackTrace::backtrace()
{
    std::vector<void *> trace( 1000, nullptr );
    size_t count = backtrace_thread( thisThread(), trace.data(), trace.size() );
    trace.resize( count );
    return trace;
}
std::vector<std::vector<void *>> StackTrace::backtraceAll()
{
    // Get the list of threads
    auto threads = registeredThreads();
    // Get the backtrace of each thread
    std::vector<std::vector<void *>> trace( threads.size() );
    for ( size_t i = 0; i < threads.size(); i++ ) {
        trace[i].resize( 1000 );
        size_t count = backtrace_thread( threads[i], trace[i].data(), trace[i].size() );
        trace[i].resize( count );
    }
    return trace;
}


/****************************************************************************
 *  Function to get the current call stack                                   *
 ****************************************************************************/
std::vector<StackTrace::stack_info> StackTrace::getCallStack()
{
    void *trace[1000];
    size_t count = backtrace_thread( thisThread(), trace, 1000 );
    std::vector<StackTrace::stack_info> info( count );
    getStackInfo2( count, trace, info.data() );
    return info;
}
std::vector<StackTrace::stack_info> StackTrace::getCallStack( std::thread::native_handle_type id )
{
    void *trace[1000];
    size_t count = backtrace_thread( id, trace, 1000 );
    std::vector<StackTrace::stack_info> info( count );
    getStackInfo2( count, trace, info.data() );
    return info;
}
static std::vector<std::vector<StackTrace::stack_info>>
generateStacks( const std::vector<std::vector<void *>> &trace )
{
    // Function to find an address
    auto find = []( const auto &data, auto x ) {
        for ( size_t i = 0; i < data.size(); i++ ) {
            if ( data[i] == x )
                return static_cast<int>( i );
        }
        return -1;
    };
    // Get the stack data for all pointers
    std::vector<void *> addresses;
    addresses.reserve( 1024 );
    for ( const auto &tmp : trace ) {
        for ( auto ptr : tmp ) {
            if ( find( addresses, ptr ) == -1 )
                addresses.push_back( ptr );
        }
    }
    auto stack_data = StackTrace::getStackInfo( addresses );
    // Create the stack traces
    std::vector<std::vector<StackTrace::stack_info>> stack( trace.size() );
    for ( size_t i = 0; i < trace.size(); i++ ) {
        // Create the stack for the given thread trace
        stack[i].resize( trace[i].size() );
        for ( size_t j = 0; j < trace[i].size(); j++ ) {
            int k       = find( addresses, trace[i][j] );
            stack[i][j] = stack_data[k];
        }
    }
    return stack;
}
static StackTrace::multi_stack_info
generateMultiStack( const std::vector<std::vector<void *>> &trace )
{
    // Get the stack data for all pointers
    auto stack = generateStacks( trace );
    // Create the multi-stack trace
    StackTrace::multi_stack_info multistack;
    multistack.N = stack.size();
    for ( const auto &tmp : stack )
        multistack.add( tmp.size(), tmp.data() );
    return multistack;
}
static StackTrace::multi_stack_info
generateMultiStack( const std::vector<std::thread::native_handle_type> &threads )
{
    // Get the stack data for all pointers
    std::vector<std::vector<void *>> trace( threads.size() );
    auto it = threads.begin();
    for ( size_t i = 0; i < threads.size(); i++, ++it )
        trace[i] = StackTrace::backtrace( *it );
    // Create the multi-stack trace
    return generateMultiStack( trace );
}
StackTrace::multi_stack_info StackTrace::getAllCallStacks()
{
    // Get the list of active thread
    auto threads = registeredThreads();
    // Create the multi-stack structure
    auto stack = generateMultiStack( threads );
    return stack;
}


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
    if ( GetModuleFileNameA( nullptr, temp, sizeof( temp ) - 1 ) > 0 ) {
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
    HINSTANCE hToolhelp    = nullptr;
    tCT32S pCT32S          = nullptr;
    tM32F pM32F            = nullptr;
    tM32N pM32N            = nullptr;

    HANDLE hSnap;
    MODULEENTRY32 me;
    me.dwSize = sizeof( me );

    for ( size_t i = 0; i < ( sizeof( dllname ) / sizeof( dllname[0] ) ); i++ ) {
        hToolhelp = LoadLibrary( dllname[i] );
        if ( hToolhelp == nullptr )
            continue;
        pCT32S = (tCT32S) GetProcAddress( hToolhelp, "CreateToolhelp32Snapshot" );
        pM32F  = (tM32F) GetProcAddress( hToolhelp, "Module32First" );
        pM32N  = (tM32N) GetProcAddress( hToolhelp, "Module32Next" );
        if ( ( pCT32S != nullptr ) && ( pM32F != nullptr ) && ( pM32N != nullptr ) )
            break; // found the functions!
        FreeLibrary( hToolhelp );
        hToolhelp = nullptr;
    }

    if ( hToolhelp == nullptr )
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
DWORD StackTrace::LoadModule( HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr,
                              DWORD size )
{
    CHAR *szImg  = _strdup( img );
    CHAR *szMod  = _strdup( mod );
    DWORD result = ERROR_SUCCESS;
    if ( ( szImg == nullptr ) || ( szMod == nullptr ) ) {
        result = ERROR_NOT_ENOUGH_MEMORY;
    } else {
        if ( SymLoadModule( hProcess, 0, szImg, szMod, baseAddr, size ) == 0 )
            result = GetLastError();
    }
    ULONGLONG fileVersion = 0;
    if ( szImg != nullptr ) {
        // try to retrive the file-version:
        VS_FIXEDFILEINFO *fInfo = nullptr;
        DWORD dwHandle;
        DWORD dwSize = GetFileVersionInfoSizeA( szImg, &dwHandle );
        if ( dwSize > 0 ) {
            LPVOID vData = malloc( dwSize );
            if ( vData != nullptr ) {
                if ( GetFileVersionInfoA( szImg, dwHandle, dwSize, vData ) != 0 ) {
                    UINT len;
                    TCHAR szSubBlock[] = _T("\\");
                    if ( VerQueryValue( vData, szSubBlock, (LPVOID *) &fInfo, &len ) == 0 ) {
                        fInfo = nullptr;
                    } else {
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
    if ( szImg != nullptr )
        free( szImg );
    if ( szMod != nullptr )
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
            printf( "ERROR: LoadModule (%s)\n", getSystemErrorMessage( dwRes ).data() );
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
            printf( "ERROR: SymInitialize (%s)\n", getSystemErrorMessage( GetLastError() ).data() );

        DWORD symOptions = SymGetOptions();
        symOptions |= SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS;
        symOptions     = SymSetOptions( symOptions );
        char buf[1024] = { 0 };
        if ( SymGetSearchPath( GetCurrentProcess(), buf, sizeof( buf ) ) == FALSE )
            printf( "ERROR: SymGetSearchPath (%s)\n",
                    getSystemErrorMessage( GetLastError() ).data() );

        // First try to load modules from toolhelp32
        BOOL loaded = StackTrace::GetModuleListTH32( GetCurrentProcess(), GetCurrentProcessId() );

        // Try to load from Psapi
        if ( !loaded )
            loaded = StackTrace::GetModuleListPSAPI( GetCurrentProcess() );
    }
}
#endif


/****************************************************************************
 *  Get the signal name                                                      *
 ****************************************************************************/
#ifdef USE_WINDOWS
static char *strsignal( int sig )
{
    if ( sig == SIGABRT )
        return "Abnormal termination";
    else if ( sig == SIGFPE )
        return "Floating-point error";
    else if ( sig == SIGILL )
        return "Illegal instruction";
    else if ( sig == SIGINT )
        return "CTRL+C signal";
    else if ( sig == SIGSEGV )
        return "Illegal storage access";
    else if ( sig == SIGTERM )
        return "Termination request";
    else
        return "Unknown";
}
#endif
static std::array<char, 64> signalNames[128];
const char *StackTrace::signalName( int sig )
{
    static bool initialized = false;
    if ( !initialized ) {
        StackTrace_mutex.lock();
        for ( int i = 0; i < 128; i++ )
            copy( strsignal( i + 1 ), signalNames[i] );
        StackTrace_mutex.unlock();
        initialized = true;
    }
    bool valid = sig > 0 && sig <= 128;
    return valid ? signalNames[sig - 1].data() : nullptr;
}
std::vector<int> StackTrace::allSignalsToCatch()
{
    std::vector<int> signals;
#ifdef USE_WINDOWS
    signals = { SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV, SIGTERM };
#else
    signals.reserve( SIGRTMAX );
    for ( int i = 1; i < 32; i++ ) {
        if ( i == SIGKILL || i == SIGSTOP )
            continue;
        signals.push_back( i );
    }
    for ( int i = SIGRTMIN; i <= SIGRTMAX; i++ ) {
        if ( i == SIGKILL || i == SIGSTOP )
            continue;
        signals.push_back( i );
    }
#endif
    return signals;
}
template<class TYPE>
static inline void erase( std::vector<TYPE> &x, TYPE y )
{
    auto it = std::remove( x.begin(), x.end(), y );
    x.erase( it, x.end() );
}
std::vector<int> StackTrace::defaultSignalsToCatch()
{
    auto signals = allSignalsToCatch();
#ifndef USE_WINDOWS
    erase( signals, SIGWINCH );  // Don't catch window changed by default
    erase( signals, SIGCONT );   // Don't catch continue by default
    erase( signals, SIGCHLD );   // Don't catch child exited by default
    erase( signals, SIGALRM );   // Don't catch alarm signals by default
    erase( signals, SIGVTALRM ); // Don't catch virtual alarm signals by default
    erase( signals, SIGPROF );   // Don't catch profile signals by default
#endif
    return signals;
}


/****************************************************************************
 *  Set the signal handlers                                                  *
 ****************************************************************************/
static std::function<void( StackTrace::abort_error &err )> abort_fun;
StackTrace::abort_error rethrow()
{
    StackTrace::abort_error error;
    try {
        static int tried_throw = 0;
        if ( tried_throw == 0 ) {
            tried_throw = 1;
            throw;
        }
        // No active exception
    } catch ( const StackTrace::abort_error &err ) {
        // Caught a std::runtime_error
        error = err;
    } catch ( const std::exception &err ) {
        // Caught a std::runtime_error
        error.message = err.what();
    } catch ( ... ) {
        // Caught an unknown exception
        error.message = "Unknown exception";
    }
    if ( error.type == StackTrace::terminateType::unknown )
        error.type = StackTrace::terminateType::exception;
    if ( error.bytes == 0 )
        error.bytes = StackTrace::Utilities::getMemoryUsage();
    if ( error.stack.empty() ) {
        error.stackType = StackTrace::printStackType::local;
        error.stack     = StackTrace::backtrace();
    }
    return error;
}
void StackTrace::terminateFunctionSignal( int sig )
{
    StackTrace::abort_error err;
    err.type      = StackTrace::terminateType::signal;
    err.signal    = sig;
    err.bytes     = StackTrace::Utilities::getMemoryUsage();
    err.stack     = StackTrace::backtrace();
    err.stackType = StackTrace::getDefaultStackType();
    abort_fun( err );
}
static bool signals_set[256] = { false };
static void term_func()
{
    auto err = rethrow();
    StackTrace::clearSignals();
    abort_fun( err );
}
static void null_term_func() {}
void StackTrace::clearSignal( int sig )
{
    if ( signals_set[sig] ) {
        signal( sig, SIG_DFL );
        signals_set[sig] = false;
    }
}
void StackTrace::clearSignals( const std::vector<int> &signals )
{
    for ( auto sig : signals ) {
        signal( sig, SIG_DFL );
        signals_set[sig] = false;
    }
}
void StackTrace::clearSignals()
{
    for ( size_t i = 0; i < sizeof( signals_set ); i++ ) {
        if ( signals_set[i] ) {
            signal( i, SIG_DFL );
            signals_set[i] = false;
        }
    }
}
void StackTrace::setSignals( const std::vector<int> &signals, void ( *handler )( int ) )
{
    for ( auto sig : signals ) {
        signal( sig, handler );
        signals_set[sig] = true;
    }
    std::this_thread::yield();
}
void StackTrace::raiseSignal( int signal ) { std::raise( signal ); }
void StackTrace::setErrorHandler( std::function<void( StackTrace::abort_error & )> abort,
                                  const std::vector<int> &signals )
{
    abort_fun = abort;
    std::set_terminate( term_func );
    setSignals( signals, &terminateFunctionSignal );
    // std::set_unexpected( term_func );  // Deprecated
}
void StackTrace::clearErrorHandler()
{
    abort_fun = []( const StackTrace::abort_error & ) {};
    std::set_terminate( null_term_func );
    clearSignals();
    // std::set_unexpected( term_func );  // Deprecated
}


/****************************************************************************
 *  Functions to handle MPI errors                                           *
 ****************************************************************************/
#ifdef USE_MPI
static bool MPI_Initialized()
{
    int initialized = 0, finalized = 0;
    MPI_Initialized( &initialized );
    MPI_Finalized( &finalized );
    return initialized != 0 && finalized == 0;
}
static std::shared_ptr<MPI_Errhandler> mpierr;
static void MPI_error_handler_fun( MPI_Comm *comm, int *err, ... )
{
    if ( *err == MPI_ERR_COMM && *comm == MPI_COMM_WORLD ) {
        // Special error handling for an invalid MPI_COMM_WORLD
        std::cerr << "Error invalid MPI_COMM_WORLD";
        exit( -1 );
    }
    int msg_len        = 0;
    char message[1000] = { 0 };
    MPI_Error_string( *err, message, &msg_len );
    StackTrace::abort_error error;
    error.message   = std::string( message );
    error.type      = StackTrace::terminateType::MPI;
    error.bytes     = StackTrace::Utilities::getMemoryUsage();
    error.stack     = StackTrace::backtrace();
    error.stackType = StackTrace::printStackType::global;
    throw error;
}
void StackTrace::setMPIErrorHandler( MPI_Comm comm )
{
    if ( !MPI_Initialized() )
        return;
    if ( mpierr.get() == nullptr ) {
        mpierr = std::make_shared<MPI_Errhandler>();
        MPI_Comm_create_errhandler( MPI_error_handler_fun, mpierr.get() );
    }
    MPI_Comm_set_errhandler( comm, *mpierr );
}
void StackTrace::clearMPIErrorHandler( MPI_Comm comm )
{
    if ( !MPI_Initialized() )
        return;
    if ( mpierr.get() != nullptr )
        MPI_Errhandler_free( mpierr.get() ); // Delete the error handler
    mpierr.reset();
    MPI_Comm_set_errhandler( comm, MPI_ERRORS_ARE_FATAL );
}
#endif


/****************************************************************************
 *  Global call stack functionality                                          *
 ****************************************************************************/
#ifdef USE_MPI
static MPI_Comm globalCommForGlobalCommStack  = MPI_COMM_NULL;
static volatile int globalMonitorThreadStatus = -1;
static void runGlobalMonitorThread()
{
    int rank = 0;
    int size = 1;
    MPI_Comm_size( globalCommForGlobalCommStack, &size );
    MPI_Comm_rank( globalCommForGlobalCommStack, &rank );
    while ( globalMonitorThreadStatus == 1 ) {
        // Check for any messages
        int flag = 0;
        MPI_Status status;
        int err = MPI_Iprobe( MPI_ANY_SOURCE, 1, globalCommForGlobalCommStack, &flag, &status );
        if ( err != MPI_SUCCESS ) {
            printf( "Internal error in StackTrace::getGlobalCallStacks::runGlobalMonitorThread\n" );
            break;
        } else if ( flag != 0 ) {
            // We received a request
            int src_rank = status.MPI_SOURCE;
            int tag;
            MPI_Recv( &tag, 1, MPI_INT, src_rank, 1, globalCommForGlobalCommStack, &status );
            // Get the list of threads (except this)
            auto threads = StackTrace::registeredThreads();
            if ( threads.empty() )
                continue;
            // Get the stack info for the threads
            auto multistack = generateMultiStack( threads );
            // Pack and send the data
            size_t bytes = multistack.size();
            char *data   = new char[bytes];
            multistack.pack( data );
            MPI_Send( data, bytes, MPI_CHAR, src_rank, tag, globalCommForGlobalCommStack );
            delete[] data;
        } else {
            // No requests recieved
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        }
    }
}
void StackTrace::globalCallStackInitialize( MPI_Comm comm )
{
    globalMonitorThreadStatus = 3;
    // Check that we have the necessary MPI thread support
    if ( !MPI_Initialized() ) {
        printf( "Warning: MPI not initialized before calling globalCallStackInitialize\n" );
        return;
    }
    int rank = 0;
    MPI_Comm_rank( comm, &rank );
    int provided;
    MPI_Query_thread( &provided );
    if ( provided != MPI_THREAD_MULTIPLE ) {
        if ( rank == 0 )
            printf( "Warning: getGlobalCallStacks requires support for MPI_THREAD_MULTIPLE\n" );
        return;
    }
    // Check that we have support to get call stacks from threads
    int N_threads = 0;
    if ( rank == 0 ) {
        std::thread thread( StackTrace::Utilities::sleep_ms, 200 );
        std::this_thread::yield();
        auto thread_ids = registeredThreads();
        N_threads       = thread_ids.size();
        thread.join();
    }
    MPI_Bcast( &N_threads, 1, MPI_INT, 0, comm );
    if ( N_threads == 1 ) {
        if ( rank == 0 )
            printf( "Warning: getAllCallStacks not supported on this OS\n" );
        return;
    }
    // Create the communicator and initialize the helper thread
    globalMonitorThreadStatus = 1;
    MPI_Comm_dup( comm, &globalCommForGlobalCommStack );
    globalMonitorThread.reset( new std::thread( runGlobalMonitorThread ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
}
void StackTrace::globalCallStackFinalize()
{
    if ( globalMonitorThread ) {
        globalMonitorThreadStatus = 2;
        globalMonitorThread->join();
        globalMonitorThread.reset();
    }
    if ( globalCommForGlobalCommStack != MPI_COMM_NULL )
        MPI_Comm_free( &globalCommForGlobalCommStack );
    globalCommForGlobalCommStack = MPI_COMM_NULL;
}
StackTrace::multi_stack_info getRemoteCallStacks()
{
    if ( globalMonitorThreadStatus == -1 ) {
        // User did not call globalCallStackInitialize
        printf( "Warning: getGlobalCallStacks called without call to globalCallStackInitialize\n" );
        return StackTrace::multi_stack_info();
    } else if ( globalMonitorThreadStatus != 1 ) {
        // globalCallStackInitialize is not supported
        return StackTrace::multi_stack_info();
    }
    // Signal all processes that we want their stack for all threads
    int rank = 0;
    int size = 1;
    MPI_Comm_size( globalCommForGlobalCommStack, &size );
    MPI_Comm_rank( globalCommForGlobalCommStack, &rank );
    std::random_device rd;
    std::mt19937 gen( rd() );
    std::uniform_int_distribution<> dis( 2, 0x7FFF );
    int tag = dis( gen );
    std::vector<MPI_Request> sendRequest( size );
    for ( int i = 0; i < size; i++ ) {
        if ( i == rank )
            continue;
        MPI_Isend( &tag, 1, MPI_INT, i, 1, globalCommForGlobalCommStack, &sendRequest[i] );
    }
    // Recieve the backtrace for all remote processes/threads
    int N_finished        = 1;
    auto start            = std::chrono::steady_clock::now();
    double time           = 0;
    const double max_time = 10.0 + size * 20e-3;
    StackTrace::multi_stack_info multistack;
    while ( N_finished < size && time < max_time ) {
        int flag = 0;
        MPI_Status status;
        int err = MPI_Iprobe( MPI_ANY_SOURCE, tag, globalCommForGlobalCommStack, &flag, &status );
        if ( err != MPI_SUCCESS ) {
            printf( "Internal error in StackTrace::getGlobalCallStacks\n" );
            break;
        } else if ( flag != 0 ) {
            // We recieved a response
            int src_rank = status.MPI_SOURCE;
            int count;
            MPI_Get_count( &status, MPI_CHAR, &count );
            char *data = new char[count];
            MPI_Recv( data, count, MPI_CHAR, src_rank, tag, globalCommForGlobalCommStack, &status );
            StackTrace::multi_stack_info tmp;
            tmp.unpack( data );
            delete[] data;
            multistack.add( tmp );
            N_finished++;
        } else {
            auto stop = std::chrono::steady_clock::now();
            time      = std::chrono::duration_cast<std::chrono::seconds>( stop - start ).count();
            std::this_thread::yield();
        }
    }
    for ( int i = 0; i < size; i++ ) {
        if ( i == rank )
            continue;
        MPI_Request_free( &sendRequest[i] );
    }
    return multistack;
}
#else
StackTrace::multi_stack_info getRemoteCallStacks() { return StackTrace::multi_stack_info(); }
#endif
StackTrace::multi_stack_info StackTrace::getGlobalCallStacks()
{
    auto threads    = registeredThreads();
    auto multistack = generateMultiStack( threads );
    multistack.add( getRemoteCallStacks() );
    return multistack;
}


/****************************************************************************
 *  Cleanup the call stack                                                   *
 ****************************************************************************/
static constexpr size_t findMatching( const char *str, size_t N, size_t pos ) noexcept
{
    size_t pos2 = pos + 1;
    int count   = 1;
    while ( count != 0 && pos2 < N ) {
        if ( str[pos2] == '<' )
            count++;
        if ( str[pos2] == '>' )
            count--;
        pos2++;
    }
    return pos2;
}
static void cleanupFunctionName( char *function )
{
    constexpr size_t npos = std::string::npos;
    // First find the string length
    size_t N = strlen( function );
    // Cleanup template space
    strrep( function, N, " >", ">" );
    strrep( function, N, "< ", "<" );
    // Remove std::__1::
    strrep( function, N, "std::__1::", "std::" );
    // Replace std::ratio with abbriviated version
    auto find = [&function, &N]( const string_view &str, size_t pos = 0 ) {
        return string_view( function, N ).find( str, pos );
    };
    if ( find( "std::ratio<" ) != npos ) {
        strrep( function, N, "std::ratio<1l, 1000000000000000000000000l>", "std::yocto" );
        strrep( function, N, "std::ratio<1l, 1000000000000000000000l>", "std::zepto" );
        strrep( function, N, "std::ratio<1l, 1000000000000000000l>", "std::atto" );
        strrep( function, N, "std::ratio<1l, 1000000000000000l>", "std::femto" );
        strrep( function, N, "std::ratio<1l, 1000000000000l>", "std::pico" );
        strrep( function, N, "std::ratio<1l, 1000000000l>", "std::nano" );
        strrep( function, N, "std::ratio<1l, 1000000l>", "std::micro" );
        strrep( function, N, "std::ratio<1l, 1000l>", "std::milli" );
        strrep( function, N, "std::ratio<1l, 100l>", "std::centi" );
        strrep( function, N, "std::ratio<1l, 10l>", "std::deci" );
        strrep( function, N, "std::ratio<1l, 1l>", "" );
        strrep( function, N, "std::ratio<10l, 1l>", "std::deca" );
        strrep( function, N, "std::ratio<60l, 1l>", "std::ratio<60>" );
        strrep( function, N, "std::ratio<100l, 1l>", "std::hecto" );
        strrep( function, N, "std::ratio<1000l, 1l>", "std::kilo" );
        strrep( function, N, "std::ratio<3600l, 1l>", "std::ratio<3600>" );
        strrep( function, N, "std::ratio<1000000l, 1l>", "std::mega" );
        strrep( function, N, "std::ratio<1000000000l, 1l>", "std::giga" );
        strrep( function, N, "std::ratio<1000000000000l, 1l>", "std::tera" );
        strrep( function, N, "std::ratio<1000000000000000l, 1l>", "std::peta" );
        strrep( function, N, "std::ratio<1000000000000000000l, 1l>", "std::exa" );
        strrep( function, N, "std::ratio<1000000000000000000000l, 1l>", "std::zetta" );
        strrep( function, N, "std::ratio<1000000000000000000000000l, 1l>", "std::yotta" );
        strrep( function, N, " >", ">" );
        strrep( function, N, "< ", "<" );
    }
    // Replace std::chrono::duration with abbriviated version
    if ( find( "std::chrono::duration<" ) != npos ) {
        // clang-format off
        strrep( function, N, "std::chrono::duration<long, std::nano>", "std::chrono::nanoseconds" );
        strrep( function, N, "std::chrono::duration<long, std::micro>", "std::chrono::microseconds" );
        strrep( function, N, "std::chrono::duration<long, std::milli>", "std::chrono::milliseconds" );
        strrep( function, N, "std::chrono::duration<long>", "std::chrono::seconds" );
        strrep( function, N, "std::chrono::duration<long,>", "std::chrono::seconds" );
        strrep( function, N, "std::chrono::duration<long, std::ratio<60>>", "std::chrono::minutes" );
        strrep( function, N, "std::chrono::duration<long, std::ratio<3600>>", "std::chrono::hours" );
        strrep( function, N, " >", ">" );
        strrep( function, N, "< ", "<" );
        // clang-format on
    }
    // Replace std::this_thread::sleep_for with abbriviated version.
    if ( find( "::sleep_for<" ) != npos ) {
        strrep( function, N, "::sleep_for<long, std::nano>", "::sleep_for<nanoseconds>" );
        strrep( function, N, "::sleep_for<long, std::micro>", "::sleep_for<microseconds>" );
        strrep( function, N, "::sleep_for<long, std::milli>", "::sleep_for<milliseconds>" );
        strrep( function, N, "::sleep_for<long>", "::sleep_for<seconds>" );
        strrep( function, N, "::sleep_for<long,>", "::sleep_for<seconds>" );
        strrep( function, N, "::sleep_for<long, std::ratio<60>>", "::sleep_for<minutes>" );
        strrep( function, N, "::sleep_for<long, std::ratio<3600>>", "::sleep_for<hours>" );
        strrep( function, N, "::sleep_for<nanoseconds>(std::chrono::nanoseconds",
                "::sleep_for(std::chrono::nanoseconds" );
        strrep( function, N, "::sleep_for<microseconds>(std::chrono::microseconds",
                "::sleep_for(std::chrono::microseconds" );
        strrep( function, N, "::sleep_for<milliseconds>(std::chrono::milliseconds",
                "::sleep_for(std::chrono::milliseconds" );
        strrep( function, N, "::sleep_for<seconds>(std::chrono::seconds",
                "::sleep_for(std::chrono::seconds" );
        strrep( function, N, "::sleep_for<milliseconds>(std::chrono::minutes",
                "::sleep_for(std::chrono::milliseconds" );
        strrep( function, N, "::sleep_for<milliseconds>(std::chrono::hours",
                "::sleep_for(std::chrono::hours" );
    }
    // Replace std::basic_string with abbriviated version
    strrep( function, N, "std::__cxx11::basic_string<", "std::basic_string<" );
    size_t pos = 0;
    while ( pos < N ) {
        // Find next instance of std::basic_string
        pos = find( "std::basic_string<", pos );
        if ( pos == npos )
            break;
        // Find the matching >
        size_t pos1 = pos + 17;
        size_t pos2 = findMatching( function, N, pos1 );
        if ( pos2 == pos1 )
            break;
        if ( strncmp( &function[pos1 + 1], "char", 4 ) == 0 )
            N = replace( function, N, pos, pos2 - pos, "std::string" );
        else if ( strncmp( &function[pos1 + 1], "wchar_t", 7 ) == 0 )
            N = replace( function, N, pos, pos2 - pos, "std::wstring" );
        else if ( strncmp( &function[pos1 + 1], "char16_t", 8 ) == 0 )
            N = replace( function, N, pos, pos2 - pos, "std::u16string" );
        else if ( strncmp( &function[pos1 + 1], "char32_t", 8 ) == 0 )
            N = replace( function, N, pos, pos2 - pos, "std::u32string" );
        pos++;
    }
    // Replace std::make_shared with abbriviated version
    if ( find( "std::make_shared<" ) != npos ) {
        size_t pos1 = find( "std::make_shared<" );
        size_t pos2 = find( ",", pos1 );
        size_t pos3 = find( "(", pos1 );
        N           = replace( function, N, pos2, pos3 - pos2, ">" );
    }
    // Remove std::allocator in std::vector
    if ( find( "std::vector<" ) != npos ) {
        size_t pos1 = find( "std::vector<" );
        size_t pos2 = find( ", std::allocator", pos1 );
        size_t pos3 = findMatching( function, N, pos1 + 11 );
        N           = replace( function, N, pos2, pos3 - pos2, ">" );
    }
}
static bool keep( const StackTrace::stack_info &info )
{
    const size_t npos = std::string::npos;
    string_view object( info.object.data() );
    string_view function( info.function.data() );
    string_view filename( info.filename.data() );
    // Remove backtrace_thread from StackTrace.cpp
    if ( filename == "StackTrace.cpp" && function.find( "backtrace_thread" ) != npos )
        return false;
    // Remove libc functions
    if ( object.find( "libc.so" ) != npos ) {
        // Remove __libc_start_main
        if ( function.find( "__libc_start_main" ) != npos )
            return false;
    }
    // Remove libc++ functions
    if ( object.find( "libstdc++" ) != npos ) {
        // Remove std::this_thread::__sleep_for
        if ( function.find( "std::this_thread::__sleep_for(" ) != npos )
            return false;
    }
    // Remove pthread functions
    if ( object.find( "libpthread" ) != npos ) {
        // Remove __restore_rt
        if ( function.find( "__restore_rt" ) != npos && object.find( "libpthread" ) != npos )
            return false;
    }
    // Remove condition_variable functions
    if ( filename == "condition_variable" ) {
        // Remove std::condition_variable::__wait_until_impl
        if ( function.find( "std::condition_variable::__wait_until_impl" ) != npos )
            return false;
    }
    // Remove std::function references
    if ( filename == "functional" ) {
        if ( function.find( "std::_Function_handler<" ) != npos ||
             function.find( "std::_Bind_simple<" ) != npos || function.find( "_M_invoke" ) != npos )
            return false;
    }
    // Remove std::thread::_Impl
    if ( filename == "thread" ) {
        if ( function.find( "std::thread::_Impl<" ) != npos ||
             function.find( "std::thread::_Invoker<" ) != npos )
            return false;
    }
    if ( filename == "invoke.h" ) {
        if ( function.find( "std::__invoke_impl" ) != npos ||
             function.find( "std::__invoke_result" ) != npos )
            return false;
    }
    // Remove pthread internals
    if ( function == "__GI___pthread_timedjoin_ex" )
        return false;
    // Remove MPI internal routines
    if ( function == "MPIR_Barrier_impl" || function == "MPIR_Barrier_intra" ||
         function == "MPIC_Sendrecv" )
        return false;
    // Remove OpenMPI specific internal routines
    if ( function == "opal_libevent2022_event_set_log_callback" ||
         function == "opal_libevent2022_event_base_loop" )
        return false;
    // Remove MATLAB internal routines
    if ( object == "libmwmcr.so" || object == "libmwm_lxe.so" || object == "libmwbridge.so" ||
         object == "libmwiqm.so" || object == "libmwm_dispatcher.so" || object == "libmwmvm.so" ||
         object.find( "libPocoNetSSL.so" ) != std::string::npos )
        return false;
    // Remove std::shared_ptr functions
    if ( filename == "shared_ptr.h" ) {
        if ( function.find( "> std::allocate_shared<" ) != npos ||
             function.find( "std::_Sp_make_shared_tag," ) != npos )
            return false;
    }
    if ( filename == "shared_ptr_base.h" )
        return false;
    // Remove new_allocator functions
    if ( filename == "new_allocator.h" )
        return false;
    // Remove alloc_traits functions
    if ( filename == "alloc_traits.h" )
        return false;
    // Remove gthr-default functions
    if ( filename == "gthr-default.h" )
        return false;
    // Remove entries with no useful information
    if ( function.empty() && filename.empty() )
        return false;
    return true;
}
void StackTrace::cleanupStackTrace( multi_stack_info &stack )
{
    auto it           = stack.children.begin();
    const size_t npos = std::string::npos;
    while ( it != stack.children.end() ) {
        string_view object( it->stack.object.data() );
        string_view function( it->stack.function.data() );
        string_view filename( it->stack.filename.data() );
        // Remove callstack (and all children) for threads that are just contributing
        if ( filename == "StackTrace.cpp" ) {
            bool test = function.find( "_callstack_signal_handler" ) != npos ||
                        function.find( "getGlobalCallStacks" ) != npos ||
                        function.find( "backtrace" ) != npos || function.find( "(" ) == npos;
            if ( test ) {
                it = stack.children.erase( it );
                continue;
            }
        }
        // Remove libc fgets children
        if ( object.find( "libc.so" ) != npos ) {
            if ( function.find( "fgets" ) != npos )
                it->children.clear();
        }
        // Identify if we want to print the current item
        bool remove_entry = !keep( it->stack );
        // Remove the desired entry
        if ( remove_entry ) {
            if ( it->children.empty() ) {
                it = stack.children.erase( it );
                continue;
            } else if ( it->children.size() == 1 ) {
                *it = it->children[0];
                continue;
            }
        }
        // Cleanup the children
        cleanupStackTrace( *it );
        // Combine any children with the same address (can occur when we remove items)
        bool remove = false;
        for ( auto it2 = stack.children.begin(); it2 != it; it2++ ) {
            if ( it->stack == it2->stack ) {
                remove = true;
                it2->N += it->N;
                for ( auto &tmp : it->children )
                    it2->children.push_back( tmp );
                cleanupStackTrace( *it2 );
            }
        }
        if ( remove ) {
            it = stack.children.erase( it );
            continue;
        }
        ++it;
    }
}


/****************************************************************************
 *  Generate stack from string                                               *
 ****************************************************************************/
static StackTrace::stack_info parseLine( const char *str )
{
    char tmp[1000];
    StackTrace::stack_info stack;
    // Load the address
    const char *p0 = strchr( str, 0 );
    const char *p1 = strchr( str, 'x' );
    const char *p2 = strchr( str, ':' );
    memset( tmp, 0, sizeof( tmp ) );
    memcpy( tmp, p1 + 1, p2 - p1 - 1 );
    uint64_t address = strtol( tmp, nullptr, 16 );
    stack.address    = reinterpret_cast<void *>( address );
    stack.address2   = stack.address;
    // Load object, function, file
    const char *p3 = p2 + 1;
    while ( *p3 == ' ' )
        p3++;
    if ( *p3 == 0 )
        return stack;
    const char *p4 = strstr( p3, "  " );
    const char *p5 = nullptr;
    if ( p4 != nullptr ) {
        while ( *p4 == ' ' )
            p4++;
        p5 = strstr( p4, "  " );
        if ( p5 != nullptr ) {
            while ( *p5 == ' ' )
                p5++;
        }
    }
    if ( p5 == nullptr ) {
        if ( p3 - p2 > 20 ) {
            p5 = p4;
            p4 = p3;
        }
    }
    if ( p4 == nullptr )
        p4 = p0;
    if ( p5 == nullptr )
        p5 = p0;
    // Load line
    const char *p6 = strchr( p5, ':' );
    if ( p6 == nullptr )
        p6 = p0;
    // Store the results
    auto copyField = []( const char *p1, const char *p2, auto &field ) {
        field.fill( 0 );
        memcpy( field.data(), p1, std::min<int>( p2 - p1, field.size() ) );
        for ( int i = field.size() - 1; i > 0 && ( field[i] == ' ' || field[i] == 0 ); i-- )
            field[i] = 0;
    };
    copyField( p3, p4, stack.object );
    copyField( p4, p5, stack.function );
    copyField( p5, p6, stack.filename );
    if ( p6 != p0 )
        stack.line = atoi( p6 + 1 );
    return stack;
}
StackTrace::multi_stack_info StackTrace::generateFromString( const std::string &str )
{
    // Break the string according to line breaks
    std::vector<std::string> data;
    size_t p1 = 0;
    size_t p2 = str.find( '\n' );
    while ( p2 != std::string::npos ) {
        data.push_back( str.substr( p1, p2 - p1 ) );
        p1 = p2 + 1;
        p2 = str.find( '\n', p1 );
    }
    data.push_back( str.substr( p1 ) );
    // Generate the stack
    return generateFromString( data );
}
StackTrace::multi_stack_info StackTrace::generateFromString( const std::vector<std::string> &text )
{
    // Get the data from the text
    std::vector<int> indent;
    std::vector<multi_stack_info> stack;
    for ( const auto &str : text ) {
        auto p1 = str.find( '[' );
        auto p2 = str.find( ']' );
        auto p3 = str.find( 'x' );
        if ( p3 == std::string::npos )
            continue;
        multi_stack_info tmp;
        tmp.N = 1;
        if ( p1 < p2 && p1 < p3 )
            tmp.N = std::stoi( str.substr( p1 + 1, p2 - p1 - 1 ) );
        tmp.stack = parseLine( &str[p3 - 1] );
        indent.push_back( std::min( p1, p3 - 1 ) );
        stack.push_back( tmp );
    }
    // Generate the stack hierarchy
    multi_stack_info stack2;
    std::vector<std::pair<int, std::vector<multi_stack_info> *>> map;
    map.emplace_back( 0, &stack2.children );
    for ( size_t i = 0; i < stack.size(); i++ ) {
        while ( indent[i] < map.back().first )
            map.resize( map.size() - 1 );
        if ( indent[i] == map.back().first ) {
            map.back().second->push_back( stack[i] );
        } else {
            map.back().second->back().children.push_back( stack[i] );
            map.emplace_back( indent[i], &map.back().second->back().children );
        }
    }
    return stack2;
}


/****************************************************************************
 *  abort_error                                                              *
 ****************************************************************************/
StackTrace::abort_error::abort_error()
    : type( terminateType::unknown ), stackType( printStackType::local ), signal( 0 ), bytes( 0 )
{
}
const char *StackTrace::abort_error::what() const noexcept
{
    d_msg.clear();
    if ( type == terminateType::abort ) {
        d_msg += "Program abort called";
    } else if ( type == terminateType::signal ) {
        d_msg += "Unhandled signal (" + std::to_string( signal ) + ") caught";
    } else if ( type == terminateType::exception ) {
        d_msg += "Unhandled exception caught";
    } else if ( type == terminateType::MPI ) {
        d_msg += "Error calling MPI routine";
    } else {
        d_msg += "Unknown error called";
    }
    string_view filename( source.file_name() );
    if ( !filename.empty() ) {
        d_msg += " in file '" + std::string( filename ) + "'";
        if ( source.line() > 0 ) {
            d_msg += " at line " + std::to_string( source.line() );
        }
    }
    d_msg += ":\n";
    d_msg += "   " + message + "\n";
    if ( bytes > 0 ) {
        d_msg += "Bytes used = " + std::to_string( bytes ) + "\n";
    }
    if ( !stack.empty() && stackType != printStackType::none ) {
        d_msg += "Stack Trace:\n";
        if ( stackType == printStackType::local ) {
            for ( const auto &item : getStackInfo( stack ) ) {
                if ( !keep( item ) )
                    continue;
                char txt[1000];
                item.print2( txt );
                d_msg += " \n";
                d_msg += txt;
            }
        } else if ( stackType == printStackType::threaded || stackType == printStackType::global ) {
            // Get the call stack
            std::vector<std::vector<void *>> trace;
            trace.push_back( stack );
            // Get the call stack for all threads except the current one
            auto threads = StackTrace::registeredThreads();
            erase( threads, thisThread() );
            for ( auto tid : threads )
                trace.push_back( backtrace( tid ) );
            // Generate call stack
            auto multistack = generateMultiStack( trace );
            // Add remote call stack info
            if ( stackType == printStackType::global )
                multistack.add( getRemoteCallStacks() );
            // Cleanup call stack
            cleanupStackTrace( multistack );
            // Print the results
            d_msg += multistack.printString( " " );
        } else {
            d_msg += "Unknown value for stackType\n";
        }
    }
    for ( size_t i = 0; i < d_msg.size(); i++ )
        if ( d_msg[i] == 0 )
            d_msg.erase( i, 1 );
    return d_msg.c_str();
}


/****************************************************************************
 * Get/Set default stack type                                                *
 ****************************************************************************/
static StackTrace::printStackType abort_stackType = StackTrace::printStackType::global;
void StackTrace::setDefaultStackType( StackTrace::printStackType type ) { abort_stackType = type; }
StackTrace::printStackType StackTrace::getDefaultStackType() { return abort_stackType; }
