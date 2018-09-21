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
#include <numeric>
#include <boost/spirit/include/karma.hpp>
#include <boost/program_options.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <dpdk/rte_lcore.h>
#pragma GCC diagnostic pop
#include <spdk/event.h>
#include <spdk/bdev.h>
#include <spdk/env.h>
#include <spdk/blob_bdev.h>
#include <spdk/blob.h>
#include <spdk/log.h>
#include <spdk/io_channel.h>
#include <tar_benchmark/tar_benchmark.hpp>

struct file_t;

spdk_nvme_cmd get_flush_command() {
  spdk_nvme_cmd command;
  memset( &command, 0, sizeof( spdk_nvme_cmd ) );
  return command;
}

struct context_t {
  context_t( size_t block_size_, size_t count_, bool write_, bool random_, bool zero_, bool flush_ ) : page_size( 4096 ), block_size( block_size_ ), count( count_ ), write( write_ ), random( random_ ), zero( zero_ ), flush( flush_ ), bdev( nullptr ), step( 0 ), flush_command( get_flush_command() ) {
  }
  size_t page_size;
  size_t block_size;
  size_t count;
  bool write;
  bool random;
  bool zero;
  bool flush;
  spdk_bdev *bdev;
  spdk_bdev_desc* desc;
  std::shared_ptr< spdk_io_channel > channel;
  std::vector< file_t > files;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::shared_ptr< uint8_t > buffer;
  size_t step; 
  spdk_nvme_cmd flush_command;
};

struct file_t {
  file_t( context_t *context_, size_t lcore_, size_t size_, size_t offset_ ) : context( context_ ), lcore( lcore_ ), size( size_ ), offset( offset_ ), elapsed( 0 ) {}
  context_t *context;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  size_t lcore;
  size_t size;
  size_t offset;
  size_t elapsed;
};

#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    throw -1; \
    return; \
  }

void io_file( void *file_, void* );

void close( file_t *file, const std::chrono::time_point<std::chrono::high_resolution_clock> &end ) {
  if( file->context->step + 1 == file->context->files.size() ) {
    file->context->channel.reset();
    spdk_bdev_close( file->context->desc );
    file->context->buffer.reset();
    const size_t elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->context->begin ).count();
    const size_t transfered = file->context->block_size * file->context->count * file->context->page_size;
    const size_t io_elapsed = std::accumulate( file->context->files.begin(), file->context->files.end(), size_t( 0 ), []( size_t sum, const auto &v ) { return sum + v.elapsed; } );
    std::cout << file->context->block_size * file->context->page_size << "\t" << transfered << "\t" << elapsed << "\t" << file->context->files.size() << "\t" << ( double( transfered ) / 1000 / 1000 ) / ( double( elapsed ) / 1000 / 1000 / 1000 ) << "MB/s\t" << elapsed / file->context->files.size() << "ns\t" << io_elapsed << "\t" << elapsed / file->context->files.size() << "ns" << std::endl;
    spdk_app_stop( 0 );
  }
  else {
    spdk_event *event = spdk_event_allocate( 0, io_file, &file->context->files[ ++file->context->step ], nullptr );
    spdk_event_call( event );
  }
}

