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

struct file_t;

struct context_t {
  context_t( const std::string &i, size_t max_meta_concurrency_, size_t max_concurrency_ ) : input( i ), max_meta_concurrency( max_meta_concurrency_ ), max_concurrency( max_concurrency_ ), block_size( 512 ), total_size( 0 ), page_size( 4096 ), channel_count( 0 ), completed_count( 0 ), meta_count( 0 ), clean_count( 0 ), head( 0 ), total_elapsed_time( 0 ) {}
  std::string input;
  size_t max_meta_concurrency;
  size_t max_concurrency;
  size_t block_size;
  size_t total_size;
  size_t file_count;
  size_t page_size;
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_blob_store *blobstore;
  spdk_blob *blob;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::vector< file_t > files;
  std::shared_ptr< uint8_t > buffer;
  std::vector< tar_benchmark::header > headers;
  std::vector< std::shared_ptr< spdk_io_channel > > channels;
  std::atomic< size_t > channel_count;
  std::atomic< size_t > completed_count;
  std::atomic< size_t > meta_count;
  std::atomic< size_t > clean_count;
  std::atomic< size_t > head;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::atomic< size_t > total_elapsed_time;
};

void close_file( file_t& );

struct file_t {
  file_t( context_t *c, size_t i, size_t s, size_t o ) : context( c ), index( i ), size( s ), offset( o ), page_count( 0 ), lcore( 0 ) {}
  context_t *context;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  size_t index;
  size_t size;
  size_t offset;
  size_t page_count;
  size_t lcore;
  spdk_blob *fd;
  spdk_blob_id id;
};




/*
struct channel_t;

struct context_t {
  context_t( const std::string &i ) : input( i ), max_concurrency( 0 ), bdev( nullptr ), bs( nullptr ), blobstore( nullptr ), page_size( 0 ), completed_count( 0 ), creating_count( 0 ), closing_count( 0 ), total_elapsed_time( 0 ), writing_count( 0 ), completed( false ) {}
  std::string input;
  size_t max_concurrency;
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_blob_store *blobstore;
  size_t page_size;
  spdk_blob *blob;
  std::shared_ptr< uint8_t > buf;
  size_t tar_size;
  std::vector< tar_benchmark::header > headers;
  std::unique_ptr< boost::lockfree::queue< tar_benchmark::result_t > > results;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  std::vector< std::shared_ptr< spdk_io_channel > > channels;
  std::vector< channel_t > channels;
  std::vector< blob_t > blobs;
  size_t active_count;
  size_t creating_count;
  size_t closing_count;
  size_t total_elapsed_time;
  std::atomic< size_t > writing_count;
  bool completed;
};

struct blob_t;

struct channel_t {
  channel_t(
    context_t *c,
    size_t core_id_,
    tar_benchmark::header *b,
    tar_benchmark::header *e
  ) : context( c ), core_id( core_id_ ), begin( b ), end( e ), active_count( 0 ) {}
  context_t *context;
  size_t core_id;
  std::shared_ptr< spdk_io_channel > channel;
  tar_benchmark::header *begin;
  tar_benchmark::header *end;
  size_t active_count;
};

struct blob_t {
  blob_t( channel_t *c, tar_benchmark::header *h, size_t i, size_t n ) : channel( c ), header( h ), lcore( i ), index( n ), blob( nullptr ), page_count( 0 ) {}
  channel_t *channel;
  tar_benchmark::header *header;
  size_t lcore;
  size_t index;
  spdk_blob *blob;
  std::chrono::time_point<std::chrono::high_resolution_clock> begin;
  spdk_blob_id blobid;
  size_t page_count;
};
*/
#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    throw -1; \
    return; \
  }

void close_storage( context_t *context ) {
  for( size_t i = 0u; i != context->channels.size(); ++i ) {
    spdk_event *event = spdk_event_allocate( i,
      []( void *context_, void* ) {
        auto context = reinterpret_cast< context_t* >( context_ );
        context->channels[ rte_lcore_id() ].reset();
        if( ++context->clean_count == context->channels.size() ) {
          spdk_event *event = spdk_event_allocate( 0,
	    []( void *context_, void* ) {
              auto context = reinterpret_cast< context_t* >( context_ );
              spdk_bs_unload( context->blobstore, []( void *context_, int bserrno ) {
                auto context = reinterpret_cast< context_t* >( context_ );
                if( bserrno ) ABORT( "blobstoreを閉じる事ができない" )
                context->buffer.reset();
                tar_benchmark::print_elapsed_time( *context->results );
                std::cout << "total\t" << context->total_elapsed_time.load() << std::endl;
                spdk_app_stop( 0 );
              }, context_ );
	    }, context_, nullptr
	  );
          spdk_event_call( event );
        }        
      }, reinterpret_cast< void* >( context ), nullptr
    );
    spdk_event_call( event );
  }
}

void create_file( void *file_, void* );

void close_file( void *file_, void* ) {
  auto file = reinterpret_cast< file_t* >( file_ );
  {
    auto count = file->context->meta_count.load();
    if( count < file->context->max_meta_concurrency )
      if( !file->context->meta_count.compare_exchange_strong( count, count + 1u ) ) {
        spdk_event *event = spdk_event_allocate( 0, close_file, file_, nullptr );
        spdk_event_call( event );
        return;
      }
  }
  spdk_blob_close( file->fd, []( void *file_, int bserrno ) {
    auto file = reinterpret_cast< file_t* >( file_ );
    if( bserrno ) ABORT( "blobを閉じる事ができない" )
    file->fd = nullptr;
    --file->context->meta_count;
    if( ++file->context->completed_count == file->context->files.size() ) {
      auto end = std::chrono::high_resolution_clock::now();
      const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->context->begin ).count();
      file->context->total_elapsed_time = elapsed;
      close_storage( file->context );
    }
    else {
      const auto next = ++file->context->head;
      if( next <= file->context->files.size() ) {
        size_t current = next - 1;
        file->context->files[ current ].lcore = rte_lcore_id();
        spdk_event *event = spdk_event_allocate( 0, create_file, &file->context->files[ current ], nullptr );
        spdk_event_call( event );
      }
    }
  }, file_ );
}

