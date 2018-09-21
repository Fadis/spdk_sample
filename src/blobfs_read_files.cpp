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
#include <spdk/blobfs.h>
#include <spdk/log.h>
#include <tar_benchmark/tar_benchmark.hpp>

struct file_t;

struct context_t {
  context_t() :  bdev( nullptr ), bs( nullptr ), fs( nullptr ), loaded_count( 0 ), completed_count( 0 ) {}
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_filesystem *fs;
  std::shared_ptr< uint8_t > buf;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::shared_ptr< spdk_io_channel > channel;
  std::vector< file_t > files;
  size_t loaded_count;
  size_t completed_count;
  size_t total_elapsed_time;
};

struct file_t {
  file_t( context_t *c, const std::string &name_, size_t s, size_t o ) : context( c ), name( name_ ), size( s ), offset( o ), fd( nullptr ) {}
  context_t *context;
  std::string name;
  size_t size;
  size_t offset;
  spdk_file *fd;
};

#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    throw -1; \
    return; \
  }

void read_file( void *ctx_, void* ) {
  auto ctx = reinterpret_cast< context_t* >( ctx_ );
  size_t offset = 0;
  size_t file_count = 0;
  for( auto iter = spdk_fs_iter_first( ctx->fs ); iter; iter = spdk_fs_iter_next( iter ), ++file_count ) {
    std::string name = spdk_file_get_name( iter );
    size_t size = spdk_file_get_length( iter );
    ctx->files.emplace_back( ctx, std::move( name ), size, offset );
    size_t page_size = 0x1000;
    offset += ( size / page_size + ( ( size % page_size ) ? 1u : 0u ) ) * page_size;
  }
  ctx->channel.reset(
    spdk_fs_alloc_io_channel( ctx->fs ),
    []( spdk_io_channel *p ) { spdk_fs_free_io_channel( p ); }
  );
  ctx->buf.reset(
    reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( offset, 0x1000, nullptr ) ),
    []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
  );
  ctx->results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( file_count ) );
  auto gbegin = std::chrono::high_resolution_clock::now();
  for( auto &file: ctx->files ) {
    if( spdk_fs_open_file( ctx->fs, ctx->channel.get(), file.name.c_str(), 0, &file.fd ) < 0 )
      ABORT( "ファイルを開く事ができない" );
    auto begin = std::chrono::high_resolution_clock::now();
    if( spdk_file_read( file.fd, ctx->channel.get(), std::next( ctx->buf.get(), file.offset ), 0, file.size ) < 0 )
      ABORT( "ファイルを読み出す事ができない" );
    auto end = std::chrono::high_resolution_clock::now();
    tar_benchmark::record_elapsed_time(
      *ctx->results,
      file.size,
      std::chrono::duration_cast< std::chrono::nanoseconds >( end - begin ).count()
    );
    if( spdk_file_close( file.fd, ctx->channel.get() ) < 0 )
      ABORT( "ファイルを閉じる事ができない" );
  }
  auto gend = std::chrono::high_resolution_clock::now();
  ctx->total_elapsed_time = std::chrono::duration_cast< std::chrono::nanoseconds >( gend - gbegin ).count();
  ctx->channel.reset();
  ctx->buf.reset();
  spdk_fs_unload( ctx->fs, []( void *ctx_, int fserrno ) {
    if( fserrno ) ABORT( "ファイルシステムを閉じる事ができない" )
    auto ctx = reinterpret_cast< context_t* >( ctx_ );
    ctx->fs = nullptr;
    tar_benchmark::print_elapsed_time( *ctx->results );
    std::cout << "total\t" << ctx->total_elapsed_time << std::endl;
    spdk_app_stop( 0 );
  }, ctx_ );
}

void load_storage( void *ctx_, void * ) {
  auto ctx = reinterpret_cast< context_t* >( ctx_ );
  ctx->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !ctx->bdev ) ABORT( "デバイスが見つからない" );
  ctx->bs = spdk_bdev_create_bs_dev( ctx->bdev, NULL, NULL );
  if( !ctx->bs ) ABORT( "blobstoreデバイスを作成できない" );
  spdk_fs_set_cache_size( 512 );
  spdk_fs_load( ctx->bs,
    []( fs_request_fn f, void *arg ) {
      spdk_event *event = spdk_event_allocate( 0, []( void *arg1, void *arg2 ) {
        reinterpret_cast< fs_request_fn >( arg1 )( arg2 );
      }, (void *)f, arg );
      spdk_event_call( event );
    },
    []( void *ctx_, struct spdk_filesystem *fs, int fserrno ) {
      auto ctx = reinterpret_cast< context_t* >( ctx_ );
      if( fserrno ) ABORT( "ファイルシステムをロードできない" )
      ctx->fs = fs;
      spdk_event *event = spdk_event_allocate( 1, read_file, ctx_, nullptr );
      spdk_event_call( event );
    }, ctx_
  );
}

int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file");
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
  context_t ctx;
  spdk_app_start(&opts, load_storage, &ctx, nullptr );
}

