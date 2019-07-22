#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>


#include "StackTrace/ErrorHandlers.h"
#include "StackTrace/StackTrace.h"
#include "StackTrace/Utilities.h"


#ifdef USE_TIMER
#include "MemoryApp.h"
#include "ProfilerApp.h"
#endif


using StackTrace::Utilities::time;


// Include MPI
// clang-format off
#ifdef USE_MPI
    #include "mpi.h"
    int getRank() {
        int rank = 0;
        MPI_Comm_rank( MPI_COMM_WORLD, &rank );
        return rank;
    }
    void barrier() { MPI_Barrier( MPI_COMM_WORLD ); }
    int sumReduce( int x ) {
        int y;
        MPI_Allreduce( &x, &y, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD );
        return y;
    }
    int startup( int argc, char *argv[] ) {
        int provided_thread_support;
        MPI_Init_thread( &argc, &argv, MPI_THREAD_MULTIPLE, &provided_thread_support );
        return getRank();
    }
    void shutdown( ) {
        MPI_Barrier( MPI_COMM_WORLD );
        MPI_Finalize( );
    }
#else
    #define MPI_COMM_WORLD 0
    void MPI_Finalize() {}
    void barrier() {}
    int sumReduce( int x ) { return x; }
    int startup( int, char*[] ) { return 0; }
    void shutdown( ) { }
    int getRank() { return 0; }
#endif
// clang-format on


// Function to get the time in ms
#define to_ms( x ) std::chrono::duration_cast<std::chrono::milliseconds>( x ).count()


// NULL_USE function
#define NULL_USE( variable )                       \
    do {                                           \
        if ( 0 ) {                                 \
            auto static temp = (char *) &variable; \
            temp++;                                \
        }                                          \
    } while ( 0 )


// Class to store pass/failures
class UnitTest
{
public:
    void passes( const std::string &msg ) { passes_.push_back( msg ); }
    void failure( const std::string &msg ) { failure_.push_back( msg ); }
    void expected( const std::string &msg ) { expected_.push_back( msg ); }

    void print() const
    {
        int rank = getRank();
        if ( rank == 0 ) {
            std::cout << "\nTests passed:" << std::endl;
            for ( const auto &msg : passes_ )
                std::cout << "   " << msg << std::endl;
            std::cout << std::endl << "Tests expected failed:" << std::endl;
        }
        barrier();
        for ( const auto &msg : expected_ )
            std::cout << "   Rank " << rank << ": " << msg << std::endl;
        barrier();
        if ( rank == 0 )
            std::cout << std::endl << "Tests failed:" << std::endl;
        barrier();
        for ( const auto &msg : failure_ )
            std::cout << "   Rank " << rank << ": " << msg << std::endl;
        barrier();
    }

    int N_failed() const { return sumReduce( failure_.size() ); }

    void clear()
    {
        passes_   = std::vector<std::string>();
        failure_  = std::vector<std::string>();
        expected_ = std::vector<std::string>();
    }

private:
    std::vector<std::string> passes_;
    std::vector<std::string> failure_;
    std::vector<std::string> expected_;
};


// Function to return the call stack
std::vector<StackTrace::stack_info> get_call_stack( const std::vector<int> &null = {} )
{
    NULL_USE( null );
    auto stack = StackTrace::getCallStack();
    // Trick compiler to skip inline for this function with fake recursion
    if ( stack.size() > 10000 ) {
        stack = get_call_stack();
    }
    return stack;
}


// Sleep for the given time
// Note: since we are testing interrupts, we may not sleep for the desired time
//   so we need to perform the sleep in a loop
void sleep_ms( int N )
{
    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();
    while ( to_ms( t2 - t1 ) < N ) {
        int N2 = N - to_ms( t2 - t1 );
        std::this_thread::sleep_for( std::chrono::milliseconds( N2 ) );
        t2 = std::chrono::high_resolution_clock::now();
    }
}
void sleep_s( int N )
{
    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();
    while ( to_ms( t2 - t1 ) < 1000 * N ) {
        int N2 = 1000 * N - to_ms( t2 - t1 );
        std::this_thread::sleep_for( std::chrono::milliseconds( N2 ) );
        t2 = std::chrono::high_resolution_clock::now();
    }
}


