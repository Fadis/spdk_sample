/*
 * Copyright 2018 Naomasa Matsubayashi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <thread>
#include <iostream>
#include <string>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <new>
#include <fcntl.h>
#include <libaio.h>
#include <boost/spirit/include/karma.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/lockfree/queue.hpp>
#include <tar_benchmark/tar_benchmark.hpp>

struct io_failure {};

void clean_output_dir( const std::string &output ) {
  boost::system::error_code error;
  boost::filesystem::remove_all( output, error );
  if( error ) throw io_failure();
}

void create_dirs( const boost::filesystem::path &dir ) {
  boost::system::error_code error;
  boost::filesystem::create_directories( dir, error );
  if( error ) throw io_failure();
}

struct file_t;

struct context_t {
  context_t( const std::string &i, const std::string &o, size_t c );
  std::string input;
  std::string output;
  size_t max_concurrency;
  size_t block_size;
  size_t total_size;
  size_t file_count;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::vector< file_t > files;
  std::shared_ptr< uint8_t > buf;
  std::vector< tar_benchmark::header > headers;
  std::atomic< size_t > active_count;
  std::atomic< size_t > loaded_count;
  std::atomic< size_t > completed_count;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::atomic< size_t > total_elapsed_time;
  std::atomic< bool > completed;
  io_context_t io_context;
};

void close_file( file_t& );

struct file_t {
  file_t( context_t *c, const std::string &name_, size_t s, size_t o ) : context( c ), name( name_ ), size( s ), offset( o ), pcb( &cb ) {
    cb.data = reinterpret_cast< void* >( this );
  }
  file_t( file_t &&src ) : context( src.context ), name( std::move( src.name ) ), begin( src.begin ), cb( src.cb ), size( src.size ), offset( src.offset ), pcb( &cb ) {
    cb.data = reinterpret_cast< void* >( this );
  }
  file_t &operator=( file_t &&src ) {
    context = src.context;
    name = std::move( src.name );
    begin = src.begin;
    cb = src.cb;
    size = src.size;
    offset = src.offset;
    pcb = &cb;
    cb.data = reinterpret_cast< void* >( this );
    return *this;
  }
  void create_file() {
    while( 1 ) {
      auto count = context->active_count.load();
      if( count < context->max_concurrency )
        if( context->active_count.compare_exchange_strong( count, count + 1u ) )
          break;
    }
    const auto path = boost::filesystem::path( name );
    auto dir = path;
    dir.remove_filename();
    create_dirs( dir );
    int fd = open( name.c_str(), O_WRONLY|O_CREAT|O_DIRECT, 0644 );
    if( fd < 0 ) throw io_failure();
    io_prep_pwrite( &cb, fd, std::next( context->buf.get(), offset ), size / context->block_size * context->block_size + ( ( size % context->block_size ) ? context->block_size : 0 ), 0 );
    cb.data = reinterpret_cast< void* >( this );
  }
  void write_file() {
    while( 1 ) {
      auto r = io_submit( context->io_context, 1, &pcb );
      if( r >= 0 ) break;
      if( r != -EAGAIN && r < 0 ) throw io_failure();
    }
  }
  void close_file() {
    close( cb.aio_fildes );
    --context->active_count;
  }
  context_t *context;
  std::string name;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  iocb cb;
  size_t size;
  size_t offset;
  iocb *pcb;
};

context_t::context_t( const std::string &i, const std::string &o, size_t c ) : input( i ), output( o ), max_concurrency( c ), block_size( 512 ), total_size( 0 ), active_count( 0 ), loaded_count( 0 ), completed_count( 0 ), total_elapsed_time( 0 ), completed( false ) {
  const auto [mapped,size] = tar_benchmark::load_tar( input );
  uint8_t *buf_;
  if( posix_memalign( (void**)&buf_, block_size, size ) < 0 ) throw std::bad_alloc();
  buf.reset( buf_ );
  memcpy( buf.get(), mapped.get(), size );
  headers = tar_benchmark::parse_tar( buf, size );
  file_count = std::count_if( headers.begin(), headers.end(), []( const auto &h ) { return h.type == tar_benchmark::filetype_t::regular && h.size != 0; } );
  total_size = std::accumulate( headers.begin(), headers.end(), size_t( 0 ), []( size_t sum, const auto &h ) { return sum + ( ( h.type == tar_benchmark::filetype_t::regular ) ? h.size : size_t( 0 ) ); } );
  for( const auto &header: headers )
    if( header.type == tar_benchmark::filetype_t::regular )
      files.emplace_back( this, output+std::string( header.name_begin, header.name_end ), header.size, header.offset );
  results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( files.size() ) );
  clean_output_dir( output );
  io_queue_init( 1, &io_context );
}

void close_file( file_t &file ) {
  auto end = std::chrono::high_resolution_clock::now();
  file.close_file();
  const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file.begin ).count();
  tar_benchmark::record_elapsed_time( *file.context->results, file.size, elapsed );
  if( ++file.context->completed_count == file.context->files.size() ) {
    auto end = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file.context->begin ).count();
    file.context->total_elapsed_time = elapsed;
    file.context->completed = true;
  }
}

void read_file( file_t &file ) {
  file.create_file();
  file.begin = std::chrono::high_resolution_clock::now();
  file.write_file();
}

void run( const std::string &input, const std::string &output, size_t concurrency ) {
  const auto [mapped,size] = tar_benchmark::load_tar( input );
  auto headers = tar_benchmark::parse_tar( mapped, size );
  clean_output_dir( output );
  context_t context( input, output, concurrency );
  context.begin = std::chrono::high_resolution_clock::now();
  std::vector< iocb > ios;
  ios.reserve( context.files.size() );
  std::transform( context.files.begin(), context.files.end(), std::back_inserter( ios ), []( const auto &v ) { return v.cb; } );
  std::thread poller( [&context]() {
    while( !context.completed.load() ) {
      io_event event;
      event.res = 0;
      auto count = io_getevents( context.io_context, 0, 1, &event, nullptr );
      if( count < 0 ) throw io_failure();
      if( count > 0 ) {
        if( int( event.res ) < 0 ) throw io_failure();
        close_file( *reinterpret_cast< file_t* >( reinterpret_cast< void* >( event.data ) ) );
      }
    }
  } );
  for( size_t i = 0u; i < context.files.size(); ++i ) {
    auto &file = context.files[ i ];
    read_file( file );
  }
  poller.join();
  tar_benchmark::print_elapsed_time( *context.results );
  std::cout << "total\t" << context.total_elapsed_time.load() << std::endl;
}

int main( int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("input,i", po::value<std::string>()->default_value( "input.tar" ), "input")
    ("output,o", po::value<std::string>()->default_value( "out/" ), "output")
    ("concurrency,x", po::value<size_t>()->default_value( 0 ), "max concurrency");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  run( opt_var[ "input" ].as< std::string >(), opt_var[ "output" ].as< std::string >(), opt_var[ "concurrency" ].as< size_t> () );
}

