#include "UnitTest.h"
#include "Utilities.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>


#define printp printf
#define pout std::cout


/********************************************************************
*  Constructor/Destructor                                           *
********************************************************************/
UnitTest::UnitTest()
{
#ifdef USE_MPI
    comm = MPI_COMM_WORLD;
#endif
}
UnitTest::~UnitTest() { reset(); }
void UnitTest::reset()
{
    pass_messages.clear();
    fail_messages.clear();
    expected_fail_messages.clear();
}


/********************************************************************
*  Print a global report                                            *
*  Note: only rank 0 will print, all messages will be aggregated    *
********************************************************************/
static inline void print_messages( const std::vector<std::vector<std::string>> &messages )
{
    if ( messages.size() > 1 ) {
        for ( size_t i = 0; i < messages.size(); i++ ) {
            if ( messages[i].size() > 0 ) {
                printp( "     Proccessor %i:\n", static_cast<int>( i ) );
                for ( size_t j = 0; j < messages[i].size(); j++ )
                    pout << "        " << messages[i][j] << std::endl;
            }
        }
    } else {
        for ( size_t j = 0; j < messages[0].size(); j++ )
            pout << "    " << messages[0][j] << std::endl;
    }
}
void UnitTest::report( const int level0 ) const
{
    int size = getSize();
    int rank = getRank();
    // Broadcast the print level from rank 0
    int level = level0;
#ifdef USE_MPI
    if ( getSize() > 1 )
        MPI_Bcast( &level, 1, MPI_INT, 0, comm );
#endif
    if ( level < 0 || level > 2 )
        ERROR( "Invalid print level" );
    // Perform a global all gather to get the number of failures per processor
    std::vector<int> N_pass( size, 0 );
    std::vector<int> N_fail( size, 0 );
    std::vector<int> N_expected_fail( size, 0 );
    int local_pass_size          = static_cast<int>(pass_messages.size());
    int local_fail_size          = static_cast<int>(fail_messages.size());
    int local_expected_fail_size = static_cast<int>(expected_fail_messages.size());
    if ( getSize() > 1 ) {
#ifdef USE_MPI
        MPI_Allgather( &local_pass_size, 1, MPI_INT, &N_pass[0], 1, MPI_INT, comm );
        MPI_Allgather( &local_fail_size, 1, MPI_INT, &N_fail[0], 1, MPI_INT, comm );
        MPI_Allgather(
            &local_expected_fail_size, 1, MPI_INT, &N_expected_fail[0], 1, MPI_INT, comm );
#endif
    } else {
        N_pass[0]          = local_pass_size;
        N_fail[0]          = local_fail_size;
        N_expected_fail[0] = local_expected_fail_size;
    }
    int N_pass_tot          = 0;
    int N_fail_tot          = 0;
    int N_expected_fail_tot = 0;
    for ( int i = 0; i < size; i++ ) {
        N_pass_tot += N_pass[i];
        N_fail_tot += N_fail[i];
        N_expected_fail_tot += N_expected_fail[i];
    }
    // Send all messages to rank 0 (if needed)
    std::vector<std::vector<std::string>> pass_messages_rank( size );
    std::vector<std::vector<std::string>> fail_messages_rank( size );
    std::vector<std::vector<std::string>> expected_fail_rank( size );
    // Get the pass messages
    if ( ( level == 1 && N_pass_tot <= 20 ) || level == 2 ) {
        if ( rank == 0 ) {
            // Rank 0 should receive all messages
            for ( int i = 0; i < size; i++ ) {
                if ( i == 0 )
                    pass_messages_rank[i] = pass_messages;
                else if ( N_pass[i] > 0 )
                    pass_messages_rank[i] = unpack_message_stream( i, 1 );
            }
        } else if ( !pass_messages.empty() ) {
            // All other ranks send their message (use non-blocking communication)
            pack_message_stream( pass_messages, 0, 1 );
        }
    }
    // Get the fail messages
    if ( level == 1 || level == 2 ) {
        if ( rank == 0 ) {
            // Rank 0 should receive all messages
            for ( int i = 0; i < size; i++ ) {
                if ( i == 0 )
                    fail_messages_rank[i] = fail_messages;
                else if ( N_fail[i] > 0 )
                    fail_messages_rank[i] = unpack_message_stream( i, 2 );
            }
        } else if ( !fail_messages.empty() ) {
            // All other ranks send their message (use non-blocking communication)
            pack_message_stream( fail_messages, 0, 2 );
        }
    }
    // Get the expected_fail messages
    if ( ( level == 1 && N_expected_fail_tot <= 50 ) || level == 2 ) {
        if ( rank == 0 ) {
            // Rank 0 should receive all messages
            for ( int i = 0; i < size; i++ ) {
                if ( i == 0 )
                    expected_fail_rank[i] = expected_fail_messages;
                else if ( N_expected_fail[i] > 0 )
                    expected_fail_rank[i] = unpack_message_stream( i, 3 );
            }
        } else if ( !expected_fail_messages.empty() ) {
            // All other ranks send their message (use non-blocking communication)
            pack_message_stream( expected_fail_messages, 0, 3 );
        }
    }
    // Print the results of all messages (only rank 0 will print)
    if ( rank == 0 ) {
        pout << std::endl;
        // Print the passed tests
        pout << "Tests passed" << std::endl;
        if ( level == 0 || ( level == 1 && N_pass_tot > 20 ) ) {
            // We want to print a summary
            if ( size > 8 ) {
                // Print 1 summary for all processors
                printp( "     %i tests passed (use report level 2 for more detail)\n", N_pass_tot );
            } else {
                // Print a summary for each processor
                for ( int i = 0; i < size; i++ )
                    printp( "     %i tests passed (proc %i) (use report level 2 for more detail)\n",
                        N_pass[i], i );
            }
        } else {
            // We want to print all messages
            for ( int i = 0; i < size; i++ )
                ASSERT( (int) pass_messages_rank[i].size() == N_pass[i] );
            print_messages( pass_messages_rank );
        }
        pout << std::endl;
        // Print the tests that failed
        pout << "Tests failed" << std::endl;
        if ( level == 0 ) {
            // We want to print a summary
            if ( size > 8 ) {
                // Print 1 summary for all processors
                printp( "     %i tests failed (use report level 2 for more detail)\n", N_fail_tot );
            } else {
                // Print a summary for each processor
                for ( int i = 0; i < size; i++ )
                    printp( "     %i tests failed (proc %i) (use report level 2 for more detail)\n",
                        N_fail[i], i );
            }
        } else {
            // We want to print all messages
            for ( int i = 0; i < size; i++ )
                ASSERT( (int) fail_messages_rank[i].size() == N_fail[i] );
            print_messages( fail_messages_rank );
        }
        pout << std::endl;
        // Print the tests that expected failed
        pout << "Tests expected failed" << std::endl;
        if ( level == 0 || ( level == 1 && N_expected_fail_tot > 50 ) ) {
            // We want to print a summary
            if ( size > 8 ) {
                // Print 1 summary for all processors
                printp( "     %i tests expected failed (use report level 2 for more detail)\n",
                    N_expected_fail_tot );
            } else {
                // Print a summary for each processor
                for ( int i = 0; i < size; i++ )
                    printp( "     %i tests expected failed (proc %i) (use report level 2 for more "
                            "detail)\n",
                        N_expected_fail[i], i );
            }
        } else {
            // We want to print all messages
            for ( int i = 0; i < size; i++ )
                ASSERT( (int) expected_fail_rank[i].size() == N_expected_fail[i] );
            print_messages( expected_fail_rank );
        }
        pout << std::endl;
    }
// Add a barrier to synchronize all processors (rank 0 is much slower)
#ifdef USE_MPI
    if ( getSize() > 1 )
        MPI_Barrier( comm );
#endif
    Utilities::sleep_ms( 10 ); // Need a brief pause to allow any printing to finish
}


