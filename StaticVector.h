#ifndef included_StackTrace_StaticVector
#define included_StackTrace_StaticVector

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>

#include "StackTrace/StackTrace.h"


namespace StackTrace::Utilities {


/****************************************************************************
 *  Class to replace a std::vector with a fixed capacity                     *
 ****************************************************************************/
template<class TYPE, std::size_t CAPACITY>
class staticVector final
{
public:
    staticVector() : d_size( 0 ), d_data{ TYPE() } {}
    size_t size() const { return d_size; }
    bool empty() const { return d_size == 0; }
    void push_back( const TYPE &v )
    {
        if ( d_size < CAPACITY )
            d_data[d_size++] = v;
    }
    TYPE &operator[]( size_t i ) { return d_data[i]; }
    TYPE *begin() { return d_data; }
    TYPE *end() { return d_data + d_size; }
    TYPE &back() { return d_data[d_size - 1]; }
    TYPE *data() { return d_size == 0 ? nullptr : d_data; }
    void pop_back() { d_size = std::max<size_t>( d_size, 1 ) - 1; }
    const TYPE *begin() const { return d_data; }
    const TYPE *end() const { return d_data + d_size; }
    const TYPE &back() const { return d_data[d_size - 1]; }
    void clear() { d_size = 0; }
    void resize( size_t N, TYPE x = TYPE() )
    {
        if ( N > CAPACITY )
            throw std::logic_error( "Invalid size" );
        for ( size_t i = d_size; i < N; i++ )
            d_data[i] = x;
        d_size = N;
    }
    void erase( const TYPE &x )
    {
        size_t N = 0;
        for ( size_t i = 0; i < d_size; i++ ) {
            if ( d_data[i] != x )
                d_data[N++] = d_data[i];
        }
        d_size = N;
    }
    void insert( const TYPE &x )
    {
        if ( std::find( begin(), end(), x ) == end() ) {
            push_back( x );
            std::sort( begin(), end() );
        }
    }
    size_t find( const TYPE &x )
    {
        return std::distance( begin(), std::find( begin(), end(), x ) );
    }

private:
    size_t d_size;
    TYPE d_data[CAPACITY];
};


} // namespace StackTrace::Utilities

#endif