// Function to perform various signal tests
int global_signal_helper[1024] = { 0 };
void handleSignal( int s ) { global_signal_helper[s] = 1; }
void testSignal( UnitTest &results )
{
    barrier();
    int rank = getRank();
    if ( rank == 0 ) {
        // Get a list of signals that can be caught
        auto signals = StackTrace::allSignalsToCatch();
        // Identify the signals
        std::cout << "\nIdentifying signals\n";
        // for ( int i = 1; i <= std::max( 1, signals.back() ); i++ )
        for ( int i = 1; i <= std::max( 1, signals.back() ); i++ )
            std::cout << "  " << i << ": " << StackTrace::signalName( i ) << std::endl;
        // Test setting/catching different signals
        StackTrace::setSignals( signals, handleSignal );
        for ( auto sig : signals )
            StackTrace::raiseSignal( sig );
        sleep_ms( 50 );
        StackTrace::clearSignals( signals );
        bool pass = true;
        for ( auto sig : signals )
            pass = pass && global_signal_helper[sig] == 1;
        std::cout << std::endl;
        if ( pass )
            results.passes( "Signals" );
        else
            results.failure( "Signals" );
    }
    barrier();
}


// Test current stack trace
void testCurrentStack( UnitTest &results, bool &decoded_symbols )
{
    barrier();
    const int rank  = getRank();
    double ts1      = time();
    auto call_stack = get_call_stack();
    double ts2      = time();
    if ( rank == 0 ) {
        std::cout << "Call stack:" << std::endl;
        StackTrace::stack_info::print( std::cout, call_stack, "   " );
        std::cout << "Time to get call stack: " << ts2 - ts1 << std::endl;
    }
    decoded_symbols = false;
    if ( !call_stack.empty() ) {
        results.passes( "non empty call stack" );
        for ( auto &item : call_stack ) {
            if ( strstr( item.function.data(), "get_call_stack" ) )
                decoded_symbols = true;
        }
        if ( decoded_symbols )
            results.passes( "call stack decoded function symbols" );
        else
            results.failure( "call stack was unable to decode function symbols" );
    } else {
        results.failure( "non empty call stack" );
    }
    if ( rank == 0 ) {
        ts1        = time();
        auto trace = StackTrace::backtrace();
        ts2        = time();
        std::cout << "Time to get backtrace: " << ts2 - ts1 << std::endl << std::endl;
    }
}


// Test stack trace of another thread
void testThreadStack( UnitTest &results, bool decoded_symbols )
{
    barrier();
    const int rank = getRank();
    double t1      = time();
    std::thread thread( sleep_ms, 1000 );
    sleep_ms( 50 ); // Give thread time to start
    double t2       = time();
    auto call_stack = StackTrace::getCallStack( thread.native_handle() );
    double t3       = time();
    thread.join();
    double t4 = time();
    if ( rank == 0 ) {
        std::cout << "Call stack (thread):" << std::endl;
        StackTrace::stack_info::print( std::cout, call_stack, "   " );
        std::cout << "Time to get call stack (thread): " << t3 - t2 << std::endl;
        std::cout << std::endl;
    }
    if ( !call_stack.empty() ) {
        results.passes( "non empty call stack (thread)" );
        bool pass = false;
        for ( auto &item : call_stack ) {
            if ( strstr( item.function.data(), "sleep_ms" ) )
                pass = true;
        }
        pass = pass && std::abs( t4 - t1 ) > 0.9;
        if ( pass )
            results.passes( "call stack (thread)" );
        else if ( !decoded_symbols )
            std::cout << "call stack (thread) failed to decode symbols";
        else
            results.failure( "call stack (thread)" );
    } else {
        results.failure( "non empty call stack (thread)" );
    }
}


// Test stack trace of another thread
void testFullStack( UnitTest & )
{
    barrier();
    const int rank = getRank();
    std::thread thread1( sleep_ms, 2000 );
    std::thread thread2( sleep_ms, 2000 );
    std::thread thread3( sleep_s, 2 );
    sleep_ms( 50 ); // Give thread time to start
    double t1       = time();
    auto call_stack = StackTrace::getAllCallStacks();
    cleanupStackTrace( call_stack );
    double t2 = time();
    thread1.join();
    thread2.join();
    thread3.join();
    if ( rank == 0 ) {
        std::cout << "Call stack (all threads):" << std::endl;
        call_stack.print( std::cout );
        std::cout << "Time to get call stack (all threads): " << t2 - t1 << std::endl;
        std::cout << std::endl;
    }
}


