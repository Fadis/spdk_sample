/*
 * Copyright 2018 Naomasa Matsubayashi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <atomic>
#include <chrono>
#include <memory>
#include <iostream>
#include <string>
#include <boost/spirit/include/karma.hpp>
#include <boost/program_options.hpp>
#include <spdk/nvme.h>
#include <spdk/env.h>

struct context_t {
  context_t() : ctrlr( nullptr ), ns( nullptr ) {}
  spdk_nvme_ctrlr *ctrlr;
  spdk_nvme_ns *ns;
  std::shared_ptr< spdk_nvme_qpair > qpair;
  std::shared_ptr< uint8_t > buffer;
  std::atomic< size_t > active_count;
};

struct request_t {
  request_t( context_t *c ) : context( c ) {}
  context_t *context;
};

bool probe_cb(void *, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *) {
  std::cout << trid->traddr << " に接続します" << std::endl;
  return true;
}
void attach_cb(void *ctx_, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *) {
  std::cout << trid->traddr << " に接続しました" << std::endl;
  const spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data( ctrlr );
  const spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap( ctrlr );
  std::cout << "ベンダーID:\t\t\t" << cdata->vid << std::endl;
  std::cout << "モデルナンバー:\t\t\t" << std::string( cdata->mn, std::next( cdata->mn, SPDK_NVME_CTRLR_MN_LEN ) ) << std::endl;
  std::cout << "シリアルナンバー:\t\t" << std::string( cdata->sn, std::next( cdata->sn, SPDK_NVME_CTRLR_SN_LEN ) ) << std::endl;
  std::cout << "ファームウェアバージョン:\t" << std::string( cdata->fr, std::next( cdata->fr, SPDK_NVME_CTRLR_FR_LEN ) ) << std::endl;
  if( cdata->mdts )
    std::cout << "最大データ転送サイズ:\t\t" << ( 1ull << ( 12 + cap.bits.mpsmin + cdata->mdts ) ) << std::endl;
  else
    std::cout << "最大データ転送サイズ:\t\t" << "無制限" << std::endl;
  std::cout << "NVMeバージョン:\t\t\t" << cdata->ver.bits.mjr << "." << cdata->ver.bits.mnr << "." << cdata->ver.bits.ter << std::endl;
  const int num_ns = spdk_nvme_ctrlr_get_num_ns( ctrlr );
  context_t *ctx = reinterpret_cast< context_t* >( ctx_ );
  for( int nsid = 1; nsid <= num_ns; ++nsid ) {
    spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns( ctrlr, nsid );
    if( ns && spdk_nvme_ns_is_active( ns ) ) {
      std::cout << "名前空間" << spdk_nvme_ns_get_id( ns ) << ":\t\t\t" << spdk_nvme_ns_get_size( ns ) << "バイト" << std::endl;
      if( nsid == 1 ) {
        ctx->ctrlr = ctrlr;
        ctx->ns = ns;
      }
    }
  }
}

struct unable_to_write {};

void write_cb( void *request_, const struct spdk_nvme_cpl *cpl ) {
  auto request =  reinterpret_cast< request_t* >( request_ );
  if( cpl->status.sct != 0 || cpl->status.sc != 0 ) {
    std::cout << "NVMeデバイスは書き込み要求に対して失敗を返して来た" << std::endl;
    throw unable_to_write();
  }
  --request->context->active_count;
}

int main( int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("coremask,c", po::value<std::string>()->default_value( "0x1" ), "Hexadecimal bitmask of cores to run on")
    ("block_size,b", po::value<size_t>()->default_value( 1 ), "block size")
    ("count,n", po::value<size_t>()->default_value( 1 ), "count");
  po::variables_map opt_var;
  po::store( po::parse_command_line( argc, argv, opt_desc ), opt_var );
  if ( opt_var.count("help") ) {
    std::cout << opt_desc << std::endl;
    return 0;
  }
  spdk_env_opts opts;
  spdk_env_opts_init( &opts );
  const std::string core_count = opt_var[ "coremask" ].as< std::string >();
  opts.core_mask = core_count.c_str();
  opts.name = "sequential";
  opts.shm_id = 0;
  spdk_env_init( &opts );
  context_t ctx;
  spdk_nvme_probe( nullptr, reinterpret_cast< void* >( &ctx ), probe_cb, attach_cb, nullptr );
  if( !ctx.ctrlr ) {
    std::cout << "NVMeデバイスは見つからなかった" << std::endl;
    return -1;
  }
  ctx.qpair.reset(
    spdk_nvme_ctrlr_alloc_io_qpair( ctx.ctrlr, nullptr, 0 ),
    &spdk_nvme_ctrlr_free_io_qpair
  );
  if( !ctx.qpair ) {
    std::cout << "キューペアを作る事が出来ない" << std::endl;
    return -1;
  }
  ctx.buffer.reset(
    reinterpret_cast< uint8_t* >( spdk_nvme_ctrlr_alloc_cmb_io_buffer( ctx.ctrlr, 0x1000 ) ),
    [&ctx]( uint8_t *p ) { spdk_nvme_ctrlr_free_cmb_io_buffer( ctx.ctrlr, reinterpret_cast< void* >( p ), 0x1000 ); }
  );
  size_t bs = opt_var[ "block_size" ].as< size_t >();
  size_t count = opt_var[ "count" ].as< size_t >();
  if( !ctx.buffer ) {
    ctx.buffer.reset(
      reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( 0x1000 * bs, 0x1000, nullptr ) ),
      []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
    );
    if( !ctx.buffer ) {
      std::cout << "バッファを確保できない" << std::endl;
      return -1;
    }
  }
  std::for_each( ctx.buffer.get(), std::next( ctx.buffer.get(), 0x1000 * bs ), []( auto &v ) { v = rand(); } );
  std::vector< request_t > requests( count, request_t( &ctx ) );
  auto begin = std::chrono::high_resolution_clock::now();
  size_t i = 0;
  for( auto &request: requests ) {
    while( 1 ) {
      auto active = ctx.active_count.load();
      if( active < 500u )
        if( ctx.active_count.compare_exchange_strong( active, active + 1u ) ) break;
      if( spdk_nvme_ns_cmd_flush(
        ctx.ns, ctx.qpair.get(),
        []( void *request_, const struct spdk_nvme_cpl *cpl ) {
          auto request =  reinterpret_cast< request_t* >( request_ );
          if( cpl->status.sct != 0 || cpl->status.sc != 0 ) {
            std::cout << "NVMeデバイスはflush要求に対して失敗を返して来た" << std::endl;
            throw unable_to_write();
          }
          std::cout << "flushed" << std::endl;
          request->context->active_count -= 500;
        }, nullptr
      ) < 0 ) {
        std::cout << "I/Oの完了を待つことが出来ない" << std::endl;
        return -1;
      }
    }
    if( spdk_nvme_ns_cmd_write(
      ctx.ns, ctx.qpair.get(), ctx.buffer.get(), i * bs, bs,
      []( void *, const struct spdk_nvme_cpl *cpl ) {
        if( cpl->status.sct != 0 || cpl->status.sc != 0 ) {
          std::cout << "NVMeデバイスは書き込み要求に対して失敗を返して来た" << std::endl;
          throw unable_to_write();
        }
      }, reinterpret_cast< void* >( &request ), 0
    ) < 0 ) {
      std::cout << "write要求をキューペアに積むことが出来ない " << request.context->active_count.load() << std::endl;
      return -1;
    }
    ++i;
  }
  if( spdk_nvme_ns_cmd_flush(
    ctx.ns, ctx.qpair.get(),
    []( void *request_, const struct spdk_nvme_cpl *cpl ) {
      auto request =  reinterpret_cast< request_t* >( request_ );
      if( cpl->status.sct != 0 || cpl->status.sc != 0 ) {
        std::cout << "NVMeデバイスはflush要求に対して失敗を返して来た" << std::endl;
        throw unable_to_write();
      }
      std::cout << "flushed" << std::endl;
      request->context->active_count -= 500;
    }, nullptr
  ) < 0 ) {
    std::cout << "I/Oの完了を待つことが出来ない" << std::endl;
    return -1;
  }
  while( ctx.active_count.load() != 0 );
  //while( ctx.active_count.load() != 0 ) std::cout <<  ctx.active_count.load() << std::endl;
  auto end = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( end - begin ).count();
  const auto transfered = bs * count;
  const auto rate = double( transfered ) / double( elapsed / 1000. / 1000. / 1000. );
  std::cout << ( rate / 1000. / 1000. ) << "MB/s" << std::endl;

}
