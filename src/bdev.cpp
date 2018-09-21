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
#include <spdk/log.h>

struct context {
  context( bool c, const std::string &m ) : create( c ), message( m ), bdev( nullptr ), bs( nullptr ), blobstore( nullptr ), page_size( 0 ), blob( nullptr ) {}
  bool create;
  std::string message;
  spdk_bdev *bdev;
  spdk_bs_dev *bs;
  spdk_blob_store *blobstore;
  size_t page_size;
  spdk_blob *blob;
  std::shared_ptr< uint8_t > buf;
  std::shared_ptr< spdk_io_channel > channel;
};

#define ABORT( message ) \
  { \
    std::cout << message << std::endl; \
    spdk_app_stop( -1 ); \
    return; \
  }

void start_cb( void *ctx_, void * ) {
  auto ctx = reinterpret_cast< context* >( ctx_ );
  ctx->bdev = spdk_bdev_get_by_name("Nvme0n1");
  if( !ctx->bdev ) ABORT( "デバイスが見つからない" );
  ctx->bs = spdk_bdev_create_bs_dev( ctx->bdev, NULL, NULL );
  if( !ctx->bs ) ABORT( "blobstoreデバイスを作成できない" );
  if( ctx->create )
    spdk_bs_init( ctx->bs, nullptr, []( void *ctx_, struct spdk_blob_store *blobstore, int bserrno ) {
      auto ctx = reinterpret_cast< context* >( ctx_ );
      if( bserrno ) ABORT( "blobstoreを初期化できない" );
      ctx->blobstore = blobstore;
      ctx->page_size = spdk_bs_get_page_size( ctx->blobstore );
      std::cout << "ページサイズ: " << ctx->page_size << std::endl;
      spdk_bs_create_blob( ctx->blobstore, []( void *ctx_, spdk_blob_id blobid, int bserrno ) {
        auto ctx = reinterpret_cast< context* >( ctx_ );
        if( bserrno ) ABORT( "blobstoreを作成できない" )
        std::cout << "blob_id: " << blobid << std::endl;
        spdk_bs_open_blob( ctx->blobstore, blobid, []( void *ctx_, struct spdk_blob *blob, int bserrno ) {
          auto ctx = reinterpret_cast< context* >( ctx_ );
          if( bserrno ) ABORT( "blobを開く事ができない" )
          ctx->blob = blob;
          spdk_blob_resize( ctx->blob, 1, []( void *ctx_, int bserrno ) {
            auto ctx = reinterpret_cast< context* >( ctx_ );
            if( bserrno ) ABORT( "blobをリサイズできない" )
            spdk_blob_sync_md( ctx->blob, []( void *ctx_, int bserrno ) {
              auto ctx = reinterpret_cast< context* >( ctx_ );
              if( bserrno ) ABORT( "blobのメタデータを同期できない" )
              ctx->buf.reset(
                reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( ctx->page_size, 0x1000, nullptr ) ),
                []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
              );
              if( !ctx->buf ) ABORT( "バッファを確保できない" )
              std::copy( ctx->message.begin(), ctx->message.end(), ctx->buf.get() );
              ctx->channel.reset(
                spdk_bs_alloc_io_channel( ctx->blobstore ),
                []( spdk_io_channel *p ) { spdk_bs_free_io_channel( p ); }
              );
              spdk_blob_io_write(
                ctx->blob, ctx->channel.get(), ctx->buf.get(), 0, 1,
                []( void *ctx_, int bserrno ) {
                  auto ctx = reinterpret_cast< context* >( ctx_ );
                  if( bserrno ) ABORT( "blobに書き込む事ができない" )
                  std::fill( ctx->buf.get(), std::next( ctx->buf.get(), ctx->page_size ), 0 );
                  spdk_blob_io_read(
                    ctx->blob, ctx->channel.get(), ctx->buf.get(), 0, 1,
                    []( void *ctx_, int bserrno ) {
                      auto ctx = reinterpret_cast< context* >( ctx_ );
                      if( bserrno ) ABORT( "blobを読み出す事ができない" )
                      std::cout << ctx->buf.get() << std::endl;
                      spdk_blob_close( ctx->blob, []( void *ctx_, int bserrno ) {
                        auto ctx = reinterpret_cast< context* >( ctx_ );
                        if( bserrno ) ABORT( "blobを閉じる事ができない" )
                        ctx->blob = nullptr;
                        ctx->buf.reset();
                        ctx->channel.reset();
                        spdk_bs_unload( ctx->blobstore, []( void *ctx_, int bserrno ) {
                          auto ctx = reinterpret_cast< context* >( ctx_ );
                          if( bserrno ) ABORT( "blobstoreを閉じる事ができない" )
                          ctx->blobstore = nullptr;
                          spdk_app_stop( 0 );
                        }, ctx_ );
                      }, ctx_ );
                    },
                    ctx_
                  );
                },
                ctx_
              );
            }, ctx_ );
          }, ctx_ );
        }, ctx_ );
      }, ctx_ );
    }, ctx_ );
  else
    spdk_bs_load( ctx->bs, nullptr, []( void *ctx_, struct spdk_blob_store *blobstore, int bserrno ) {
      auto ctx = reinterpret_cast< context* >( ctx_ );
      ctx->blobstore = blobstore;
      if( bserrno ) ABORT( "blobstoreをロードできない" )
      ctx->page_size = spdk_bs_get_page_size( ctx->blobstore );
      std::cout << "ページサイズ: " << ctx->page_size << std::endl;
      spdk_bs_iter_first( ctx->blobstore, []( void *ctx_, struct spdk_blob *blob, int bserrno ) {
        auto ctx = reinterpret_cast< context* >( ctx_ );
        if( bserrno ) ABORT( "blobstoreをロードできない" )
        ctx->blob = blob;
        ctx->buf.reset(
          reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( ctx->page_size, 0x1000, nullptr ) ),
          []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
        );
        if( !ctx->buf ) ABORT( "バッファを確保できない" )
        ctx->channel.reset(
          spdk_bs_alloc_io_channel( ctx->blobstore ),
          []( spdk_io_channel *p ) { spdk_bs_free_io_channel( p ); }
        );
        spdk_blob_io_read(
          ctx->blob, ctx->channel.get(), ctx->buf.get(), 0, 1,
          []( void *ctx_, int bserrno ) {
            auto ctx = reinterpret_cast< context* >( ctx_ );
            if( bserrno ) ABORT( "blobを読み出す事ができない" )
            std::cout << ctx->buf.get() << std::endl;
            spdk_blob_close( ctx->blob, []( void *ctx_, int bserrno ) {
              auto ctx = reinterpret_cast< context* >( ctx_ );
              if( bserrno ) ABORT( "blobを閉じる事ができない" )
              ctx->blob = nullptr;
              ctx->buf.reset();
              ctx->channel.reset();
              spdk_bs_unload( ctx->blobstore, []( void *ctx_, int bserrno ) {
                auto ctx = reinterpret_cast< context* >( ctx_ );
                if( bserrno ) ABORT( "blobstoreを閉じる事ができない" )
                ctx->blobstore = nullptr;
                spdk_app_stop( 0 );
              }, ctx_ );
            }, ctx_ );
          },
          ctx_
        );
      }, ctx_ );
    }, ctx_ );
}
int main(int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("config,c", po::value<std::string>()->default_value( "spdk.conf" ), "Config file")
    ("create,n", po::value<bool>()->default_value( true ), "Crete new blobstore")
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
  opts.name = "bdev";
  const std::string config_file = opt_var[ "config" ].as< std::string >();
  opts.config_file = config_file.c_str();
  context ctx( opt_var[ "create" ].as< bool >(), opt_var[ "message" ].as< std::string >() );
  spdk_app_start(&opts, start_cb, &ctx, nullptr );
}
