/*
 * Copyright 2018 Naomasa Matsubayashi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>
#include <vector>
#include <cstddef>
#include <string>
#include <numeric>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <boost/spirit/include/qi.hpp>
#include <boost/scope_exit.hpp>
#include <boost/lockfree/queue.hpp>
#include <tar_benchmark/tar_benchmark.hpp>

namespace tar_benchmark {
  namespace {
    boost::spirit::qi::uint_parser< uint64_t, 8 > uint_p;
  }
  uint64_t parse_uint( const uint8_t *begin, const uint8_t *end_ ) {
    namespace qi = boost::spirit::qi;
    auto iter = begin;
    const auto end = std::find( begin, end_, '\0' );
    uint64_t parsed;
    if( qi::parse( iter, end, uint_p, parsed ) && iter == end )
      return parsed;
    else throw unable_to_parse_uint();
  }
  header read_header( size_t offset, const uint8_t *serialized, const header *prev, const uint8_t *head ) {
    try {
      const uint32_t sum =
      std::accumulate( serialized, serialized + 148, uint32_t( 0 ), []( auto v, auto sum ) { return v + sum; } ) +
      uint32_t( ' ' ) * 8 +
      std::accumulate( serialized + 156, serialized + 512, uint32_t( 0 ), []( auto v, auto sum ) { return v + sum; } );
      if( serialized[ 148 ] != '\0' ) {
        const uint32_t expected_sum = parse_uint( serialized + 148, serialized + 156 );
        if( sum != expected_sum ) throw unable_to_read_file();
      }
      filetype_t type;
      if( serialized[ 156 ] == 'g' ) type = filetype_t::regular;
      else if( serialized[ 156 ] == '\0' ) type = filetype_t::regular;
      else if( serialized[ 156 ] == 'L' ) type = filetype_t::long_name;
      else if( serialized[ 156 ] == 'K' ) type = filetype_t::long_link;
      else type = filetype_t( parse_uint( serialized + 156, serialized + 157 ) );
      uint64_t size = 0;
      if( serialized[ 124 ] == '\0' ) size = 0;
      else if( type == filetype_t::regular ) size = parse_uint( serialized + 124, serialized + 136 );
      else if( type == filetype_t::long_name ) size = parse_uint( serialized + 124, serialized + 136 );
      else if( type == filetype_t::long_link ) size = parse_uint( serialized + 124, serialized + 136 );
      if( prev && prev->type == filetype_t::long_name ) {
        const auto name_begin = std::next( head, prev->offset );
        const auto name_end = std::next( head, prev->offset + prev->size - 1 );
        return header( reinterpret_cast< const char* >( reinterpret_cast< const void* >( name_begin ) ), reinterpret_cast< const char* >( reinterpret_cast< const void* >( name_end ) ), offset + block_size, size, filetype_t( type ) );
      }
      else {
        const auto name_end = std::find( serialized, std::next( serialized, 100 ), '\0' );
        if( name_end[ -1 ] == '/' ) type = filetype_t::directory;
        return header( reinterpret_cast< const char* >( reinterpret_cast< const void* >( serialized ) ), reinterpret_cast< const char* >( reinterpret_cast< const void* >( name_end ) ), offset + block_size, size, filetype_t( type ) );
      }
    } catch ( unable_to_parse_uint ) { throw unable_to_read_file(); }
  }
  std::tuple< std::shared_ptr< uint8_t >, size_t > load_tar( const std::string &input ) {
    const int fd = open( input.c_str(), O_RDONLY );
    if( fd < 0 ) throw unable_to_read_file();
    BOOST_SCOPE_EXIT( fd ) {
      close( fd );
    } BOOST_SCOPE_EXIT_END;
    struct stat file_stat;
    if( fstat( fd, &file_stat ) < 0 ) throw unable_to_read_file();
    if( size_t( file_stat.st_size ) < block_size ) throw unable_to_read_file();
    return std::make_tuple( std::shared_ptr< uint8_t >(
      reinterpret_cast< uint8_t* >( mmap( nullptr, file_stat.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0 ) ),
      [l=file_stat.st_size]( uint8_t *p ) { munmap( reinterpret_cast< void* >( p ), l ); }
    ), file_stat.st_size );
  }
  std::vector< header > parse_tar( const std::shared_ptr< uint8_t > &mapped, size_t size ) {
    size_t offset = 0;
    std::vector< header > headers;
    header *prev = nullptr;
    while( offset < size - block_size ) {
      const auto h = read_header( offset, mapped.get() + offset, prev, mapped.get() );
      offset += ( ( h.size + ( block_size - 1 ) ) / block_size ) * block_size + block_size;
      headers.push_back( h );
      prev = &headers.back();
      if( mapped.get()[ offset ] == '\0' ) break;
    }
    return headers;
  }
  void record_elapsed_time( boost::lockfree::queue< result_t > &results, uint64_t size, uint64_t time ) {
    result_t r;
    r.ns = time;
    r.size = size;
    while ( !results.push( r ) );
  }
  void print_elapsed_time( boost::lockfree::queue< result_t > &results ) {
    while( 1 ) {
      result_t r;
      if( results.pop( r ) ) std::cout << r.size << "\t" << r.ns << std::endl;
      else break;
    }
  }
}