/********************************************************************
*  Pack and send the given messages                                 *
********************************************************************/
void UnitTest::pack_message_stream(
    const std::vector<std::string> &messages, const int rank, const int tag ) const
{
#ifdef USE_MPI
    // Get the size of the messages
    int N_messages   = (int) messages.size();
    int *msg_size    = new int[N_messages];
    int msg_size_tot = 0;
    for ( int i = 0; i < N_messages; i++ ) {
        msg_size[i] = (int) messages[i].size();
        msg_size_tot += msg_size[i];
    }
    // Allocate space for the message stream
    int size_data = ( N_messages + 1 ) * sizeof( int ) + msg_size_tot;
    char *data    = new char[size_data];
    // Pack the message stream
    memcpy(data,&N_messages,sizeof(int));
    memcpy(&data[sizeof(int)],msg_size,N_messages*sizeof(int));
    int k = ( N_messages + 1 ) * sizeof( int );
    for ( int i = 0; i < N_messages; i++ ) {
        messages[i].copy( &data[k], msg_size[i] );
        k += msg_size[i];
    }
    // Send the message stream (using a non-blocking send)
    MPI_Request request;
    MPI_Isend( data, size_data, MPI_CHAR, rank, tag, comm, &request );
    // Wait for the communication to send and free the temporary memory
    MPI_Status status;
    MPI_Wait( &request, &status );
    delete[] data;
    delete[] msg_size;
#endif
}


