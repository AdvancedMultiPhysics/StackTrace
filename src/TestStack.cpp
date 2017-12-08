#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>


#include "StackTrace/StackTrace.h"
#include "StackTrace/Utilities.h"


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
#else
    #define MPI_COMM_WORLD 0
    void MPI_Finalize() {}
    void barrier() {}
    int sumReduce( int x ) { return x; }
    int startup( int, char*[] ) { return 0; }
    int getRank() { return 0; }
#endif
// clang-format on


// Function to get the time in ms
#define to_ms( x ) std::chrono::duration_cast<std::chrono::milliseconds>( x ).count()


// Function to return the call stack
std::vector<StackTrace::stack_info> get_call_stack()
{
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
void testSignal( std::vector<std::string> &passes, std::vector<std::string> &failure )
{
    int rank = getRank();
    if ( rank == 0 ) {
        // Identify the signals
        std::cout << "\nIdentifying signals\n";
        for ( int i = 1; i <= 64; i++ )
            std::cout << "  " << i << ": " << StackTrace::signalName( i ) << std::endl;
        // Test setting/catching different signals
        bool pass    = true;
        auto signals = StackTrace::allSignalsToCatch();
        StackTrace::setSignals( signals, handleSignal );
        for ( auto sig : signals ) {
            raise( sig );
            sleep_ms( 10 );
            signal( sig, SIG_DFL );
            if ( global_signal_helper[sig] != 1 )
                pass = false;
        }
        std::cout << std::endl;
        if ( pass )
            passes.push_back( "Signals" );
        else
            failure.push_back( "Signals" );
    }
}


// Test current stack trace
void testCurrentStack( std::vector<std::string> &passes,
                       std::vector<std::string> &failure,
                       bool &decoded_symbols )
{
    barrier();
    const int rank  = getRank();
    double ts1      = time();
    auto call_stack = get_call_stack();
    double ts2      = time();
    if ( rank == 0 ) {
        std::cout << "Call stack:" << std::endl;
        for ( auto &elem : call_stack )
            std::cout << "   " << elem.print() << std::endl;
        std::cout << "Time to get call stack: " << ts2 - ts1 << std::endl;
    }
    decoded_symbols = false;
    if ( !call_stack.empty() ) {
        passes.push_back( "non empty call stack" );
        for ( auto &i : call_stack ) {
            if ( i.print().find( "get_call_stack" ) != std::string::npos )
                decoded_symbols = true;
        }
        if ( decoded_symbols )
            passes.push_back( "call stack decoded function symbols" );
        else
            failure.push_back( "call stack was unable to decode function symbols" );
    } else {
        failure.push_back( "non empty call stack" );
    }
    if ( rank == 0 ) {
        ts1        = time();
        auto trace = StackTrace::backtrace();
        ts2        = time();
        std::cout << "Time to get backtrace: " << ts2 - ts1 << std::endl << std::endl;
    }
}


// Test stack trace of another thread
void testThreadStack( std::vector<std::string> &passes,
                      std::vector<std::string> &failure,
                      bool decoded_symbols )
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
        for ( auto &elem : call_stack )
            std::cout << "   " << elem.print() << std::endl;
        std::cout << "Time to get call stack (thread): " << t3 - t2 << std::endl;
        std::cout << std::endl;
    }
    if ( !call_stack.empty() ) {
        passes.push_back( "non empty call stack (thread)" );
        bool pass = false;
        for ( auto &elem : call_stack ) {
            if ( elem.print().find( "sleep_ms" ) != std::string::npos )
                pass = true;
        }
        pass = pass && fabs( t4 - t1 ) > 0.9;
        if ( pass )
            passes.push_back( "call stack (thread)" );
        else if ( !decoded_symbols )
            std::cout << "call stack (thread) failed to decode symbols";
        else
            failure.push_back( "call stack (thread)" );
    } else {
        failure.push_back( "non empty call stack (thread)" );
    }
}


