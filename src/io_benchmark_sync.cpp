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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <boost/program_options.hpp>
struct io_failure {};
struct file_t;
struct context_t {
  context_t() : block_size( 512 ) {}
  size_t block_size;
  std::vector< file_t > files;
};
struct file_t {
  file_t( context_t *c, int f_, size_t size_, size_t offset_, bool zero ) : context( c ), fd( f_ ), size( size_ ), offset( offset_ ) {
    uint8_t *buf;
    if( posix_memalign( (void**)&buf, context->block_size, context->block_size * size ) < 0 ) std::bad_alloc();
    buffer.reset( buf );
    std::fill( buffer.get(), buffer.get() + context->block_size * size, zero ? 0 : 1 );
  }
  file_t( file_t &&src ) : context( src.context ), fd( src.fd ), buffer( std::move( src.buffer ) ), size( src.size ), offset(src.offset ) {}
  file_t &operator=( file_t &&src ) {
    context = src.context;
    fd = src.fd;
    size = src.size;
    offset = src.offset;
    buffer = std::move( src.buffer );
    return *this;
  }
  context_t *context;
  int fd;
  std::shared_ptr< uint8_t > buffer;
  size_t size;
  size_t offset;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
};
void run( const std::string &output, size_t bs, size_t count, bool write, bool random, bool zero ) {
  context_t ctx;
  const int fd = open( output.c_str(), write ? O_WRONLY|O_DIRECT|O_SYNC : O_RDONLY|O_DIRECT|O_SYNC );
  if( fd < 0 ) throw io_failure();
  struct stat file_stat;
  if( fstat( fd, &file_stat ) < 0 ) throw io_failure();
  size_t available = 0;
  if( ( file_stat.st_mode & S_IFMT ) == S_IFREG )
    available = file_stat.st_size / ctx.block_size / bs;
  else if( ( file_stat.st_mode & S_IFMT ) == S_IFBLK ) {
    size_t blk_size;
    ioctl( fd, BLKGETSIZE64, &blk_size );
    available = blk_size / ctx.block_size / bs;
  }
  else throw io_failure();
  count = std::min( count, available );
  for( size_t i = 0; i != count; ++i )
    ctx.files.emplace_back( &ctx, fd, bs, random ? ( rand() % available ) * bs : i * bs, zero );
  const auto begin = std::chrono::high_resolution_clock::now();
  if( write ) {
    for( auto &file: ctx.files )
      if( pwrite( file.fd, reinterpret_cast< void* >( file.buffer.get() ), file.size * ctx.block_size, file.offset * ctx.block_size ) < 0 ) throw io_failure();
  }
  else {
    for( auto &file: ctx.files )
      if( pread( file.fd, reinterpret_cast< void* >( file.buffer.get() ), file.size * ctx.block_size, file.offset * ctx.block_size ) < 0 ) throw io_failure();
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const size_t elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - begin ).count();
  const size_t transfered = bs * count * ctx.block_size;
  close( fd );
  std::cout << bs * ctx.block_size << "\t" << transfered << "\t" << elapsed << "\t" << ctx.files.size() << "\t" << ( double( transfered ) / 1000 / 1000 ) / ( double( elapsed ) / 1000 / 1000 / 1000 ) << "MB/s\t" << elapsed / ctx.files.size() << "ns" << std::endl;
}
int main( int argc, char **argv ) {
  srand( time( nullptr ) );
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("output,o", po::value<std::string>()->default_value( "/dev/nvme0n1" ), "target")
    ("block_size,b", po::value<size_t>()->default_value( 1 ), "bs")
    ("count,n", po::value<size_t>()->default_value( 1 ), "count")
    ("write,w", po::value<bool>()->default_value( false ), "write")
    ("random,r", po::value<bool>()->default_value( false ), "random")
    ("zero,z", po::value<bool>()->default_value( false ), "zero")
    ("repeat,p", po::value<size_t>()->default_value( 1 ), "repeat");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  for( size_t i = 0; i != opt_var[ "repeat" ].as< size_t >(); ++i )
    run( opt_var[ "output" ].as< std::string >(), opt_var[ "block_size" ].as< size_t >(), opt_var[ "count" ].as< size_t >(), opt_var[ "write" ].as< bool >(), opt_var[ "random" ].as< bool >(), opt_var[ "zero" ].as< bool >() );
}