/********************************************************************
*  Receive and unpack a message stream                              *
********************************************************************/
std::vector<std::string> UnitTest::unpack_message_stream( const int rank, const int tag ) const
{
#ifdef USE_MPI
    // Probe the message to get the message size
    MPI_Status status;
    MPI_Probe( rank, tag, comm, &status );
    int size_data = -1;
    MPI_Get_count( &status, MPI_BYTE, &size_data );
    ASSERT( size_data >= 0 );
    // Allocate memory to receive the data
    char *data = new char[size_data];
    // receive the data (using a non-blocking receive)
    MPI_Request request;
    MPI_Irecv( data, size_data, MPI_CHAR, rank, tag, comm, &request );
    // Wait for the communication to be received
    MPI_Wait( &request, &status );
    // Unpack the message stream
    int N_messages = 0;
    memcpy(&N_messages,data,sizeof(int));
    std::vector<int> msg_size( N_messages );
    std::vector<std::string> messages( N_messages );
    memcpy(msg_size.data(),data,N_messages*sizeof(int));
    int k = ( N_messages + 1 ) * sizeof( int );
    for ( int i = 0; i < N_messages; i++ ) {
        messages[i] = std::string( &data[k], msg_size[i] );
        k += msg_size[i];
    }
    // Delete the temporary memory
    delete[] data;
    return messages;
#else
    return std::vector<std::string>();
#endif
}


/********************************************************************
*  Other functions                                                  *
********************************************************************/
int UnitTest::getRank() const
{
    int rank = 0;
#ifdef USE_MPI
    int flag = 0;
    MPI_Initialized( &flag );
    if ( flag )
        MPI_Comm_rank( comm, &rank );
#endif
    return rank;
}
int UnitTest::getSize() const
{
    int size = 1;
#ifdef USE_MPI
    int flag = 0;
    MPI_Initialized( &flag );
    if ( flag )
        MPI_Comm_size( comm, &size );
#endif
    return size;
}
size_t UnitTest::NumPassGlobal() const
{
    size_t num = pass_messages.size();
#ifdef USE_MPI
    if ( getSize() > 1 ) {
        int send = static_cast<int>( num );
        int sum  = 0;
        MPI_Allreduce( &send, &sum, 1, MPI_INT, MPI_SUM, comm );
        num = static_cast<size_t>( sum );
    }
#endif
    return num;
}
size_t UnitTest::NumFailGlobal() const
{
    size_t num = fail_messages.size();
#ifdef USE_MPI
    if ( getSize() > 1 ) {
        int send = static_cast<int>( num );
        int sum  = 0;
        MPI_Allreduce( &send, &sum, 1, MPI_INT, MPI_SUM, comm );
        num = static_cast<size_t>( sum );
    }
#endif
    return num;
}
size_t UnitTest::NumExpectedFailGlobal() const
{
    size_t num = expected_fail_messages.size();
#ifdef USE_MPI
    if ( getSize() > 1 ) {
        int send = static_cast<int>( num );
        int sum  = 0;
        MPI_Allreduce( &send, &sum, 1, MPI_INT, MPI_SUM, comm );
        num = static_cast<size_t>( sum );
    }
#endif
    return num;
}
