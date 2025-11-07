#ifndef included_StackTraceMacros
#define included_StackTraceMacros

// clang-format off
#if @SET_USE_MPI@
    #define STACKTRACE_USE_MPI
#endif
// clang-format on

// Disable/Enable warnings
// clang-format off
#ifndef STACKTRACE_DISABLE_WARNINGS
#if defined( _MSC_VER )
    #define STACKTRACE_DISABLE_WARNINGS __pragma( warning( push, 0 ) )
    #define STACKTRACE_ENABLE_WARNINGS __pragma( warning( pop ) )
#elif defined( __clang__ )
    #define STACKTRACE_DISABLE_WARNINGS                                           \
        _Pragma( "clang diagnostic push" )                                        \
        _Pragma( "clang diagnostic ignored \"-Wall\"" )                           \
        _Pragma( "clang diagnostic ignored \"-Wextra\"" )                         \
        _Pragma( "clang diagnostic ignored \"-Wunused-private-field\"" )          \
        _Pragma( "clang diagnostic ignored \"-Wdeprecated-declarations\"" )       \
        _Pragma( "clang diagnostic ignored \"-Winteger-overflow\"" )              \
        _Pragma( "clang diagnostic ignored \"-Winconsistent-missing-override\"" ) \
        _Pragma( "clang diagnostic ignored \"-Wimplicit-int-float-conversion\"" )
    #define STACKTRACE_ENABLE_WARNINGS _Pragma( "clang diagnostic pop" )
#elif defined( __INTEL_COMPILER )
    #if defined( __INTEL_LLVM_COMPILER )
        #define STACKTRACE_DISABLE_WARNINGS _Pragma( "warning (push)" )
        #define STACKTRACE_ENABLE_WARNINGS _Pragma( "warning(pop)" )
    #else
        #define STACKTRACE_DISABLE_WARNINGS   \
            _Pragma( "warning (push)" )       \
            _Pragma( "warning disable 488" )  \
            _Pragma( "warning disable 1011" ) \
            _Pragma( "warning disable 61" )   \
            _Pragma( "warning disable 1478" ) \
            _Pragma( "warning disable 488" )  \
            _Pragma( "warning disable 2651" )
        #define STACKTRACE_ENABLE_WARNINGS _Pragma( "warning(pop)" )
    #endif
#elif defined( __GNUC__ )
    #define STACKTRACE_DISABLE_WARNINGS                                   \
        _Pragma( "GCC diagnostic push" )                                  \
        _Pragma( "GCC diagnostic ignored \"-Wpragmas\"" )                 \
        _Pragma( "GCC diagnostic ignored \"-Wall\"" )                     \
        _Pragma( "GCC diagnostic ignored \"-Wextra\"" )                   \
        _Pragma( "GCC diagnostic ignored \"-Wpedantic\"" )                \
        _Pragma( "GCC diagnostic ignored \"-Wunused-local-typedefs\"" )   \
        _Pragma( "GCC diagnostic ignored \"-Woverloaded-virtual\"" )      \
        _Pragma( "GCC diagnostic ignored \"-Wunused-parameter\"" )        \
        _Pragma( "GCC diagnostic ignored \"-Wdeprecated-copy\"" )         \
        _Pragma( "GCC diagnostic ignored \"-Wdeprecated-declarations\"" ) \
        _Pragma( "GCC diagnostic ignored \"-Wvirtual-move-assign\"" )     \
        _Pragma( "GCC diagnostic ignored \"-Wunused-function\"" )         \
        _Pragma( "GCC diagnostic ignored \"-Woverflow\"" )                \
        _Pragma( "GCC diagnostic ignored \"-Wunused-variable\"" )         \
        _Pragma( "GCC diagnostic ignored \"-Wignored-qualifiers\"" )      \
        _Pragma( "GCC diagnostic ignored \"-Wenum-compare\"" )            \
        _Pragma( "GCC diagnostic ignored \"-Wsign-compare\"" )            \
        _Pragma( "GCC diagnostic ignored \"-Wterminate\"" )               \
        _Pragma( "GCC diagnostic ignored \"-Wimplicit-fallthrough\"" )    \
        _Pragma( "GCC diagnostic ignored \"-Wmaybe-uninitialized\"" )     \
        _Pragma( "GCC diagnostic ignored \"-Winaccessible-base\"" )       \
        _Pragma( "GCC diagnostic ignored \"-Wclass-memaccess\"" )         \
        _Pragma( "GCC diagnostic ignored \"-Wcast-function-type\"" )      \
        _Pragma( "GCC diagnostic ignored \"-Waggressive-loop-optimizations\"" )
    #define STACKTRACE_ENABLE_WARNINGS _Pragma( "GCC diagnostic pop" )
#else
    #define STACKTRACE_DISABLE_WARNINGS
    #define STACKTRACE_ENABLE_WARNINGS
#endif
#endif
// clang-format on

#endif
