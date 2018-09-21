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
#include <boost/spirit/include/karma.hpp>
#include <boost/program_options.hpp>
#include <spdk/event.h>
#include <spdk/bdev.h>
#include <spdk/env.h>
#include <spdk/blob_bdev.h>
#include <spdk/blob.h>
#include <spdk/blobfs.h>
#include <spdk/log.h>

struct context {
  context( bool c, bool r, const std::string &p, const std::string &m ) : create( c ), readonly( r ), path( p ), message( m ), bdev( nullptr ), bs( nullptr ), fs( nullptr ), file( nullptr ) {}
  bool create;
  bool readonly;
  std::string path;
  std::string message;
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_filesystem *fs;
  spdk_file *file;
  std::shared_ptr< uint8_t > buf;
  std::shared_ptr< spdk_io_channel > channel;
};

#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    return; \
  }

void read_cb( void *ctx_, void* ) {
  auto ctx = reinterpret_cast< context* >( ctx_ );
  if( spdk_fs_open_file( ctx->fs, ctx->channel.get(), ctx->path.c_str(), 0, &ctx->file ) < 0 )
    ABORT( "ファイルを開く事ができない" );
  const size_t filesize = spdk_file_get_length( ctx->file );
  ctx->buf.reset(
    reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( filesize, 0x1000, nullptr ) ),
    []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
  );
  if( spdk_file_read( ctx->file, ctx->channel.get(), ctx->buf.get(), 0, filesize ) < 0 )
    ABORT( "ファイルを読み出す事ができない" );
  if( spdk_file_sync( ctx->file, ctx->channel.get() ) < 0 )
    ABORT( "ファイルキャッシュを同期する事ができない" );
  std::cout << "ファイル " << ctx->path << " から" << ctx->buf.get() << " を読みだした" << std::endl;
  if( spdk_file_close( ctx->file, ctx->channel.get() ) < 0 )
    ABORT( "ファイルを閉じる事ができない" );
  ctx->file = nullptr;
  ctx->buf.reset();
  ctx->channel.reset();
  spdk_fs_unload( ctx->fs, []( void *ctx_, int fserrno ) {
    if( fserrno ) ABORT( "ファイルシステムを閉じる事ができない" ) 
    auto ctx = reinterpret_cast< context* >( ctx_ );
    ctx->fs = nullptr;
    spdk_app_stop( 0 );
  }, ctx_ );
}

void write_cb( void *ctx_, void* ) {
  auto ctx = reinterpret_cast< context* >( ctx_ );
  if( !ctx->message.empty() ) {
    if( spdk_fs_create_file( ctx->fs, ctx->channel.get(), ctx->path.c_str() ) < 0 )
      ABORT( "ファイルを作成する事ができない" );
    if( spdk_fs_open_file( ctx->fs, ctx->channel.get(), ctx->path.c_str(), 0, &ctx->file ) < 0 )
      ABORT( "ファイルを開く事ができない" );
    const size_t filesize = ctx->message.size() + 1;
    ctx->buf.reset(
      reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( filesize, 0x1000, nullptr ) ),
      []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
    );
    std::copy( ctx->message.begin(), ctx->message.end(), ctx->buf.get() );
    spdk_file_truncate( ctx->file, ctx->channel.get(), filesize );
    if( spdk_file_write( ctx->file, ctx->channel.get(), ctx->buf.get(), 0, filesize ) < 0 )
      ABORT( "ファイルを書き込む事ができない" );
    ctx->buf.reset(
      reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( filesize, 0x1000, nullptr ) ),
      []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
    );
    if( spdk_file_read( ctx->file, ctx->channel.get(), ctx->buf.get(), 0, filesize ) < 0 )
      ABORT( "ファイルを読み出す事ができない" );
    if( spdk_file_sync( ctx->file, ctx->channel.get() ) < 0 )
      ABORT( "ファイルキャッシュを同期する事ができない" );
    std::cout << "ファイル " << ctx->path << " に " << ctx->buf.get() << " を書き込んだ" << std::endl;
    if( spdk_file_close( ctx->file, ctx->channel.get() ) < 0 )
      ABORT( "ファイルを閉じる事ができない" );
    ctx->file = nullptr;
  }
  else {
    if( spdk_fs_delete_file( ctx->fs, ctx->channel.get(), ctx->path.c_str() ) < 0 )
      ABORT( "ファイルを削除する事ができない" );
  }
  ctx->buf.reset();
  ctx->channel.reset();
  spdk_fs_unload( ctx->fs, []( void *ctx_, int fserrno ) {
    if( fserrno ) ABORT( "ファイルシステムを閉じる事ができない" ) 
    auto ctx = reinterpret_cast< context* >( ctx_ );
    ctx->fs = nullptr;
    spdk_app_stop( 0 );
  }, ctx_ );
}

