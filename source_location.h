// This is a temporary replacement for std::source_location until we require C++20
#ifndef included_StackTrace_source_location
#define included_StackTrace_source_location

#include "StackTrace/std.h"

#include <cstdint>
#include <cstdio>

#if STACKTRACE_CXX_STANDARD >= 20
    #include <version>
    #if __cpp_lib_source_location
        #include <source_location>
    #endif
#endif


namespace StackTrace {


struct source_location {
public:
    constexpr source_location() : d_file( "" ), d_func( "" ), d_line( 0 ), d_col( 0 ) {}

    constexpr source_location( const char* file, const char* func, uint32_t line,
                               uint32_t col ) noexcept
        : d_file( file ), d_func( func ), d_line( line ), d_col( col )
    {
    }

    // 14.1.3, source_location field access
    constexpr uint32_t line() const noexcept { return d_line; }
    constexpr uint32_t column() const noexcept { return d_col; }
    constexpr const char* file_name() const noexcept { return d_file; }
    constexpr const char* function_name() const noexcept { return d_func; }

    // Attempt to mimic std::source_location
#if __cpp_lib_source_location
    static constexpr const char* method() { return "std::source_location"; }
    static constexpr source_location
    current( std::source_location loc = std::source_location::current() ) noexcept
    {
        return source_location( loc.file_name(), loc.function_name(), loc.line(), loc.column() );
    }
#elif ( defined( __GNUC__ ) || defined( __clang__ ) ) && \
    !( defined( __INTEL_COMPILER ) || defined( __ibmxl__ ) )
    static constexpr const char* method() { return "GNU"; }
    static constexpr source_location current( const char* file = __builtin_FILE(),
                                              const char* func = __builtin_FUNCTION(),
                                              uint32_t line    = __builtin_LINE() ) noexcept
    {
        return source_location( file, func, line, 0 );
    }
#else
    static constexpr const char* method() { return "default"; }
    static constexpr source_location current() noexcept { return source_location(); }
#endif

    // Check if the structure is empty
    // Note: this is only supported until we switch to std::source_location at which time
    //    it is unnecessary since there is no empty constructor and std::source_location::current()
    //    will always have the correct (complete) information.
    constexpr bool empty() const { return d_file[0] == 0 && d_line == 0; }

private:
    const char* d_file;
    const char* d_func;
    uint32_t d_line;
    uint32_t d_col;
};


} // namespace StackTrace


#if __cpp_lib_source_location
    #define SOURCE_LOCATION_CURRENT() StackTrace::source_location::current()
#elif ( defined( __GNUC__ ) || defined( __clang__ ) ) && \
    !( defined( __INTEL_COMPILER ) || defined( __ibmxl__ ) )
    #define SOURCE_LOCATION_CURRENT() StackTrace::source_location::current()
#else
    #define SOURCE_LOCATION_CURRENT() StackTrace::source_location( __FILE__, __func__, __LINE__, 0 )
#endif


#endif
