// This is a temporary replacement for std::source_location until we support C++20
#ifndef included_StackTrace_source_location
#define included_StackTrace_source_location


namespace StackTrace {


struct source_location {
public:
    source_location() : d_file( "" ), d_func( "" ), d_line( 0 ), d_col( 0 ) {}

    constexpr source_location(
        const char* file, const char* func, uint32_t line, uint32_t col ) noexcept
        : d_file( file ), d_func( func ), d_line( line ), d_col( col )
    {
    }

    // 14.1.3, source_location field access
    constexpr uint32_t line() const noexcept { return d_line; }
    constexpr uint32_t column() const noexcept { return d_col; }
    constexpr const char* file_name() const noexcept { return d_file; }
    constexpr const char* function_name() const noexcept { return d_func; }

private:
    const char* d_file;
    const char* d_func;
    uint32_t d_line;
    uint32_t d_col;
};


} // namespace StackTrace


#define SOURCE_LOCATION_CURRENT() StackTrace::source_location( __FILE__, __func__, __LINE__, 0 )


#endif