void init_cb( void *ctx_, void* ) {
  auto ctx = reinterpret_cast< context* >( ctx_ );
  ctx->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !ctx->bdev ) ABORT( "デバイスが見つからない" );
  ctx->bs = spdk_bdev_create_bs_dev( ctx->bdev, NULL, NULL );
  if( !ctx->bs ) ABORT( "blobstoreデバイスを作成できない" );
  spdk_fs_set_cache_size( 512 );
  if( ctx->create )
    spdk_fs_init( ctx->bs, nullptr,
      []( fs_request_fn f, void *arg ) {
        spdk_event *event = spdk_event_allocate( 0, []( void *arg1, void *arg2 ) {
	  reinterpret_cast< fs_request_fn >( arg1 )( arg2 );
	}, (void *)f, arg );
	spdk_event_call( event );
      },
      []( void *ctx_, struct spdk_filesystem *fs, int fserrno ) {
        auto ctx = reinterpret_cast< context* >( ctx_ );
        if( fserrno ) ABORT( "ファイルシステムを作成できない" ) 
        ctx->fs = fs;
        ctx->channel.reset(
          spdk_fs_alloc_io_channel( ctx->fs ),
          []( spdk_io_channel *p ) { spdk_fs_free_io_channel( p ); }
        );
        spdk_event *event = spdk_event_allocate( 1, ctx->readonly ? read_cb : write_cb, ctx_, nullptr );
        spdk_event_call (event );
      }, ctx_
    );
  else
    spdk_fs_load( ctx->bs,
      []( fs_request_fn f, void *arg ) {
        spdk_event *event = spdk_event_allocate( 0, []( void *arg1, void *arg2 ) {
	  reinterpret_cast< fs_request_fn >( arg1 )( arg2 );
	}, (void *)f, arg );
	spdk_event_call( event );
      },
      []( void *ctx_, struct spdk_filesystem *fs, int fserrno ) {
        auto ctx = reinterpret_cast< context* >( ctx_ );
        if( fserrno ) ABORT( "ファイルシステムを読み込む事ができない" ) 
        ctx->fs = fs;
        ctx->channel.reset(
          spdk_fs_alloc_io_channel( ctx->fs ),
          []( spdk_io_channel *p ) { spdk_fs_free_io_channel( p ); }
        );
        spdk_event *event = spdk_event_allocate( 1, ctx->readonly ? read_cb : write_cb, ctx_, nullptr );
        spdk_event_call (event );
      }, ctx_
    );
}

int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file")
    ("create,n", po::value<bool>()->default_value( true ), "Crete new blobstore")
    ("path,p", po::value<std::string>()->default_value( "hoge.txt" ), "filename")
    ("read,r", po::value<bool>()->default_value( false ), "read only")
    ("message,m", po::value<std::string>()->default_value( "Hello, World!" ), "message");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  struct spdk_app_opts opts = {};
  SPDK_NOTICELOG("entry\n");
  spdk_app_opts_init(&opts);
  opts.name = "blobfs";
  const std::string config_file = opt_var[ "config" ].as< std::string >();
  opts.config_file = config_file.c_str();
  context ctx( opt_var[ "create" ].as< bool >(), opt_var[ "read" ].as< bool >(), opt_var[ "path" ].as< std::string >(), opt_var[ "message" ].as< std::string >() );
  spdk_app_start(&opts, init_cb, &ctx, nullptr );
}