// Test stack trace of another thread
void testGlobalStack(
    UnitTest &, bool all, const std::basic_string<wchar_t> & = std::basic_string<wchar_t>() )
{
    barrier();
    const int rank = getRank();
    std::thread thread1( sleep_ms, 2000 );
    std::thread thread2( sleep_ms, 2000 );
    std::thread thread3( sleep_s, 2 );
    sleep_ms( 50 ); // Give threads time to start
    double t1 = time();
    StackTrace::multi_stack_info call_stack;
    if ( rank == 0 || all )
        call_stack = StackTrace::getGlobalCallStacks();
    cleanupStackTrace( call_stack );
    double t2 = time();
    thread1.join();
    thread2.join();
    thread3.join();
    barrier();
    if ( !all && rank != 0 )
        return;
    if ( rank == 0 && !all ) {
        std::cout << "Call stack (global):" << std::endl;
        call_stack.print( std::cout );
        std::cout << "Time to get call stack (global): " << t2 - t1 << std::endl;
        std::cout << std::endl;
    }
}


void testActivethreads( UnitTest &results )
{
    // Test getting a list of all active threads
    std::thread thread1( sleep_ms, 1000 );
    std::thread thread2( sleep_ms, 1000 );
    sleep_ms( 100 ); // Give threads time to start
    auto thread_ids_test = StackTrace::activeThreads();
    std::thread::native_handle_type thread_ids[3];
    auto self     = StackTrace::thisThread();
    thread_ids[0] = StackTrace::thisThread();
    thread_ids[1] = thread1.native_handle();
    thread_ids[2] = thread2.native_handle();
    thread1.join();
    thread2.join();
    bool pass = true;
    for ( auto id : thread_ids ) {
        auto it = std::find( thread_ids_test.begin(), thread_ids_test.end(), id );
        pass    = pass && it != thread_ids_test.end();
    }
    if ( pass )
        results.passes( "StackTrace::activeThreads" );
    else if ( thread_ids_test.size() == 1u && thread_ids_test[0] == self )
        results.expected( "StackTrace::activeThreads only is able to return self" );
    else
        results.failure( "StackTrace::activeThreads" );
}


void testStackFile( UnitTest &results, const std::string &filename )
{
    if ( getRank() != 0 ) {
        barrier();
        return;
    }
    // Read entire file
    std::cout << "Reading stack trace file: " << filename << std::endl;
    auto fid = fopen( filename.c_str(), "rb" );
    if ( fid == nullptr ) {
        results.failure( "Unable to open file " + filename );
        return;
    }
    fseek( fid, 0, SEEK_END );
    auto size = ftell( fid );
    fseek( fid, 0, SEEK_SET );
    auto buf  = new char[size + 1];
    size      = fread( buf, 1, size, fid );
    buf[size] = 0;
    fclose( fid );
    std::string str( buf );
    delete[] buf;
    // Load the stack
    auto stack = StackTrace::generateFromString( str );
    // Clean the stack trace
    cleanupStackTrace( stack );
    // Print the results
    stack.print( std::cout );
    std::cout << std::endl;
    barrier();
}


// Test calling exec in parallel
void test_exec( UnitTest &results )
{
    bool pass = true;
    int count = 0;
    auto fun  = [&pass, &count]( int N ) {
        auto cmd        = "echo test";
        int exit_code   = 0;
        bool pass_local = true;
        for ( int i = 0; i < N; i++ ) {
            auto out   = StackTrace::exec( cmd, exit_code );
            pass_local = pass_local && out == "test\n";
        }
        pass = pass && pass_local;
        count += N;
    };
    std::vector<std::thread> threads( 8 );
    for ( size_t i = 0; i < threads.size(); i++ )
        threads[i] = std::thread( fun, 1000 );
    for ( size_t i = 0; i < threads.size(); i++ )
        threads[i].join();
    pass = pass && count == 8000;
    if ( pass )
        results.passes( "exec called in parallel" );
    else
        results.failure( "exec called in parallel" );
}


