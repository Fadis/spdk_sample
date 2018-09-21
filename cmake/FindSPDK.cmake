find_package(DPDK)
find_package(LibAIO)
set( SPDK_ROOT_DIR "" CACHE PATH "SPDK root directory" )
set( DETECTED_SPDK_ROOT_DIR "" )
if( NOT "${SPDK_ROOT_DIR}" STREQUAL "" )
  find_path( SPDK_INCLUDE_DIR spdk/env.h  PATHS ${SPDK_ROOT_DIR}/include )
  if( NOT "${SPDK_INCLUDE_DIR}" STREQUAL "SPDK_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_SPDK_ROOT_DIR ${SPDK_ROOT_DIR} )
  endif()
endif()
if( "${DETECTED_SPDK_ROOT_DIR}" STREQUAL "" )
  find_path( SPDK_INCLUDE_DIR spdk/env.h  PATHS /usr/include )
  if( NOT "${SPDK_INCLUDE_DIR}" STREQUAL "SPDK_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_SPDK_ROOT_DIR /usr )
  endif()
endif()
if( "${DETECTED_SPDK_ROOT_DIR}" STREQUAL "" )
  find_path( SPDK_INCLUDE_DIR spdk/env.h  PATHS /usr/local/include )
  if( NOT "${SPDK_INCLUDE_DIR}" STREQUAL "SPDK_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_SPDK_ROOT_DIR /usr/local )
  endif()
endif()
if( NOT "${DETECTED_SPDK_ROOT_DIR}" STREQUAL "" )
  find_library(SPDK_app_rpc_LIBRARY spdk_app_rpc HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_LIBRARY spdk_bdev HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_aio_LIBRARY spdk_bdev_aio HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_malloc_LIBRARY spdk_bdev_malloc HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_null_LIBRARY spdk_bdev_null HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_nvme_LIBRARY spdk_bdev_nvme HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_rpc_LIBRARY spdk_bdev_rpc HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_bdev_virtio_LIBRARY spdk_bdev_virtio HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_blob_LIBRARY spdk_blob HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_blob_bdev_LIBRARY spdk_blob_bdev HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_blobfs_LIBRARY spdk_blobfs HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_conf_LIBRARY spdk_conf HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_copy_LIBRARY spdk_copy HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_copy_ioat_LIBRARY spdk_copy_ioat HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  #find_library(SPDK_cunit_LIBRARY spdk_cunit HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_env_dpdk_LIBRARY spdk_env_dpdk HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_LIBRARY spdk_event HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_bdev_LIBRARY spdk_event_bdev HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_copy_LIBRARY spdk_event_copy HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_iscsi_LIBRARY spdk_event_iscsi HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_nbd_LIBRARY spdk_event_nbd HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_net_LIBRARY spdk_event_net HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_nvmf_LIBRARY spdk_event_nvmf HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_scsi_LIBRARY spdk_event_scsi HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_event_vhost_LIBRARY spdk_event_vhost HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_ioat_LIBRARY spdk_ioat HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_iscsi_LIBRARY spdk_iscsi HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_json_LIBRARY spdk_json HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_jsonrpc_LIBRARY spdk_jsonrpc HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_log_LIBRARY spdk_log HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_log_rpc_LIBRARY spdk_log_rpc HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_lvol_LIBRARY spdk_lvol HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_nbd_LIBRARY spdk_nbd HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_net_LIBRARY spdk_net HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  #  find_library(SPDK_net_posix_LIBRARY spdk_net_posix HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_nvme_LIBRARY spdk_nvme HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_nvmf_LIBRARY spdk_nvmf HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_rpc_LIBRARY spdk_rpc HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_rte_vhost_LIBRARY spdk_rte_vhost HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_scsi_LIBRARY spdk_scsi HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_spdk_mock_LIBRARY spdk_spdk_mock HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_trace_LIBRARY spdk_trace HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_util_LIBRARY spdk_util HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_vbdev_error_LIBRARY spdk_vbdev_error HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_vbdev_gpt_LIBRARY spdk_vbdev_gpt HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_vbdev_lvol_LIBRARY spdk_vbdev_lvol HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_vbdev_passthru_LIBRARY spdk_vbdev_passthru HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_vbdev_split_LIBRARY spdk_vbdev_split HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_vhost_LIBRARY spdk_vhost HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_virtio_LIBRARY spdk_virtio HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
  find_library(SPDK_thread_LIBRARY spdk_thread HINTS ${DETECTED_SPDK_ROOT_DIR}/lib )
endif()
find_package_handle_standard_args(spdk DEFAULT_MSG
  SPDK_INCLUDE_DIR
  SPDK_app_rpc_LIBRARY
  SPDK_bdev_LIBRARY
  SPDK_bdev_aio_LIBRARY
  SPDK_bdev_malloc_LIBRARY
  SPDK_bdev_null_LIBRARY
  SPDK_bdev_nvme_LIBRARY
  SPDK_bdev_rpc_LIBRARY
  SPDK_bdev_virtio_LIBRARY
  SPDK_blob_LIBRARY
  SPDK_blob_bdev_LIBRARY
  SPDK_blobfs_LIBRARY
  SPDK_conf_LIBRARY
  SPDK_copy_LIBRARY
  SPDK_copy_ioat_LIBRARY
  #SPDK_cunit_LIBRARY
  SPDK_env_dpdk_LIBRARY
  SPDK_event_LIBRARY
  SPDK_event_bdev_LIBRARY
  SPDK_event_copy_LIBRARY
  SPDK_event_iscsi_LIBRARY
  SPDK_event_nbd_LIBRARY
  SPDK_event_net_LIBRARY
  SPDK_event_nvmf_LIBRARY
  SPDK_event_scsi_LIBRARY
  SPDK_event_vhost_LIBRARY
  SPDK_ioat_LIBRARY
  SPDK_iscsi_LIBRARY
  SPDK_json_LIBRARY
  SPDK_jsonrpc_LIBRARY
  SPDK_log_LIBRARY
  SPDK_log_rpc_LIBRARY
  SPDK_lvol_LIBRARY
  SPDK_nbd_LIBRARY
  SPDK_net_LIBRARY
  #  SPDK_net_posix_LIBRARY
  SPDK_nvme_LIBRARY
  SPDK_nvmf_LIBRARY
  SPDK_rpc_LIBRARY
  SPDK_rte_vhost_LIBRARY
  SPDK_scsi_LIBRARY
  SPDK_spdk_mock_LIBRARY
  SPDK_trace_LIBRARY
  SPDK_util_LIBRARY
  SPDK_vbdev_error_LIBRARY
  SPDK_vbdev_gpt_LIBRARY
  SPDK_vbdev_lvol_LIBRARY
  SPDK_vbdev_passthru_LIBRARY
  SPDK_vbdev_split_LIBRARY
  SPDK_vhost_LIBRARY
  SPDK_virtio_LIBRARY
  SPDK_thread_LIBRARY
  DPDK_ring_LIBRARY
  DPDK_mempool_LIBRARY
  DPDK_pci_LIBRARY
  DPDK_eal_LIBRARY
  LIBUUID_LIBRARY
)

if(spdk_FOUND)
set(
  SPDK_LIBRARIES
  -Wl,--whole-archive
  spdk_copy_ioat
  spdk_ioat
  spdk_vbdev_lvol
  spdk_lvol
  spdk_bdev_malloc
  spdk_bdev_null
  spdk_bdev_nvme
  spdk_nvme
  spdk_vbdev_passthru
  spdk_vbdev_error
  spdk_vbdev_gpt
  spdk_vbdev_split
  spdk_bdev_aio
  spdk_bdev_virtio
  spdk_virtio
  -Wl,--no-whole-archive
  ${LIBAIO_LIBRARIES}
  -Wl,--whole-archive
  spdk_event_bdev
  spdk_event_copy
  spdk_blobfs
  spdk_blob
  spdk_bdev
  spdk_blob_bdev
  spdk_copy
  spdk_event
  spdk_util
  spdk_conf
  spdk_trace
  spdk_log
  spdk_jsonrpc
  spdk_json
  spdk_rpc
  spdk_env_dpdk
  spdk_thread
  #spdk_bdev_rpc
  #spdk_cunit
  #spdk_event_iscsi
  #spdk_event_nbd
  #spdk_event_net
  #spdk_event_nvmf
  #spdk_event_scsi
  #spdk_event_vhost
  #spdk_iscsi
  #spdk_log_rpc
  #spdk_nbd
  #spdk_net
  #spdk_net_posix
  #spdk_nvmf
  #spdk_rte_vhost
  #spdk_scsi
  #spdk_spdk_mock
  #spdk_vhost
  #spdk_app_rpc
  rte_eal
  rte_mempool
  rte_ring
  rte_mempool_ring
  rte_bus_pci
  rte_pci
  #${DPDK_LIBRARIES}
  -Wl,--no-whole-archive
  ${CMAKE_DL_LIBS}
  ${LIBUUID_LIBRARIES}
  ${LIBNUMA_LIBRARIES}
  Threads::Threads
  rt
)
set(SPDK_LIBRARY_DIRS ${DETECTED_SPDK_ROOT_DIR}/lib)
endif(spdk_FOUND)
mark_as_advanced(
  SPDK_INCLUDE_DIR
  SPDK_LIBRARIES
  SPDK_LIBRARY_DIRS
  SPDK_app_rpc_LIBRARY
  SPDK_bdev_LIBRARY
  SPDK_bdev_aio_LIBRARY
  SPDK_bdev_malloc_LIBRARY
  SPDK_bdev_null_LIBRARY
  SPDK_bdev_nvme_LIBRARY
  SPDK_bdev_rpc_LIBRARY
  SPDK_bdev_virtio_LIBRARY
  SPDK_blob_LIBRARY
  SPDK_blob_bdev_LIBRARY
  SPDK_blobfs_LIBRARY
  SPDK_conf_LIBRARY
  SPDK_copy_LIBRARY
  SPDK_copy_ioat_LIBRARY
  #SPDK_cunit_LIBRARY
  SPDK_env_dpdk_LIBRARY
  SPDK_event_LIBRARY
  SPDK_event_bdev_LIBRARY
  SPDK_event_copy_LIBRARY
  SPDK_event_iscsi_LIBRARY
  SPDK_event_nbd_LIBRARY
  SPDK_event_net_LIBRARY
  SPDK_event_nvmf_LIBRARY
  SPDK_event_scsi_LIBRARY
  SPDK_event_vhost_LIBRARY
  SPDK_ioat_LIBRARY
  SPDK_iscsi_LIBRARY
  SPDK_json_LIBRARY
  SPDK_jsonrpc_LIBRARY
  SPDK_log_LIBRARY
  SPDK_log_rpc_LIBRARY
  SPDK_lvol_LIBRARY
  SPDK_nbd_LIBRARY
  SPDK_net_LIBRARY
  #  SPDK_net_posix_LIBRARY
  SPDK_nvme_LIBRARY
  SPDK_nvmf_LIBRARY
  SPDK_rpc_LIBRARY
  SPDK_rte_vhost_LIBRARY
  SPDK_scsi_LIBRARY
  SPDK_spdk_mock_LIBRARY
  SPDK_trace_LIBRARY
  SPDK_util_LIBRARY
  SPDK_vbdev_error_LIBRARY
  SPDK_vbdev_gpt_LIBRARY
  SPDK_vbdev_lvol_LIBRARY
  SPDK_vbdev_passthru_LIBRARY
  SPDK_vbdev_split_LIBRARY
  SPDK_vhost_LIBRARY
  SPDK_virtio_LIBRARY
  SPDK_thread_LIBRARY
)