void io_file( void *file_, void* ) {
  auto file = reinterpret_cast< file_t* >( file_ );
  if( !file->context->write ) {
    file->begin = std::chrono::high_resolution_clock::now();
    if( spdk_bdev_read_blocks(
      file->context->desc,
      file->context->channel.get(),
      file->context->buffer.get(),
      file->offset,
      file->size,
      []( struct spdk_bdev_io *bdev_io, bool success, void *file_ ) {
        auto end = std::chrono::high_resolution_clock::now();
        auto file = reinterpret_cast< file_t* >( file_ );
	spdk_bdev_free_io( bdev_io );
        if( !success ) ABORT( "ファイルを読むことができない" );
	file->elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->begin ).count();
	close( file, end );
      }
      , file_
    ) < 0 ) ABORT( "ファイルを読むことができない" );
  }
  else {
    file->begin = std::chrono::high_resolution_clock::now();
    if( spdk_bdev_write_blocks(
      file->context->desc,
      file->context->channel.get(),
      file->context->buffer.get(),
      file->offset,
      file->size,
      []( struct spdk_bdev_io *bdev_io, bool success, void *file_ ) {
        auto file = reinterpret_cast< file_t* >( file_ );
	spdk_bdev_free_io( bdev_io );
        if( !success ) ABORT( "ファイルを書くことができない" );
	if( file->context->flush ) {
	  if( spdk_bdev_nvme_io_passthru(
	    file->context->desc,
	    file->context->channel.get(),
	    &file->context->flush_command,
	    nullptr,
	    0,
	  /*if( spdk_bdev_flush_blocks(
	    file->context->desc,
	    file->context->channel.get(),
	    file->offset,
	    file->size,*/
            []( struct spdk_bdev_io *bdev_io, bool success, void *file_ ) {
              auto end = std::chrono::high_resolution_clock::now();
              auto file = reinterpret_cast< file_t* >( file_ );
	      spdk_bdev_free_io( bdev_io );
              if( !success ) ABORT( "ファイルを書くことができない" );
	      close( file, end );
	    }, file_
	  ) < 0 ) ABORT( "ファイルを書くことができない" );
	}
	else {
          auto end = std::chrono::high_resolution_clock::now();
	  close( file, end );
	}
      }
      , file_
    ) < 0 ) ABORT( "ファイルを書くことができない" );
  }
}

void run( void *ctx_, void * ) {
  auto ctx = reinterpret_cast< context_t* >( ctx_ );
  ctx->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !ctx->bdev ) ABORT( "デバイスが見つからない" );
  ctx->page_size = spdk_bdev_get_block_size( ctx->bdev );
  const auto max_page_count = spdk_bdev_get_num_blocks( ctx->bdev );
  const size_t count = std::min( ctx->count, max_page_count );
  const size_t buf_size = ctx->page_size * ctx->block_size;
  ctx->buffer.reset(
    reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( buf_size, ctx->page_size, nullptr ) ),
    []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
  );
  if( !ctx->buffer ) ABORT( "バッファを確保する事ができない" )
  std::fill( ctx->buffer.get(), std::next( ctx->buffer.get(), buf_size ), ctx->zero ? 0 : 1 );
  //const size_t cores = rte_lcore_count();
  ctx->files.reserve( count );
  if( spdk_bdev_open( ctx->bdev, true, nullptr, nullptr, &ctx->desc ) < 0 ) ABORT( "デバイスを開くことができない" );
  ctx->channel.reset(
    spdk_bdev_get_io_channel( ctx->desc ),
    []( spdk_io_channel *p ) { spdk_put_io_channel( p ); }
  );
  if( !ctx->channel ) ABORT( "チャネルを確保できない" );
  for( size_t i = 0; i != count; ++i ) {
    ctx->files.emplace_back( ctx, 0, ctx->block_size, ctx->random ? ( ( rand() % max_page_count ) / ctx->block_size ) * ctx->block_size : i * ctx->block_size );
  }
  ctx->begin = std::chrono::high_resolution_clock::now();
  spdk_event *event = spdk_event_allocate( 0, io_file, &ctx->files[ 0 ], nullptr );
  spdk_event_call( event );
}

int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file")
    ("block_size,b", po::value<size_t>()->default_value( 1 ), "bs")
    ("count,n", po::value<size_t>()->default_value( 1 ), "count")
    ("write,w", po::value<bool>()->default_value( false ), "write")
    ("random,r", po::value<bool>()->default_value( false ), "random")
    ("zero,z", po::value<bool>()->default_value( false ), "zero")
    ("flush,f", po::value<bool>()->default_value( false ), "flush");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  struct spdk_app_opts opts = {};
  SPDK_NOTICELOG("entry\n");
  spdk_app_opts_init(&opts);
  opts.name = "bdev";
  const std::string config_file = opt_var[ "config" ].as< std::string >();
  opts.config_file = config_file.c_str();
  context_t ctx(
    opt_var[ "block_size" ].as< size_t >(),
    opt_var[ "count" ].as< size_t >(),
    opt_var[ "write" ].as< bool >(),
    opt_var[ "random" ].as< bool >(),
    opt_var[ "zero" ].as< bool >(),
    opt_var[ "flush" ].as< bool >()
  );
  spdk_app_start(&opts, run, &ctx, nullptr );
}
