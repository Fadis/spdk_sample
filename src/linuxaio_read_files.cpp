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
#include <new>
#include <fcntl.h>
#include <libaio.h>
#include <boost/spirit/include/karma.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/lockfree/queue.hpp>
#include <tar_benchmark/tar_benchmark.hpp>


struct unable_to_read_file {};

struct file_t;

struct context_t {
  context_t( const std::string &i, size_t c ) : input( i ), max_concurrency( c ), page_size( 4096 ), total_size( 0 ), active_count( 0 ), loaded_count( 0 ), completed_count( 0 ), total_elapsed_time( 0 ), completed( false ) {
    io_queue_init( 1, &io_context );
  }
  std::string input;
  size_t max_concurrency;
  size_t page_size;
  size_t total_size;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::vector< file_t > files;
  std::shared_ptr< uint8_t > buf;
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
  void set_fd( int fd ) {
    io_prep_pread( &cb, fd, std::next( context->buf.get(), offset ), size, 0 );
    cb.data = reinterpret_cast< void* >( this );
  }
  context_t *context;
  std::string name;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  iocb cb;
  size_t size;
  size_t offset;
  iocb *pcb;
};

void close_file( file_t &file ) {
  auto end = std::chrono::high_resolution_clock::now();
  close( file.cb.aio_fildes );
  const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file.begin ).count();
  tar_benchmark::record_elapsed_time( *file.context->results, file.size, elapsed );
  if( ++file.context->completed_count == file.context->files.size() ) {
    auto end = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file.context->begin ).count();
    file.context->total_elapsed_time = elapsed;
    file.context->completed = true;
  }
  --file.context->active_count;
}

void read_file( file_t &file ) {
  while( 1 ) {
    auto count = file.context->active_count.load();
    if( count < file.context->max_concurrency )
      if( file.context->active_count.compare_exchange_strong( count, count + 1u ) )
        break;
  }
  const int fd = open( file.name.c_str(), O_RDONLY );
  if( fd < 0 ) throw unable_to_read_file();
  file.set_fd( fd );
  file.begin = std::chrono::high_resolution_clock::now();
  auto r = io_submit( file.context->io_context, 1, &file.pcb );
  if( r < 0 ) throw unable_to_read_file();
}

void load_files( context_t &ctx, const boost::filesystem::path &root ) {
  for( auto iter = boost::filesystem::directory_iterator( root ); iter !=  boost::filesystem::directory_iterator(); ++iter ) {
    if( boost::filesystem::is_directory( *iter ) ) load_files( ctx, *iter );
    else if( boost::filesystem::is_regular_file( *iter ) ) {
      std::string name = iter->path().string();
      size_t size = file_size( *iter );
      size_t aligned_size = ( size / ctx.page_size + ( ( size % ctx.page_size ) ? 1 : 0 ) ) * ctx.page_size;
      if( size > 0 ) {
        ctx.files.emplace_back( &ctx, std::move( name ), size, ctx.total_size );
        ctx.total_size += aligned_size;
        ++ctx.loaded_count;
      }
    }
  }
}

void run( const std::string root, size_t concurrency ) {
  context_t ctx( root, concurrency );
  load_files( ctx, boost::filesystem::path( root ) );
  uint8_t *buf;
  if( posix_memalign( (void**)&buf, ctx.page_size, ctx.total_size ) < 0 ) throw std::bad_alloc();
  ctx.buf.reset( buf );
  ctx.results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( ctx.files.size() ) );
  ctx.begin = std::chrono::high_resolution_clock::now();
  std::vector< iocb > ios;
  ios.reserve( ctx.files.size() );
  std::transform( ctx.files.begin(), ctx.files.end(), std::back_inserter( ios ), []( const auto &v ) { return v.cb; } );
  std::thread poller( [&ctx]() {
    while( !ctx.completed.load() ) {
      io_event event;
      event.res = 0;
      auto count = io_getevents( ctx.io_context, 0, 1, &event, nullptr );
      if( count < 0 ) throw unable_to_read_file();
      if( count > 0 ) {
        if( int( event.res ) < 0 ) throw unable_to_read_file();
        close_file( *reinterpret_cast< file_t* >( reinterpret_cast< void* >( event.data ) ) );
      }
    }
  } );
//#pragma omp parallel for
  for( size_t i = 0u; i < ctx.files.size(); ++i ) {
    auto &file = ctx.files[ i ];
    read_file( file );
  }
  poller.join();
  tar_benchmark::print_elapsed_time( *ctx.results );
  std::cout << "total\t" << ctx.total_elapsed_time.load() << std::endl;
}

int main( int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("input,i", po::value<std::string>()->default_value( "input.tar" ), "input")
    ("concurrency,x", po::value<size_t>()->default_value( 0 ), "max concurrency");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  run( opt_var[ "input" ].as< std::string >(), opt_var[ "concurrency" ].as< size_t>() );
}