// Test throw costs
void test_throw( UnitTest & )
{
    // Verify we can still get the global call stack
    barrier();
    std::thread thread1( sleep_ms, 1000 );
    std::thread thread2( sleep_s, 1 );
    if ( getRank() == 0 ) {
        std::cout << "Testing abort:" << std::endl;
        try {
            StackTrace::Utilities::abort( "Test", __FILE__, __LINE__ );
        } catch ( StackTrace::abort_error &e ) {
            std::cout << e.what() << std::endl;
        }
    }
    thread1.join();
    thread2.join();
    barrier();

    // Test the time to call abort vs throw
    if ( getRank() == 0 ) {
        int N   = 10;
        auto t1 = std::chrono::high_resolution_clock::now();
        for ( int i = 0; i < N; i++ ) {
            try {
                throw std::logic_error( "Test" );
            } catch ( std::exception &e ) {
            }
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        for ( int i = 0; i < N; i++ ) {
            try {
                StackTrace::Utilities::abort( "Test", __FILE__, __LINE__ );
            } catch ( std::exception &e ) {
            }
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        int dt1 = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count() / N;
        int dt2 = std::chrono::duration_cast<std::chrono::microseconds>( t3 - t2 ).count() / N;
        std::cout << "Cost for std::logic_error: " << dt1 << "us" << std::endl;
        std::cout << "Cost to call abort(): " << dt2 << "us" << std::endl;
        std::cout << std::endl;
    }
}


// Test terminate
void testTerminate( UnitTest &results )
{
    int exit;
    bool pass = true;
    auto msg1 = StackTrace::Utilities::exec( "./TestTerminate signal 2>&1", exit );
    auto msg2 = StackTrace::Utilities::exec( "./TestTerminate abort 2>&1", exit );
    auto msg3 = StackTrace::Utilities::exec( "./TestTerminate throw 2>&1", exit );
    pass      = pass && msg1.find( "Unhandled signal (6) caught" ) != std::string::npos;
    pass      = pass && msg2.find( "Program abort called in file" ) != std::string::npos;
    pass      = pass && msg3.find( "Unhandled exception caught" ) != std::string::npos;
    if ( pass )
        results.passes( "terminate" );
    else
        results.failure( "terminate" );
}


// Test getting type name
void testTypeName( UnitTest &results )
{
    using StackTrace::Utilities::getTypeName;
    bool pass = true;
    pass      = pass && getTypeName<int>() == "int";
    pass      = pass && getTypeName<float>() == "float";
    pass      = pass && getTypeName<double>() == "double";
    pass      = pass && getTypeName<std::complex<double>>() == "std::complex<double>";
    if ( pass )
        results.passes( "getTypeName" );
    else
        results.failure( "getTypeName" );
}


// The main function
int main( int argc, char *argv[] )
{
    int rank = startup( argc, argv );
    StackTrace::Utilities::setAbortBehavior( true, 3 );
    StackTrace::Utilities::setErrorHandlers();
    StackTrace::globalCallStackInitialize( MPI_COMM_WORLD );
    UnitTest results;

    // Limit the scope of variables
    {

        // Test exec
        test_exec( results );

        // Test getting type name
        testTypeName( results );

        // Test getting a list of all active threads
        testActivethreads( results );

        // Test getting the current call stack
        bool decoded_symbols = false;
        testCurrentStack( results, decoded_symbols );

        // Test getting the stacktrace of another thread
        testThreadStack( results, decoded_symbols );

        // Test getting the full stacktrace of all thread
        testFullStack( results );

        // Test getting the global stack trace of all threads/processes
        testGlobalStack( results, false );
        testGlobalStack( results, true );

        // Test getting the symbols
        auto symbols = StackTrace::getSymbols();
        if ( !symbols.empty() )
            results.passes( "Read symbols from executable" );

        // Test getting the executable
        auto exe = StackTrace::getExecutable();
        if ( rank == 0 )
            std::cout << "\nExecutable: " << exe << std::endl;
        if ( exe.find( "TestStack" ) != std::string::npos )
            results.passes( "getExecutable" );
        else
            results.failure( "getExecutable" );

        // Test identifying signals
        testSignal( results );

        // Test catching an error
        try {
            throw std::logic_error( "Test" );
            results.failure( "Failed to catch ERROR" );
        } catch ( ... ) {
            results.passes( "Caught ERROR" );
        }
        try {
            throw std::logic_error( "test" );
            results.failure( "Failed to catch exception" );
        } catch ( ... ) {
            results.passes( "Caught exception" );
        }

        // Test generating call stack from a string
        testStackFile( results, "ExampleStack.txt" );

        // Test the cost to throw using abort
        test_throw( results );

        // Test tick
        double tick = StackTrace::Utilities::tick();
        if ( tick > 0 && tick < 1e-5 )
            results.passes( "tick" );
        else
            results.failure( "tick" );

        // Test terminate
        testTerminate( results );
    }

    // Print the test results
    int N_errors = results.N_failed();
    results.print();
    results.clear();
    if ( N_errors == 0 && rank == 0 )
        std::cout << "\nAll tests passed\n";

    // Shutdown
    StackTrace::globalCallStackFinalize();
    StackTrace::Utilities::clearErrorHandlers();
    StackTrace::clearSignals();
    StackTrace::clearSymbols();
    shutdown();
#ifdef USE_TIMER
    PROFILE_DISABLE();
    std::cout << std::endl << std::endl;
    if ( rank == 0 )
        MemoryApp::print( std::cout );
#endif
    return N_errors;
}
