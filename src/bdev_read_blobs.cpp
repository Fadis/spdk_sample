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
#include <spdk/log.h>
#include <tar_benchmark/tar_benchmark.hpp>

struct blob_t;

struct context_t {
  context_t() : bdev( nullptr ), bs( nullptr ), blobstore( nullptr ), page_size( 0 ), cluster_count( 0 ), total_size( 0 ), max_concurrency( 0 ), head( 0 ), opening_count( 0 ), closing_count( 0 ), loaded_count( 0 ), completed_count( 0 ), channel_count( 0 ), reading_count( 0 ) {}
  std::string output;
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_blob_store *blobstore;
  size_t page_size;
  size_t cluster_count;
  size_t total_size;
  size_t max_concurrency;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::vector< std::shared_ptr< spdk_io_channel > > channels;
  std::vector< blob_t > blobs;
  std::shared_ptr< uint8_t > buf;
  std::atomic< size_t > head;
  std::atomic< size_t > opening_count;
  std::atomic< size_t > closing_count;
  std::atomic< size_t > loaded_count;
  std::atomic< size_t > completed_count;
  std::atomic< size_t > channel_count;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::atomic< size_t > total_elapsed_time;
  std::atomic< size_t > reading_count;
  std::atomic< bool > completed;
};

struct blob_t {
  blob_t( context_t *c, spdk_blob_id b, size_t i, size_t n, size_t s, size_t o ) : context( c ), blobid( b ), lcore( i ), index( n ), blob( nullptr ), page_count( s ), offset( o ) {}
  context_t *context;
  spdk_blob_id blobid;
  size_t lcore;
  size_t index;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  spdk_blob *blob;
  size_t page_count;
  size_t offset;
};

#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    throw -1; \
    return; \
  }

void close_storage( void *context_, void* ) {
  auto context = reinterpret_cast< context_t* >( context_ );
  spdk_bs_unload( context->blobstore, []( void *context_, int bserrno ) {
    auto context = reinterpret_cast< context_t* >( context_ );
    if( bserrno ) ABORT( "blobstoreを閉じる事ができない" )
    context->blobstore = nullptr;
    tar_benchmark::print_elapsed_time( *context->results );
    std::cout << "total\t" << context->total_elapsed_time.load() << std::endl;
    spdk_app_stop( 0 );
    context->completed = true;
  }, context_ );
}

void open_blob( void *blob_, void* );

void close_blob( void *blob_, void* ) {
  auto blob = reinterpret_cast< blob_t* >( blob_ );
  if( blob->context->closing_count >= 32u ) {
    spdk_event *event = spdk_event_allocate( 0, close_blob, blob_, nullptr );
    spdk_event_call( event );
    return;
  }
  ++blob->context->closing_count;
  spdk_blob_close( blob->blob,
    []( void *blob_, int bserrno ) {
      auto blob = reinterpret_cast< blob_t* >( blob_ );
      if( bserrno ) ABORT( "blobを開くことが出来ない" );
      --blob->context->closing_count;
      blob->blob = nullptr;
      if( ++blob->context->completed_count == blob->context->blobs.size() ) {
        auto end = std::chrono::high_resolution_clock::now();
        blob->context->buf.reset();
        for( auto &c: blob->context->channels ) c.reset();
        const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - blob->context->begin ).count();
        blob->context->total_elapsed_time = elapsed;
        close_storage( blob->context, nullptr );
      }
      else {
        const auto next = ++blob->context->head;
        if( next <= blob->context->blobs.size() ) {
          size_t current = next - 1;
          blob->context->blobs[ current ].lcore = rte_lcore_id();
          spdk_event *event = spdk_event_allocate( 0, open_blob, &blob->context->blobs[ current ], nullptr );
          spdk_event_call( event );
        }
      }


    }, blob_
  );
}

void read_blob( void *blob_, void* ) {
  auto blob = reinterpret_cast< blob_t* >( blob_ );
  if( blob->context->max_concurrency ) {
    size_t active = blob->context->reading_count.load();
    if( active >= blob->context->max_concurrency ) {
      spdk_event *event = spdk_event_allocate( 0, read_blob, blob_, nullptr );
      spdk_event_call( event );
      return;
    }
    if( !blob->context->reading_count.compare_exchange_strong( active, active + 1 ) ) {
      spdk_event *event = spdk_event_allocate( 0, read_blob, blob_, nullptr );
      spdk_event_call( event );
      return;
    }
  }
  if( !blob->context->channels[ blob->lcore ] ) {
    blob->context->channels[ blob->lcore ].reset(
      spdk_bs_alloc_io_channel( blob->context->blobstore ),
      []( spdk_io_channel *p ) { spdk_bs_free_io_channel( p ); }
    );
  }
  blob->begin = std::chrono::high_resolution_clock::now();
  spdk_blob_io_read(
    blob->blob,
    blob->context->channels[ blob->lcore ].get(),
    std::next( blob->context->buf.get(), blob->offset ), 0, blob->page_count,
    []( void *blob_, int bserrno ) {
      auto end = std::chrono::high_resolution_clock::now();
      auto blob = reinterpret_cast< blob_t* >( blob_ );
      if( blob->context->max_concurrency ) {
        --blob->context->reading_count;
      }
      tar_benchmark::record_elapsed_time(
        *blob->context->results,
        blob->page_count * blob->context->page_size,
        std::chrono::duration_cast< std::chrono::nanoseconds >( end - blob->begin ).count()
      );
      if( bserrno ) ABORT( "blobを読み出す事が出来ない" );
      spdk_event *event = spdk_event_allocate( 0, close_blob, blob_, nullptr );
      spdk_event_call( event );
    }, blob_
  );
}

