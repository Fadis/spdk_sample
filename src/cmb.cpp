/*
 * Copyright 2018 Naomasa Matsubayashi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <memory>
#include <iostream>
#include <string>
#include <spdk/nvme.h>
#include <spdk/env.h>

struct context {
  context() : ctrlr( nullptr ), ns( nullptr ) {}
  spdk_nvme_ctrlr *ctrlr;
  spdk_nvme_ns *ns;
};

bool probe_cb(void *, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *) {
  std::cout << trid->traddr << " に接続します" << std::endl;
  return true;
}
void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *) {
  std::cout << trid->traddr << " に接続しました" << std::endl;
  const int num_ns = spdk_nvme_ctrlr_get_num_ns( ctrlr );
  context *ctx = reinterpret_cast< context* >( cb_ctx );
  for( int nsid = 1; nsid <= num_ns; ++nsid ) {
    spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns( ctrlr, nsid );
    if( ns && spdk_nvme_ns_is_active( ns ) ) {
      if( nsid == 1 ) {
        ctx->ctrlr = ctrlr;
        ctx->ns = ns;
      }
    }
  }
}
int main() {
  spdk_env_opts opts;
  spdk_env_opts_init( &opts );
  opts.core_mask = "0x1";
  opts.name = "cmb";
  opts.shm_id = 0;
  spdk_env_init( &opts );
  std::shared_ptr< context > ctx( new context );
  spdk_nvme_probe( nullptr, reinterpret_cast< void* >( ctx.get() ), probe_cb, attach_cb, nullptr );
  if( !ctx->ctrlr ) {
    std::cout << "NVMeデバイスは見つからなかった" << std::endl;
    return -1;
  }
  std::shared_ptr< uint8_t > buf(
    reinterpret_cast< uint8_t* >( spdk_nvme_ctrlr_alloc_cmb_io_buffer( ctx->ctrlr, 0x1000 ) ),
    [ctx]( uint8_t *p ) { spdk_nvme_ctrlr_free_cmb_io_buffer( ctx->ctrlr, reinterpret_cast< void* >( p ), 0x1000 ); }
  );
  if( !buf ) std::cout << "CMBを確保できない" << std::endl;
  else std::cout << "CMBを確保できた" << std::endl;
}
