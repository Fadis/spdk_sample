find_package(LibUUID)
find_package(LibNUMA)
set( DPDK_ROOT_DIR "" CACHE PATH "DPDK root directory" )
set( DETECTED_DPDK_ROOT_DIR "" )
if( NOT "${DPDK_ROOT_DIR}" STREQUAL "" )
  find_path( DPDK_INCLUDE_DIR dpdk/rte_eal.h  PATHS ${DPDK_ROOT_DIR}/include )
  if( NOT "${DPDK_INCLUDE_DIR}" STREQUAL "DPDK_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_DPDK_ROOT_DIR ${DPDK_ROOT_DIR} )
  endif()
endif()
if( "${DETECTED_DPDK_ROOT_DIR}" STREQUAL "" )
  find_path( DPDK_INCLUDE_DIR dpdk/rte_eal.h  PATHS /usr/include )
  if( NOT "${DPDK_INCLUDE_DIR}" STREQUAL "DPDK_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_DPDK_ROOT_DIR /usr )
  endif()
endif()
if( "${DETECTED_DPDK_ROOT_DIR}" STREQUAL "" )
  find_path( DPDK_INCLUDE_DIR dpdk/rte_eal.h  PATHS /usr/local/include )
  if( NOT "${DPDK_INCLUDE_DIR}" STREQUAL "DPDK_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_DPDK_ROOT_DIR /usr/local )
  endif()
endif()
if( NOT "${DETECTED_DPDK_ROOT_DIR}" STREQUAL "" )
  find_library(DPDK_dpdk_LIBRARY dpdk HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_acl_LIBRARY rte_acl HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bbdev_LIBRARY rte_bbdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bitratestats_LIBRARY rte_bitratestats HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bpf_LIBRARY rte_bpf HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bus_dpaa_LIBRARY rte_bus_dpaa HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bus_fslmc_LIBRARY rte_bus_fslmc HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bus_ifpga_LIBRARY rte_bus_ifpga HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bus_pci_LIBRARY rte_bus_pci HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_bus_vdev_LIBRARY rte_bus_vdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_cfgfile_LIBRARY rte_cfgfile HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_cmdline_LIBRARY rte_cmdline HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_common_octeontx_LIBRARY rte_common_octeontx HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_compressdev_LIBRARY rte_compressdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_cryptodev_LIBRARY rte_cryptodev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_distributor_LIBRARY rte_distributor HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_eal_LIBRARY rte_eal HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_efd_LIBRARY rte_efd HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_ethdev_LIBRARY rte_ethdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_eventdev_LIBRARY rte_eventdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_flow_classify_LIBRARY rte_flow_classify HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_gro_LIBRARY rte_gro HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_gso_LIBRARY rte_gso HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_hash_LIBRARY rte_hash HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_ifcvf_vdpa_LIBRARY rte_ifcvf_vdpa HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_ip_frag_LIBRARY rte_ip_frag HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_jobstats_LIBRARY rte_jobstats HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_kni_LIBRARY rte_kni HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_kvargs_LIBRARY rte_kvargs HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_latencystats_LIBRARY rte_latencystats HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_lpm_LIBRARY rte_lpm HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mbuf_LIBRARY rte_mbuf HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_member_LIBRARY rte_member HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_LIBRARY rte_mempool HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_bucket_LIBRARY rte_mempool_bucket HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_dpaa_LIBRARY rte_mempool_dpaa HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_dpaa2_LIBRARY rte_mempool_dpaa2 HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_octeontx_LIBRARY rte_mempool_octeontx HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_ring_LIBRARY rte_mempool_ring HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_mempool_stack_LIBRARY rte_mempool_stack HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_meter_LIBRARY rte_meter HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_metrics_LIBRARY rte_metrics HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_net_LIBRARY rte_net HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pci_LIBRARY rte_pci HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pdump_LIBRARY rte_pdump HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pipeline_LIBRARY rte_pipeline HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_af_packet_LIBRARY rte_pmd_af_packet HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_ark_LIBRARY rte_pmd_ark HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_avf_LIBRARY rte_pmd_avf HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_avp_LIBRARY rte_pmd_avp HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_axgbe_LIBRARY rte_pmd_axgbe HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_bbdev_null_LIBRARY rte_pmd_bbdev_null HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_bnxt_LIBRARY rte_pmd_bnxt HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_bond_LIBRARY rte_pmd_bond HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_crypto_scheduler_LIBRARY rte_pmd_crypto_scheduler HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_cxgbe_LIBRARY rte_pmd_cxgbe HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa_LIBRARY rte_pmd_dpaa HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa2_LIBRARY rte_pmd_dpaa2 HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa2_cmdif_LIBRARY rte_pmd_dpaa2_cmdif HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa2_event_LIBRARY rte_pmd_dpaa2_event HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa2_qdma_LIBRARY rte_pmd_dpaa2_qdma HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa2_sec_LIBRARY rte_pmd_dpaa2_sec HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa_event_LIBRARY rte_pmd_dpaa_event HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_dpaa_sec_LIBRARY rte_pmd_dpaa_sec HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_e1000_LIBRARY rte_pmd_e1000 HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_ena_LIBRARY rte_pmd_ena HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_enic_LIBRARY rte_pmd_enic HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_failsafe_LIBRARY rte_pmd_failsafe HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_fm10k_LIBRARY rte_pmd_fm10k HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_i40e_LIBRARY rte_pmd_i40e HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_ifpga_rawdev_LIBRARY rte_pmd_ifpga_rawdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_ixgbe_LIBRARY rte_pmd_ixgbe HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_kni_LIBRARY rte_pmd_kni HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_lio_LIBRARY rte_pmd_lio HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_nfp_LIBRARY rte_pmd_nfp HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_null_LIBRARY rte_pmd_null HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_null_crypto_LIBRARY rte_pmd_null_crypto HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_octeontx_LIBRARY rte_pmd_octeontx HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_octeontx_ssovf_LIBRARY rte_pmd_octeontx_ssovf HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_opdl_event_LIBRARY rte_pmd_opdl_event HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_qede_LIBRARY rte_pmd_qede HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_ring_LIBRARY rte_pmd_ring HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_sfc_efx_LIBRARY rte_pmd_sfc_efx HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_skeleton_event_LIBRARY rte_pmd_skeleton_event HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_skeleton_rawdev_LIBRARY rte_pmd_skeleton_rawdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_softnic_LIBRARY rte_pmd_softnic HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_sw_event_LIBRARY rte_pmd_sw_event HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_tap_LIBRARY rte_pmd_tap HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_thunderx_nicvf_LIBRARY rte_pmd_thunderx_nicvf HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_vdev_netvsc_LIBRARY rte_pmd_vdev_netvsc HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_vhost_LIBRARY rte_pmd_vhost HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_virtio_LIBRARY rte_pmd_virtio HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_virtio_crypto_LIBRARY rte_pmd_virtio_crypto HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_pmd_vmxnet3_uio_LIBRARY rte_pmd_vmxnet3_uio HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_port_LIBRARY rte_port HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_power_LIBRARY rte_power HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_rawdev_LIBRARY rte_rawdev HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_reorder_LIBRARY rte_reorder HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_ring_LIBRARY rte_ring HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_sched_LIBRARY rte_sched HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_security_LIBRARY rte_security HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_table_LIBRARY rte_table HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_timer_LIBRARY rte_timer HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
  find_library(DPDK_vhost_LIBRARY rte_vhost HINTS ${DETECTED_DPDK_ROOT_DIR}/lib )
endif()
find_package_handle_standard_args(dpdk DEFAULT_MSG
  DPDK_INCLUDE_DIR
  DPDK_dpdk_LIBRARY
  DPDK_acl_LIBRARY
  DPDK_bbdev_LIBRARY
  DPDK_bitratestats_LIBRARY
  DPDK_bpf_LIBRARY
  DPDK_bus_dpaa_LIBRARY
  DPDK_bus_fslmc_LIBRARY
  DPDK_bus_ifpga_LIBRARY
  DPDK_bus_pci_LIBRARY
  DPDK_bus_vdev_LIBRARY
  DPDK_cfgfile_LIBRARY
  DPDK_cmdline_LIBRARY
  DPDK_common_octeontx_LIBRARY
  DPDK_compressdev_LIBRARY
  DPDK_cryptodev_LIBRARY
  DPDK_distributor_LIBRARY
  DPDK_eal_LIBRARY
  DPDK_efd_LIBRARY
  DPDK_ethdev_LIBRARY
  DPDK_eventdev_LIBRARY
  DPDK_flow_classify_LIBRARY
  DPDK_gro_LIBRARY
  DPDK_gso_LIBRARY
  DPDK_hash_LIBRARY
  DPDK_ifcvf_vdpa_LIBRARY
  DPDK_ip_frag_LIBRARY
  DPDK_jobstats_LIBRARY
  DPDK_kni_LIBRARY
  DPDK_kvargs_LIBRARY
  DPDK_latencystats_LIBRARY
  DPDK_lpm_LIBRARY
  DPDK_mbuf_LIBRARY
  DPDK_member_LIBRARY
  DPDK_mempool_LIBRARY
  DPDK_mempool_bucket_LIBRARY
  DPDK_mempool_dpaa_LIBRARY
  DPDK_mempool_dpaa2_LIBRARY
  DPDK_mempool_octeontx_LIBRARY
  DPDK_mempool_ring_LIBRARY
  DPDK_mempool_stack_LIBRARY
  DPDK_meter_LIBRARY
  DPDK_metrics_LIBRARY
  DPDK_net_LIBRARY
  DPDK_pci_LIBRARY
  DPDK_pdump_LIBRARY
  DPDK_pipeline_LIBRARY
  DPDK_pmd_af_packet_LIBRARY
  DPDK_pmd_ark_LIBRARY
  DPDK_pmd_avf_LIBRARY
  DPDK_pmd_avp_LIBRARY
  DPDK_pmd_axgbe_LIBRARY
  DPDK_pmd_bbdev_null_LIBRARY
  DPDK_pmd_bnxt_LIBRARY
  DPDK_pmd_bond_LIBRARY
  DPDK_pmd_crypto_scheduler_LIBRARY
  DPDK_pmd_cxgbe_LIBRARY
  DPDK_pmd_dpaa_LIBRARY
  DPDK_pmd_dpaa2_LIBRARY
  DPDK_pmd_dpaa2_cmdif_LIBRARY
  DPDK_pmd_dpaa2_event_LIBRARY
  DPDK_pmd_dpaa2_qdma_LIBRARY
  DPDK_pmd_dpaa2_sec_LIBRARY
  DPDK_pmd_dpaa_event_LIBRARY
  DPDK_pmd_dpaa_sec_LIBRARY
  DPDK_pmd_e1000_LIBRARY
  DPDK_pmd_ena_LIBRARY
  DPDK_pmd_enic_LIBRARY
  DPDK_pmd_failsafe_LIBRARY
  DPDK_pmd_fm10k_LIBRARY
  DPDK_pmd_i40e_LIBRARY
  DPDK_pmd_ifpga_rawdev_LIBRARY
  DPDK_pmd_ixgbe_LIBRARY
  DPDK_pmd_kni_LIBRARY
  DPDK_pmd_lio_LIBRARY
  DPDK_pmd_nfp_LIBRARY
  DPDK_pmd_null_LIBRARY
  DPDK_pmd_null_crypto_LIBRARY
  DPDK_pmd_octeontx_LIBRARY
  DPDK_pmd_octeontx_ssovf_LIBRARY
  DPDK_pmd_opdl_event_LIBRARY
  DPDK_pmd_qede_LIBRARY
  DPDK_pmd_ring_LIBRARY
  DPDK_pmd_sfc_efx_LIBRARY
  DPDK_pmd_skeleton_event_LIBRARY
  DPDK_pmd_skeleton_rawdev_LIBRARY
  DPDK_pmd_softnic_LIBRARY
  DPDK_pmd_sw_event_LIBRARY
  DPDK_pmd_tap_LIBRARY
  DPDK_pmd_thunderx_nicvf_LIBRARY
  DPDK_pmd_vdev_netvsc_LIBRARY
  DPDK_pmd_vhost_LIBRARY
  DPDK_pmd_virtio_LIBRARY
  DPDK_pmd_virtio_crypto_LIBRARY
  DPDK_pmd_vmxnet3_uio_LIBRARY
  DPDK_port_LIBRARY
  DPDK_power_LIBRARY
  DPDK_rawdev_LIBRARY
  DPDK_reorder_LIBRARY
  DPDK_ring_LIBRARY
  DPDK_sched_LIBRARY
  DPDK_security_LIBRARY
  DPDK_table_LIBRARY
  DPDK_timer_LIBRARY
  DPDK_vhost_LIBRARY
  LIBUUID_LIBRARY
)
if(dpdk_FOUND)
set(
  DPDK_LIBRARIES
  -Wl,--whole-archive
  rte_acl
  rte_bbdev
  rte_bitratestats
  rte_bpf
  rte_bus_dpaa
  rte_bus_fslmc
  rte_bus_ifpga
  rte_bus_pci
  rte_bus_vdev
  rte_cfgfile
  rte_cmdline
  rte_common_octeontx
  rte_compressdev
  rte_cryptodev
  rte_distributor
  rte_eal
  rte_efd
  rte_ethdev
  rte_eventdev
  rte_flow_classify
  rte_gro
  rte_gso
  rte_hash
  rte_ifcvf_vdpa
  rte_ip_frag
  rte_jobstats
  rte_kni
  rte_kvargs
  rte_latencystats
  rte_lpm
  rte_mbuf
  rte_member
  rte_mempool
  rte_mempool_bucket
  rte_mempool_dpaa
  rte_mempool_dpaa2
  rte_mempool_octeontx
  rte_mempool_ring
  rte_mempool_stack
  rte_meter
  rte_metrics
  rte_net
  rte_pci
  rte_pdump
  rte_pipeline
  rte_pmd_af_packet
  rte_pmd_ark
  rte_pmd_avf
  rte_pmd_avp
  rte_pmd_axgbe
  rte_pmd_bbdev_null
  rte_pmd_bnxt
  rte_pmd_bond
  rte_pmd_crypto_scheduler
  rte_pmd_cxgbe
  rte_pmd_dpaa
  rte_pmd_dpaa2
  rte_pmd_dpaa2_cmdif
  rte_pmd_dpaa2_event
  rte_pmd_dpaa2_qdma
  rte_pmd_dpaa2_sec
  rte_pmd_dpaa_event
  rte_pmd_dpaa_sec
  rte_pmd_e1000
  rte_pmd_ena
  rte_pmd_enic
  rte_pmd_failsafe
  rte_pmd_fm10k
  rte_pmd_i40e
  rte_pmd_ifpga_rawdev
  rte_pmd_ixgbe
  rte_pmd_kni
  rte_pmd_lio
  rte_pmd_nfp
  rte_pmd_null
  rte_pmd_null_crypto
  rte_pmd_octeontx
  rte_pmd_octeontx_ssovf
  rte_pmd_opdl_event
  rte_pmd_qede
  rte_pmd_ring
  rte_pmd_sfc_efx
  rte_pmd_skeleton_event
  rte_pmd_skeleton_rawdev
  rte_pmd_softnic
  rte_pmd_sw_event
  rte_pmd_tap
  rte_pmd_thunderx_nicvf
  rte_pmd_vdev_netvsc
  rte_pmd_vhost
  rte_pmd_virtio
  rte_pmd_virtio_crypto
  rte_pmd_vmxnet3_uio
  rte_port
  rte_power
  rte_rawdev
  rte_reorder
  rte_ring
  rte_sched
  rte_security
  rte_table
  rte_timer
  rte_vhost
  -Wl,--no-whole-archive
  ${CMAKE_DL_LIBS}
  ${LIBUUID_LIBRARIES}
  ${LIBNUMA_LIBRARIES}
  Threads::Threads
  rt
)
set(DPDK_LIBRARY_DIRS ${DETECTED_DPDK_ROOT_DIR}/lib)
endif(dpdk_FOUND)
mark_as_advanced(
  DPDK_INCLUDE_DIR
  DPDK_LIBRARIES
  DPDK_LIBRARY_DIRS
  DPDK_dpdk_LIBRARY
  DPDK_acl_LIBRARY
  DPDK_bbdev_LIBRARY
  DPDK_bitratestats_LIBRARY
  DPDK_bpf_LIBRARY
  DPDK_bus_dpaa_LIBRARY
  DPDK_bus_fslmc_LIBRARY
  DPDK_bus_ifpga_LIBRARY
  DPDK_bus_pci_LIBRARY
  DPDK_bus_vdev_LIBRARY
  DPDK_cfgfile_LIBRARY
  DPDK_cmdline_LIBRARY
  DPDK_common_octeontx_LIBRARY
  DPDK_compressdev_LIBRARY
  DPDK_cryptodev_LIBRARY
  DPDK_distributor_LIBRARY
  DPDK_eal_LIBRARY
  DPDK_efd_LIBRARY
  DPDK_ethdev_LIBRARY
  DPDK_eventdev_LIBRARY
  DPDK_flow_classify_LIBRARY
  DPDK_gro_LIBRARY
  DPDK_gso_LIBRARY
  DPDK_hash_LIBRARY
  DPDK_ifcvf_vdpa_LIBRARY
  DPDK_ip_frag_LIBRARY
  DPDK_jobstats_LIBRARY
  DPDK_kni_LIBRARY
  DPDK_kvargs_LIBRARY
  DPDK_latencystats_LIBRARY
  DPDK_lpm_LIBRARY
  DPDK_mbuf_LIBRARY
  DPDK_member_LIBRARY
  DPDK_mempool_LIBRARY
  DPDK_mempool_bucket_LIBRARY
  DPDK_mempool_dpaa_LIBRARY
  DPDK_mempool_dpaa2_LIBRARY
  DPDK_mempool_octeontx_LIBRARY
  DPDK_mempool_ring_LIBRARY
  DPDK_mempool_stack_LIBRARY
  DPDK_meter_LIBRARY
  DPDK_metrics_LIBRARY
  DPDK_net_LIBRARY
  DPDK_pci_LIBRARY
  DPDK_pdump_LIBRARY
  DPDK_pipeline_LIBRARY
  DPDK_pmd_af_packet_LIBRARY
  DPDK_pmd_ark_LIBRARY
  DPDK_pmd_avf_LIBRARY
  DPDK_pmd_avp_LIBRARY
  DPDK_pmd_axgbe_LIBRARY
  DPDK_pmd_bbdev_null_LIBRARY
  DPDK_pmd_bnxt_LIBRARY
  DPDK_pmd_bond_LIBRARY
  DPDK_pmd_crypto_scheduler_LIBRARY
  DPDK_pmd_cxgbe_LIBRARY
  DPDK_pmd_dpaa_LIBRARY
  DPDK_pmd_dpaa2_LIBRARY
  DPDK_pmd_dpaa2_cmdif_LIBRARY
  DPDK_pmd_dpaa2_event_LIBRARY
  DPDK_pmd_dpaa2_qdma_LIBRARY
  DPDK_pmd_dpaa2_sec_LIBRARY
  DPDK_pmd_dpaa_event_LIBRARY
  DPDK_pmd_dpaa_sec_LIBRARY
  DPDK_pmd_e1000_LIBRARY
  DPDK_pmd_ena_LIBRARY
  DPDK_pmd_enic_LIBRARY
  DPDK_pmd_failsafe_LIBRARY
  DPDK_pmd_fm10k_LIBRARY
  DPDK_pmd_i40e_LIBRARY
  DPDK_pmd_ifpga_rawdev_LIBRARY
  DPDK_pmd_ixgbe_LIBRARY
  DPDK_pmd_kni_LIBRARY
  DPDK_pmd_lio_LIBRARY
  DPDK_pmd_nfp_LIBRARY
  DPDK_pmd_null_LIBRARY
  DPDK_pmd_null_crypto_LIBRARY
  DPDK_pmd_octeontx_LIBRARY
  DPDK_pmd_octeontx_ssovf_LIBRARY
  DPDK_pmd_opdl_event_LIBRARY
  DPDK_pmd_qede_LIBRARY
  DPDK_pmd_ring_LIBRARY
  DPDK_pmd_sfc_efx_LIBRARY
  DPDK_pmd_skeleton_event_LIBRARY
  DPDK_pmd_skeleton_rawdev_LIBRARY
  DPDK_pmd_softnic_LIBRARY
  DPDK_pmd_sw_event_LIBRARY
  DPDK_pmd_tap_LIBRARY
  DPDK_pmd_thunderx_nicvf_LIBRARY
  DPDK_pmd_vdev_netvsc_LIBRARY
  DPDK_pmd_vhost_LIBRARY
  DPDK_pmd_virtio_LIBRARY
  DPDK_pmd_virtio_crypto_LIBRARY
  DPDK_pmd_vmxnet3_uio_LIBRARY
  DPDK_port_LIBRARY
  DPDK_power_LIBRARY
  DPDK_rawdev_LIBRARY
  DPDK_reorder_LIBRARY
  DPDK_ring_LIBRARY
  DPDK_sched_LIBRARY
  DPDK_security_LIBRARY
  DPDK_table_LIBRARY
  DPDK_timer_LIBRARY
  DPDK_vhost_LIBRARY
)