// Test stack trace of another thread
void testFullStack( std::vector<std::string> &, std::vector<std::string> & )
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
        auto text = call_stack.print( "   " );
        for ( auto &line : text )
            std::cout << line << std::endl;
        std::cout << "Time to get call stack (all threads): " << t2 - t1 << std::endl;
        std::cout << std::endl;
    }
}


// Test stack trace of another thread
void testGlobalStack( std::vector<std::string> &,
                      std::vector<std::string> &,
                      bool all,
                      const std::basic_string<wchar_t> & = std::basic_string<wchar_t>() )
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
        auto text = call_stack.print( "   " );
        for ( auto &line : text )
            std::cout << line << std::endl;
        std::cout << "Time to get call stack (global): " << t2 - t1 << std::endl;
        std::cout << std::endl;
    }
}


// The main function
int main( int argc, char *argv[] )
{
    int rank = startup( argc, argv );
    StackTrace::Utilities::setAbortBehavior( true, true, true );
    StackTrace::globalCallStackInitialize( MPI_COMM_WORLD );
    std::vector<std::string> passes, failure;

    // Limit the scope of variables
    {
        // Test getting the current call stack
        bool decoded_symbols = false;
        testCurrentStack( passes, failure, decoded_symbols );

        // Test getting the stacktrace of another thread
        testThreadStack( passes, failure, decoded_symbols );

        // Test getting the full stacktrace of all thread
        testFullStack( passes, failure );

        // Test getting the global stack trace of all threads/processes
        testGlobalStack( passes, failure, false );
        testGlobalStack( passes, failure, true );

        // Test getting a list of all active threads
        std::thread thread1( sleep_ms, 1000 );
        std::thread thread2( sleep_ms, 1000 );
        sleep_ms( 50 ); // Give threads time to start
        auto thread_ids_test = StackTrace::activeThreads();
        std::set<std::thread::native_handle_type> thread_ids;
        thread_ids.insert( StackTrace::thisThread() );
        thread_ids.insert( thread1.native_handle() );
        thread_ids.insert( thread2.native_handle() );
        thread1.join();
        thread2.join();
        if ( thread_ids == thread_ids_test )
            passes.push_back( "StackTrace::activeThreads" );
        else
            failure.push_back( "StackTrace::activeThreads" );

        // Test getting the symbols
        std::vector<void *> address;
        std::vector<char> type;
        std::vector<std::string> obj;
        int rtn = StackTrace::getSymbols( address, type, obj );
        if ( rtn == 0 && !address.empty() )
            passes.push_back( "Read symbols from executable" );

        // Test getting the executable
        auto exe = StackTrace::getExecutable();
        if ( rank == 0 )
            std::cout << "\nExecutable: " << exe << std::endl;
        if ( exe.find( "TestStack" ) != std::string::npos )
            passes.push_back( "getExecutable" );
        else
            failure.push_back( "getExecutable" );

        // Test identifying signals
        testSignal( passes, failure );

        // Test catching an error
        try {
            throw std::logic_error( "Test" );
            failure.push_back( "Failed to catch ERROR" );
        } catch ( ... ) {
            passes.push_back( "Caught ERROR" );
        }
        try {
            throw std::logic_error( "test" );
            failure.push_back( "Failed to catch exception" );
        } catch ( ... ) {
            passes.push_back( "Caught exception" );
        }
    }

    // Print the test results
    StackTrace::globalCallStackFinalize();
    int N_errors = sumReduce( failure.size() );
    if ( rank == 0 ) {
        std::cout << "Tests passed:" << std::endl;
        for ( const auto &msg : passes )
            std::cout << "   " << msg << std::endl;
        std::cout << std::endl << "Tests failed:" << std::endl;
    }
    barrier();
    for ( const auto &msg : failure )
        std::cout << "   Rank " << rank << ": " << msg << std::endl;
    barrier();
    if ( N_errors == 0 && rank == 0 )
        std::cout << "\nAll tests passed\n";
    barrier();
    MPI_Finalize();
    return N_errors;
}
