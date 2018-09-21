/*
 * Copyright 2018 Naomasa Matsubayashi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef TAR_BENCHMARK_TAR_BENCHMARK_HPP
#define TAR_BENCHMARK_TAR_BENCHMARK_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <boost/lockfree/queue.hpp>

namespace tar_benchmark {
  enum class filetype_t {
    regular = 0,
    symbolic_link = 1,
    sym = 2,
    character_device = 3,
    block_device = 4,
    directory = 5,
    fifo = 6,
    cont = 7,
    long_name = 8,
    long_link = 9,
  };
  struct header {
    header(
      const char *name_begin_,
      const char *name_end_,
      size_t offset_,
      size_t size_,
      filetype_t type_
    ) : name_begin( name_begin_ ), name_end( name_end_ ), offset( offset_ ), size( size_ ), type( type_ ) {}
    const char *name_begin;
    const char *name_end;
    size_t offset;
    size_t size;
    filetype_t type;
  };
  struct unable_to_parse_uint {};
  struct unable_to_read_file {};
  struct unable_to_write_file {};
  constexpr size_t block_size = 512;
  uint64_t parse_uint( const uint8_t *begin, const uint8_t *end_ );
  header read_header( size_t offset, const uint8_t *serialized, const header*, const uint8_t* );
  struct result_t {
    uint64_t size;
    uint64_t ns;
  };
  std::tuple< std::shared_ptr< uint8_t >, size_t > load_tar( const std::string &input );
  std::vector< header > parse_tar( const std::shared_ptr< uint8_t > &mapped, size_t size );
  void record_elapsed_time( boost::lockfree::queue< result_t > &results, uint64_t size, uint64_t time );
  void print_elapsed_time( boost::lockfree::queue< result_t > &results );
}

#endif

