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

struct unable_to_write_file {};

void clean_output_dir( const std::string &output ) {
  boost::system::error_code error;
  boost::filesystem::remove_all( output, error );
  if( error ) throw unable_to_write_file();
}

void create_dirs( const boost::filesystem::path &dir ) {
  boost::system::error_code error;
  boost::filesystem::create_directories( dir, error );
  if( error ) throw unable_to_write_file();
}

struct file_t;

struct context_t {
  context_t( size_t total, size_t regular );
  boost::lockfree::queue< tar_benchmark::result_t > elapsed_time;
  size_t expected_count;
  std::atomic< size_t > completed_count;
  std::vector< file_t > file_stats;
  std::atomic< size_t > active_count;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::atomic< size_t > total_elapsed_time;
  std::atomic< bool > completed;
};

void close_file( sigval v );
void sync_file( sigval v );

struct file_t {
  file_t( context_t *c ) : context( c ) {
    sync_cb.aio_fildes = 0;
    sync_cb.aio_offset = 0;
    sync_cb.aio_buf = nullptr;
    sync_cb.aio_nbytes = 0;
    sync_cb.aio_reqprio = 0;
    sync_cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
    sync_cb.aio_sigevent.sigev_notify_function = close_file;
    sync_cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    sync_cb.aio_lio_opcode = 0;
    write_cb.aio_fildes = 0;
    write_cb.aio_offset = 0;
    write_cb.aio_buf = nullptr;
    write_cb.aio_nbytes = 0;
    write_cb.aio_reqprio = 0;
    write_cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
    write_cb.aio_sigevent.sigev_notify_function = sync_file;
    write_cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    write_cb.aio_lio_opcode = 0;
  }
  file_t( file_t &&src ) : context( src.context ), begin( src.begin ), sync_cb( src.sync_cb ), write_cb( src.write_cb ) {
    sync_cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    write_cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
  }
  file_t &operator=( file_t &&src ) {
    context = src.context;
    begin = src.begin;
    sync_cb = src.sync_cb;
    write_cb = src.write_cb;
    sync_cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    write_cb.aio_sigevent.sigev_value.sival_ptr = reinterpret_cast< void* >( this );
    return *this;
  }
  void set_fd( int fd ) {
    sync_cb.aio_fildes = fd;
    write_cb.aio_fildes = fd;
  }
  void set_size( size_t s ) {
    sync_cb.aio_nbytes = s;
    write_cb.aio_nbytes = s;
  }
  void set_buf( void *buf ) {
    write_cb.aio_buf = buf;
    sync_cb.aio_buf = buf;
  }
  context_t *context;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  aiocb sync_cb;
  aiocb write_cb;
};

context_t::context_t( size_t total, size_t regular ) : elapsed_time( regular ), expected_count( regular ), completed_count( 0 ), active_count( 0 ), total_elapsed_time( 0 ), completed( false ) {
  for( size_t i = 0; i != total; ++i ) file_stats.emplace_back( this );
}

void close_file( sigval v ) {
  auto file = reinterpret_cast< file_t* >( v.sival_ptr );
  close( file->sync_cb.aio_fildes );
  --file->context->active_count;
  auto end = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->begin ).count();
  tar_benchmark::record_elapsed_time( file->context->elapsed_time, file->sync_cb.aio_nbytes, elapsed );
  if( ++file->context->completed_count == file->context->expected_count ) {
    auto end = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->context->begin ).count();
    file->context->total_elapsed_time = elapsed;
    file->context->completed = true;
  }
}

void sync_file( sigval v ) {
  auto file = reinterpret_cast< file_t* >( v.sival_ptr );
  if( aio_fsync( O_DSYNC, &file->sync_cb ) < 0 ) throw unable_to_write_file();
}

void write_file( file_t &file, const std::shared_ptr< uint8_t > &mapped, const tar_benchmark::header &h, const boost::filesystem::path &path ) {
  while( 1 ) {
    auto count = file.context->active_count.load();
    if( count < 1000u )
      if( file.context->active_count.compare_exchange_strong( count, count + 1u ) )
        break;
  }
  int fd = open( path.c_str(), O_WRONLY|O_CREAT, 0644 );
  if( fd < 0 ) throw unable_to_write_file();
  file.set_fd( fd );
  file.set_size( h.size );
  file.set_buf( reinterpret_cast< void* >( mapped.get() + h.offset ) );
  file.begin = std::chrono::high_resolution_clock::now();
  if( aio_write( &file.write_cb ) < 0 ) throw unable_to_write_file();
}

void create_file( file_t &file, const std::shared_ptr< uint8_t > &mapped, const tar_benchmark::header &h, const std::string &output ) {
  const auto path = boost::filesystem::path( output + std::string( h.name_begin, h.name_end ) );
  auto dir = path;
  dir.remove_filename();
  create_dirs( dir );
  write_file( file, mapped, h, path );
}

void run( const std::string &input, const std::string &output ) {
  const auto [mapped,size] = tar_benchmark::load_tar( input );
  auto headers = tar_benchmark::parse_tar( mapped, size );
  clean_output_dir( output );
  size_t expected_count = std::count_if( headers.begin(), headers.end(), []( const auto &h ) { return h.type == tar_benchmark::filetype_t::regular && h.size != 0; } );
  context_t context( headers.size(), expected_count );
  context.begin = std::chrono::high_resolution_clock::now();
#pragma omp parallel for
  for( size_t i = 0u; i < headers.size(); ++i ) {
    const auto &h = headers[ i ];
    if( h.type == tar_benchmark::filetype_t::regular && h.size != 0 ) {
      create_file( context.file_stats[ i ], mapped, h, output );
    }
  }
  while( !context.completed.load() ) sleep( 1 );
  tar_benchmark::print_elapsed_time( context.elapsed_time );
  std::cout << "total\t" << context.total_elapsed_time.load() << std::endl;
}

int main( int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("input,i", po::value<std::string>()->default_value( "input.tar" ), "input")
    ("output,o", po::value<std::string>()->default_value( "out/" ), "output");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  run( opt_var[ "input" ].as< std::string >(), opt_var[ "output" ].as< std::string >() );
}