void write_file( void *file_, void* ) {
  auto file = reinterpret_cast< file_t* >( file_ );
  auto head = std::next( file->context->buffer.get(), file->context->headers[ file->index ].offset );
  const size_t lcore = rte_lcore_id();
  file->lcore = lcore;
  file->begin = std::chrono::high_resolution_clock::now();
  spdk_blob_io_write(
    file->fd, file->context->channels[ lcore ].get(), head, 0, file->page_count,
    []( void *file_, int bserrno ) {
      auto end = std::chrono::high_resolution_clock::now();
      auto file = reinterpret_cast< file_t* >( file_ );
      if( bserrno ) ABORT( "blobに書き込む事ができない" )
      tar_benchmark::record_elapsed_time(
        *file->context->results,
        file->context->headers[ file->index ].size,
        std::chrono::duration_cast< std::chrono::nanoseconds >( end - file->begin ).count()
      );
      spdk_event *event = spdk_event_allocate( 0, close_file, file_, nullptr );
      spdk_event_call( event );
    },
    file_
  );
}

void create_file( void *file_, void* ) {
  auto file = reinterpret_cast< file_t* >( file_ );
  {
    auto count = file->context->meta_count.load();
    if( count < file->context->max_meta_concurrency )
      if( !file->context->meta_count.compare_exchange_strong( count, count + 1u ) ) {
        spdk_event *event = spdk_event_allocate( 0, create_file, file_, nullptr );
        spdk_event_call( event );
        return;
      }
  }
  spdk_bs_create_blob( file->context->blobstore, []( void *file_, spdk_blob_id id, int bserrno ) {
    if( bserrno ) ABORT( "blobを作成できない" )
    auto file = reinterpret_cast< file_t* >( file_ );
    file->id = id;
    spdk_bs_open_blob( file->context->blobstore, id, []( void *file_, struct spdk_blob *fd, int bserrno ) {
      if( bserrno ) ABORT( "blobを開く事ができない" )
      auto file = reinterpret_cast< file_t* >( file_ );
      file->fd = fd;
      file->page_count = file->context->headers[ file->index ].size / file->context->page_size + ( ( file->size % file->context->page_size ) ? 1u : 0u );
      spdk_blob_resize( file->fd, file->page_count, []( void *file_, int bserrno ) {
        auto file = reinterpret_cast< file_t* >( file_ );
        --file->context->meta_count;
        if( bserrno ) ABORT( "blobをリサイズできない" )
        spdk_event *event = spdk_event_allocate( file->lcore, write_file, file_, nullptr );
        spdk_event_call( event );
      }, file_ );
    }, file_ );
  }, file_ );
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
              if( context->max_concurrency == 0 ) context->max_concurrency = context->files.size();
	      context->head = context->max_concurrency;
              context->begin = std::chrono::high_resolution_clock::now();
              for( size_t i = 0; i < context->max_concurrency; ++i ) {
                create_file( &context->files[ i ], nullptr );
              }
	    }, context_, nullptr
	  );
          spdk_event_call( event );
        }
      }, reinterpret_cast< void* >( context ), nullptr
    );
    spdk_event_call( event );
  }
}

void init_storage( void *context_, void * ) {
  auto context = reinterpret_cast< context_t* >( context_ );
  context->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !context->bdev ) ABORT( "デバイスが見つからない" );
  context->bs = spdk_bdev_create_bs_dev( context->bdev, NULL, NULL );
  if( !context->bs ) ABORT( "blobstoreデバイスを作成できない" );
  struct spdk_bs_opts opts;
  spdk_bs_opts_init( &opts );
  opts.max_channel_ops = 8000;
  spdk_bs_init( context->bs, &opts, []( void *context_, struct spdk_blob_store *blobstore, int bserrno ) {
    auto context = reinterpret_cast< context_t* >( context_ );
    if( bserrno ) ABORT( "blobstoreを初期化できない" );
    context->blobstore = blobstore;
    context->page_size = spdk_bs_get_page_size( context->blobstore );
    std::cout << "ページサイズ: " << context->page_size << std::endl;
    auto [mapped_tar,tar_size] = tar_benchmark::load_tar( context->input );
    context->buffer.reset(
      reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( tar_size, context->page_size, nullptr ) ),
      []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
    );
    memcpy( reinterpret_cast< void* >( context->buffer.get() ), reinterpret_cast< void* >( mapped_tar.get() ), tar_size );
    mapped_tar.reset();
    context->headers = tar_benchmark::parse_tar( context->buffer, tar_size );
    context->results.reset( new boost::lockfree::queue< tar_benchmark::result_t >( context->headers.size() ) );
    size_t i = 0;
    for( const auto &header: context->headers ) {
      if( header.type == tar_benchmark::filetype_t::regular && header.size > 0 )
        context->files.emplace_back( context, i, header.size, header.offset );
      ++i;
    }
    create_channels( context );
  }, context_ );
}
int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file")
    ("input,i", po::value<std::string>()->default_value( "input.tar" ), "input")
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
  context_t context( opt_var[ "input" ].as< std::string >(), 32u, opt_var[ "concurrency" ].as< size_t >() );
  spdk_app_start(&opts, init_storage, &context, nullptr );
}