void open_blob( void *blob_, void* ) {
  auto blob = reinterpret_cast< blob_t* >( blob_ );
  if( blob->context->opening_count >= 32u ) {
    spdk_event *event = spdk_event_allocate( 0, open_blob, blob_, nullptr );
    spdk_event_call( event );
    return;
  }
  ++blob->context->opening_count;
  spdk_bs_open_blob( blob->context->blobstore, blob->blobid,
    []( void *blob_, struct spdk_blob *b, int bserrno ) {
      auto blob = reinterpret_cast< blob_t* >( blob_ );
      if( bserrno ) ABORT( "blobを開くことが出来ない" );
      --blob->context->opening_count;
      blob->blob = b;
      spdk_event *event = spdk_event_allocate( blob->lcore, read_blob, blob_, nullptr );
      spdk_event_call( event );
    }, blob_
  );
}

void load_blobs( void *context_, struct spdk_blob *blob, int bserrno ) {
  auto context = reinterpret_cast< context_t* >( context_ );
  if( bserrno == -ENOENT ) {
    context->buf.reset(
      reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( context->total_size, context->page_size, nullptr ) ),
      []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
    );
    if( !context->buf ) ABORT( "バッファを確保する事ができない" )
    if( context->max_concurrency == 0 ) context->max_concurrency = context->blobs.size();
    context->head = context->max_concurrency;
    context->begin = std::chrono::high_resolution_clock::now();
    for( size_t i = 0; i < context->max_concurrency; ++i ) {
      spdk_event *event = spdk_event_allocate( 0, open_blob, reinterpret_cast< void* >( &context->blobs[ i ] ), nullptr );
      spdk_event_call( event );
    }
    if( context->blobs.empty() ) {
      spdk_event *event = spdk_event_allocate( 0, close_storage, context_, nullptr );
      spdk_event_call( event );
    }
    return;
  }
  if( bserrno ) ABORT( "blobを取得できない" );
  const auto blob_index = context->loaded_count++;
  const size_t cores = rte_lcore_count();
  const auto bid = spdk_blob_get_id( blob );
  const auto size = spdk_blob_get_num_pages( blob ) / 256;
  context->blobs.emplace_back( context, bid, blob_index % cores, blob_index / cores, size, context->total_size );
  context->total_size += size * context->page_size;
  spdk_bs_iter_next(
    context->blobstore,
    blob,
    load_blobs,
    context_
  );
}

void create_channels( context_t *context ) {
  context->channels.resize( rte_lcore_count() );
  for( size_t i = 0u; i != context->channels.size(); ++i ) {
    spdk_event *event = spdk_event_allocate( i,
      []( void *context_, void* ) {
        auto context = reinterpret_cast< context_t* >( context_ );
	const auto lcore = rte_lcore_id();
        context->channels[ lcore ].reset(
          spdk_bs_alloc_io_channel( context->blobstore ),
          []( spdk_io_channel *p ) { spdk_bs_free_io_channel( p ); }
        );
        if( ++context->channel_count == context->channels.size() ) {
          spdk_event *event = spdk_event_allocate( 0,
	    []( void *context_, void* ) {
              auto context = reinterpret_cast< context_t* >( context_ );
              context->begin = std::chrono::high_resolution_clock::now();
              spdk_bs_iter_first(
                context->blobstore,
                load_blobs,
                context_
              );
	    }, context_, nullptr
	  );
          spdk_event_call( event );
        }
      }, reinterpret_cast< void* >( context ), nullptr
    );
    spdk_event_call( event );
  }
}

void load_storage( void *context_, void * ) {
  auto context = reinterpret_cast< context_t* >( context_ );
  context->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !context->bdev ) ABORT( "デバイスが見つからない" );
  context->bs = spdk_bdev_create_bs_dev( context->bdev, NULL, NULL );
  if( !context->bs ) ABORT( "blobstoreデバイスを作成できない" );
  struct spdk_bs_opts opts;
  spdk_bs_opts_init( &opts );
  opts.max_channel_ops = 8000;
  spdk_bs_load( context->bs, &opts, []( void *context_, struct spdk_blob_store *blobstore, int bserrno ) {
    auto context = reinterpret_cast< context_t* >( context_ );
    if( bserrno ) ABORT( "blobstoreをロードできない" );
    context->blobstore = blobstore;
    context->page_size = spdk_bs_get_page_size( context->blobstore );
    std::cout << "ページサイズ: " << context->page_size << std::endl;
    context->cluster_count = spdk_bs_total_data_cluster_count( context->blobstore );
    std::cout << "クラスタ数: " << context->cluster_count << std::endl;
    context->results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( context->cluster_count ) );
    const size_t cores = rte_lcore_count();
    context->channels.resize( cores );
    create_channels( context );
  }, context_ );
}
int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file")
    ("concurrency,x", po::value<size_t>()->default_value( 0 ), "max concurrency");
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
  ctx.max_concurrency = opt_var[ "concurrency" ].as< size_t >();
  spdk_app_start(&opts, load_storage, &ctx, nullptr );
}
