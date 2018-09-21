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
#include <spdk/nvme.h>
#include <spdk/env.h>

struct context {
  context() : ctrlr( nullptr ), ns( nullptr ) {}
  spdk_nvme_ctrlr *ctrlr;
  spdk_nvme_ns *ns;
};

struct io_queue {
  io_queue(
    const std::shared_ptr< context > &ctx_,
    const std::shared_ptr< spdk_nvme_qpair > &qpair_,
    const std::shared_ptr< uint8_t > &buffer_
  ) : ctx( ctx_ ), qpair( qpair_ ), buffer( buffer_ ) {}
  std::shared_ptr< context > ctx;
  std::shared_ptr< spdk_nvme_qpair > qpair;
  std::shared_ptr< uint8_t > buffer;
};

bool probe_cb(void *, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *) {
  std::cout << trid->traddr << " に接続します" << std::endl;
  return true;
}
void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *) {
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
  context *ctx = reinterpret_cast< context* >( cb_ctx );
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
void read_cb( void *cb_q, const struct spdk_nvme_cpl * ) {
  io_queue *q = reinterpret_cast< io_queue* >( cb_q );
  size_t count = 0;
  std::string serialized;
  namespace karma = boost::spirit::karma;
  for( auto iter = q->buffer.get(); iter != std::next( q->buffer.get(), 0x1000 ); ++iter, ++count ) {
    if( !( count % 16 ) ) serialized += "\n";
    karma::generate( std::back_inserter( serialized ), karma::right_align( 2, '0' )[ karma::hex ] << " ", *iter );
  }
  std::cout << serialized << std::endl;
}


int main( int argc, char **argv ) {
  namespace po = boost::program_options;
  po::options_description opt_desc( "options" );
  opt_desc.add_options()
    ("help,h", "This help")
    ("coremask,c", po::value<std::string>()->default_value( "0x1" ), "Hexadecimal bitmask of cores to run on");
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
  std::shared_ptr< context > ctx( new context );
  spdk_nvme_probe( nullptr, reinterpret_cast< void* >( ctx.get() ), probe_cb, attach_cb, nullptr );
  if( !ctx->ctrlr ) {
    std::cout << "NVMeデバイスは見つからなかった" << std::endl;
    return -1;
  }
  const std::shared_ptr< spdk_nvme_qpair > qpair(
    spdk_nvme_ctrlr_alloc_io_qpair( ctx->ctrlr, nullptr, 0 ),
    &spdk_nvme_ctrlr_free_io_qpair
  );
  if( !qpair ) {
    std::cout << "キューペアを作る事が出来ない" << std::endl;
    return -1;
  }
  std::shared_ptr< uint8_t > buf(
    reinterpret_cast< uint8_t* >( spdk_nvme_ctrlr_alloc_cmb_io_buffer( ctx->ctrlr, 0x1000 ) ),
    [ctx]( uint8_t *p ) { spdk_nvme_ctrlr_free_cmb_io_buffer( ctx->ctrlr, reinterpret_cast< void* >( p ), 0x1000 ); }
  );
  if( !buf ) {
    buf.reset(
      reinterpret_cast< uint8_t* >( spdk_dma_zmalloc( 0x1000, 0x1000, nullptr ) ),
      []( uint8_t *p ) { spdk_dma_free( reinterpret_cast< void* >( p ) ); }
    );
    if( !buf ) {
      std::cout << "バッファを確保できない" << std::endl;
      return -1;
    }
  }
  io_queue q( ctx, qpair, buf );
  if( spdk_nvme_ns_cmd_read( ctx->ns, qpair.get(), buf.get(), 0, 1, read_cb, reinterpret_cast< void* >( &q ), 0 ) < 0 ) {
    std::cout << "read要求をキューペアに積むことが出来ない" << std::endl;
    return -1;
  }
  if( spdk_nvme_qpair_process_completions( qpair.get(), 0 ) < 0 ) {
    std::cout << "I/Oの完了を待つことが出来ない" << std::endl;
    return -1;
  }
}
