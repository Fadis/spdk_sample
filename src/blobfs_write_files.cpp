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
  context_t( const std::string &i, bool flush_ ) : input( i ), flush( flush_ ), bdev( nullptr ), bs( nullptr ), fs( nullptr ), completed_count( 0 ), clean_count( 0 ) {}
  std::string input;
  bool flush;
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_filesystem *fs;
  std::shared_ptr< uint8_t > buffer;
  size_t tar_size;
  std::vector< tar_benchmark::header > headers;
  std::shared_ptr< spdk_io_channel > channel;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::vector< file_t > files;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::atomic< size_t > completed_count;
  std::atomic< size_t > clean_count;
  size_t total_elapsed_time;
};

template< typename Iterator >
std::string escape_file_name( Iterator begin, Iterator end ) {
  std::string name;
  for( auto iter = begin; iter != end; ++iter ) {
    if( *iter == '/' ) name += "_";
    else if( *iter == '_' ) name += "__";
    else name += *iter;
  }
  return name;
}

struct file_t {
  file_t( context_t *c, tar_benchmark::header *h, size_t i, size_t n ) : context( c ), header( h ), name( escape_file_name( h->name_begin, h->name_end ) ), lcore( i ), index( n ), fd( nullptr ) {}
  context_t *context;
  tar_benchmark::header *header;
  std::string name;
  size_t lcore;
  size_t index;
  spdk_file *fd;
};

#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    throw -1; \
    return; \
  }

void close_storage( context_t *context ) {
  spdk_event *event = spdk_event_allocate( 0,
    []( void *context_, void* ) {
      auto context = reinterpret_cast< context_t* >( context_ );
      spdk_fs_unload( context->fs, []( void *context_, int bserrno ) {
        auto context = reinterpret_cast< context_t* >( context_ );
        if( bserrno ) ABORT( "ファイルシステムを閉じる事ができない" )
        context->buffer.reset();
        tar_benchmark::print_elapsed_time( *context->results );
        std::cout << "total\t" << context->total_elapsed_time << std::endl;
        spdk_app_stop( 0 );
      }, context_ );
    }, reinterpret_cast< void* >( context ), nullptr
  );
  spdk_event_call( event );
}

void create_file( void *file_, void* ) {
  auto file = reinterpret_cast< file_t* >( file_ );
  if( spdk_fs_create_file( file->context->fs, file->context->channel.get(), file->name.c_str() ) < 0 )
    ABORT( "ファイルを作成する事ができない" );
  if( spdk_fs_open_file( file->context->fs, file->context->channel.get(), file->name.c_str(), 0, &file->fd ) < 0 )
    ABORT( "ファイルを開く事ができない" );
  auto head = std::next( file->context->buffer.get(), file->header->offset );
  auto begin = std::chrono::high_resolution_clock::now();
  if( spdk_file_write( file->fd, file->context->channel.get(), head, 0, file->header->size ) < 0 )
    ABORT( "ファイルを書き込む事ができない" );
  if( file->context->flush ) {
    if( spdk_file_sync( file->fd, file->context->channel.get() ) < 0 )
      ABORT( "ファイルキャッシュを同期する事ができない" );
  }
  auto end = std::chrono::high_resolution_clock::now();
  tar_benchmark::record_elapsed_time(
    *file->context->results,
    file->header->size,
    std::chrono::duration_cast< std::chrono::nanoseconds >( end - begin ).count()
  );
  if( spdk_file_close( file->fd, file->context->channel.get() ) < 0 )
    ABORT( "ファイルを閉じる事ができない" );
}

void create_channels( context_t *context ) {
  spdk_event *event = spdk_event_allocate( 1,
    []( void *context_, void* ) {
      auto context = reinterpret_cast< context_t* >( context_ );
      context->channel.reset(
        spdk_fs_alloc_io_channel_sync( context->fs ),
        []( spdk_io_channel *p ) { spdk_fs_free_io_channel( p ); }
      );
      context->begin = std::chrono::high_resolution_clock::now();
      for( auto &file: context->files ) create_file( &file, nullptr );
      auto end = std::chrono::high_resolution_clock::now();
      context->channel.reset();
      context->total_elapsed_time = std::chrono::duration_cast< std::chrono::nanoseconds >( end - context->begin ).count();
      context->buffer.reset();
      close_storage( context );
    }, reinterpret_cast< void* >( context ), nullptr
  );
  spdk_event_call( event );
}

void init_storage( void *context_, void * ) {
  auto context = reinterpret_cast< context_t* >( context_ );
  context->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !context->bdev ) ABORT( "デバイスが見つからない" );
  context->bs = spdk_bdev_create_bs_dev( context->bdev, NULL, NULL );
  if( !context->bs ) ABORT( "blobstoreデバイスを作成できない" );
  spdk_fs_set_cache_size( 512 );
  spdk_fs_init( context->bs, nullptr,
    []( fs_request_fn f, void *arg ) {
      spdk_event *event = spdk_event_allocate( 0, []( void *arg1, void *arg2 ) {
        reinterpret_cast< fs_request_fn >( arg1 )( arg2 );
      }, (void *)f, arg );
      spdk_event_call( event );
    },
    []( void *context_, struct spdk_filesystem *fs, int fserrno ) {
      auto context = reinterpret_cast< context_t* >( context_ );
      if( fserrno ) ABORT( "ファイルシステムを作成できない" )
      context->fs = fs;
      auto [mapped_tar,tar_size] = tar_benchmark::load_tar( context->input );
      context->tar_size = tar_size;
      context->buffer.reset(
        reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( context->tar_size, 0x1000, nullptr ) ),
        []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
      );
      memcpy( reinterpret_cast< void* >( context->buffer.get() ), reinterpret_cast< void* >( mapped_tar.get() ), context->tar_size );
      mapped_tar.reset();
      context->headers = tar_benchmark::parse_tar( context->buffer, context->tar_size );
      context->results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( context->headers.size() ) );
      for( auto &header: context->headers ) {
        if( header.type == tar_benchmark::filetype_t::regular && header.size > 0 )
          context->files.emplace_back( context, &header, header.size, header.offset );
      }
      create_channels( context );
    }, context_
  );
}
int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file")
    ("flush,f", po::value<bool>()->default_value( false ), "flush")
    ("input,i", po::value<std::string>()->default_value( "input.tar" ), "input");
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
  context_t ctx( opt_var[ "input" ].as< std::string >(), opt_var[ "flush" ].as< bool >() );
  spdk_app_start(&opts, init_storage, &ctx, nullptr );
}
