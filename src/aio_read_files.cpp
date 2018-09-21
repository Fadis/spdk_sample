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
#include <string>
#include <chrono>
#include <cstdio>
#include <aio.h>
#include <fcntl.h>
#include <boost/spirit/include/karma.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/lockfree/queue.hpp>
#include <tar_benchmark/tar_benchmark.hpp>

struct unable_to_read_file {};

struct file_t;

struct context_t {
  context_t( const std::string &i ) : input( i ), total_size( 0 ), active_count( 0 ), loaded_count( 0 ), completed_count( 0 ), total_elapsed_time( 0 ), completed( false ) {}
  std::string input;
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
};

void close_file( sigval v );

struct file_t {
  file_t( context_t *c, const std::string &name_, size_t s, size_t o ) : context( c ), name( name_ ), size( s ), offset( o ) {
    cb.aio_fildes = 0;
    cb.aio_offset = 0;
    cb.aio_nbytes = size;
    cb.aio_reqprio = 0;
    cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
    cb.aio_sigevent.sigev_notify_function = close_file;
    cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    cb.aio_lio_opcode = 0;
  }
  file_t( file_t &&src ) : context( src.context ), name( std::move( src.name ) ), begin( std::move( src.begin ) ), cb( src.cb ), size( src.size ) {
    cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
  }
  file_t &operator=( file_t &&src ) {
    context = src.context;
    name = std::move( name );
    begin = std::move( begin );
    cb = src.cb;
    size = src.size;
    cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    return *this;
  }
  void set_buf() {
    cb.aio_buf = std::next( context->buf.get(), offset );
  }
  context_t *context;
  std::string name;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  aiocb cb;
  size_t size;
  size_t offset;
};

void close_file( sigval v ) {
  auto end = std::chrono::high_resolution_clock::now();
  auto file = reinterpret_cast< file_t* >( v.sival_ptr );
  close( file->cb.aio_fildes );
  const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->begin ).count();
  tar_benchmark::record_elapsed_time( *file->context->results, file->size, elapsed );
  if( ++file->context->completed_count == file->context->files.size() ) {
    auto end = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->context->begin ).count();
    file->context->total_elapsed_time = elapsed;
    file->context->completed = true;
  }
  --file->context->active_count;
}

void read_file( file_t &file ) {
  while( 1 ) {
    auto count = file.context->active_count.load();
    if( count < 100u )
      if( file.context->active_count.compare_exchange_strong( count, count + 1u ) )
        break;
  }
  const int fd = open( file.name.c_str(), O_RDONLY );
  if( fd < 0 ) throw unable_to_read_file();
  file.cb.aio_fildes = fd;
  file.begin = std::chrono::high_resolution_clock::now();
  if( aio_read( &file.cb ) < 0 ) throw unable_to_read_file();
}

void load_files( context_t &ctx, const boost::filesystem::path &root ) {
  for( auto iter = boost::filesystem::directory_iterator( root ); iter !=  boost::filesystem::directory_iterator(); ++iter ) {
    if( boost::filesystem::is_directory( *iter ) ) load_files( ctx, *iter );
    else if( boost::filesystem::is_regular_file( *iter ) ) {
      std::string name = iter->path().string();
      size_t size = file_size( *iter );
      if( size > 0 ) {
        ctx.files.emplace_back( &ctx, std::move( name ), size, ctx.total_size );
        ctx.total_size += size;
        ++ctx.loaded_count;
      }
    }
  }
}

void run( const std::string root ) {
  context_t ctx( root );
  load_files( ctx, boost::filesystem::path( root ) );
  ctx.buf.reset(
    new uint8_t[ ctx.total_size ],
    []( uint8_t *p ) { delete[] p; }
  );
  for( auto &f: ctx.files ) f.set_buf();
  ctx.results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( ctx.files.size() ) );
  ctx.begin = std::chrono::high_resolution_clock::now();
  aioinit aio_config;
  aio_config.aio_threads = 100;
  aio_config.aio_num = 1000;
  aio_config.aio_idle_time = 0; 
  aio_init( &aio_config );
#pragma omp parallel for
  for( size_t i = 0u; i < ctx.files.size(); ++i ) {
    auto &file = ctx.files[ i ];
    read_file( file );
  }
  while( !ctx.completed.load() ) sleep( 1 );
  tar_benchmark::print_elapsed_time( *ctx.results );
  std::cout << "total\t" << ctx.total_elapsed_time.load() << std::endl;
}

int main( int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("input,i", po::value<std::string>()->default_value( "input.tar" ), "input");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  run( opt_var[ "input" ].as< std::string >() );
}

