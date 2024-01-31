/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ATSC3_H
#define CXDREF_ATSC3_H

#include "cxdref_common.h"

#define CXDREF_ATSC3_NUM_PLP_MAX        64

typedef enum {
    CXDREF_ATSC3_SYSTEM_BW_6_MHZ,
    CXDREF_ATSC3_SYSTEM_BW_7_MHZ,
    CXDREF_ATSC3_SYSTEM_BW_8_MHZ,
    CXDREF_ATSC3_SYSTEM_BW_OVER_8_MHZ
} cxdref_atsc3_system_bw_t;

typedef enum {
    CXDREF_ATSC3_PREAMBLE_PILOT_3,
    CXDREF_ATSC3_PREAMBLE_PILOT_4,
    CXDREF_ATSC3_PREAMBLE_PILOT_6,
    CXDREF_ATSC3_PREAMBLE_PILOT_8,
    CXDREF_ATSC3_PREAMBLE_PILOT_12,
    CXDREF_ATSC3_PREAMBLE_PILOT_16,
    CXDREF_ATSC3_PREAMBLE_PILOT_24,
    CXDREF_ATSC3_PREAMBLE_PILOT_32,
} cxdref_atsc3_preamble_pilot_t;

typedef enum {
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE1,
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE2,
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE3,
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE4,
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE5,
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE6,
    CXDREF_ATSC3_L1B_FEC_TYPE_MODE7,
    CXDREF_ATSC3_L1B_FEC_TYPE_RESERVED
} cxdref_atsc3_l1b_fec_type_t;

typedef enum {
    CXDREF_ATSC3_TIME_INFO_FLAG_NONE,
    CXDREF_ATSC3_TIME_INFO_FLAG_MS,
    CXDREF_ATSC3_TIME_INFO_FLAG_US,
    CXDREF_ATSC3_TIME_INFO_FLAG_NS
} cxdref_atsc3_time_info_flag_t;

typedef enum {
    CXDREF_ATSC3_PAPR_NONE,
    CXDREF_ATSC3_PAPR_TR,
    CXDREF_ATSC3_PAPR_ACE,
    CXDREF_ATSC3_PAPR_TR_ACE
} cxdref_atsc3_papr_t;

typedef enum {
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE1,
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE2,
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE3,
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE4,
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE5,
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE6,
    CXDREF_ATSC3_L1D_FEC_TYPE_MODE7,
    CXDREF_ATSC3_L1D_FEC_TYPE_RESERVED
} cxdref_atsc3_l1d_fec_type_t;

typedef enum {
    CXDREF_ATSC3_MISO_NONE,
    CXDREF_ATSC3_MISO_64,
    CXDREF_ATSC3_MISO_256,
    CXDREF_ATSC3_MISO_RESERVED
} cxdref_atsc3_miso_t;

typedef enum {
    CXDREF_ATSC3_FFT_SIZE_8K,
    CXDREF_ATSC3_FFT_SIZE_16K,
    CXDREF_ATSC3_FFT_SIZE_32K,
    CXDREF_ATSC3_FFT_SIZE_RESERVED
} cxdref_atsc3_fft_size_t;

typedef enum {
    CXDREF_ATSC3_GI_RESERVED_0,
    CXDREF_ATSC3_GI_1_192,
    CXDREF_ATSC3_GI_2_384,
    CXDREF_ATSC3_GI_3_512,
    CXDREF_ATSC3_GI_4_768,
    CXDREF_ATSC3_GI_5_1024,
    CXDREF_ATSC3_GI_6_1536,
    CXDREF_ATSC3_GI_7_2048,
    CXDREF_ATSC3_GI_8_2432,
    CXDREF_ATSC3_GI_9_3072,
    CXDREF_ATSC3_GI_10_3648,
    CXDREF_ATSC3_GI_11_4096,
    CXDREF_ATSC3_GI_12_4864,
    CXDREF_ATSC3_GI_RESERVED_13,
    CXDREF_ATSC3_GI_RESERVED_14,
    CXDREF_ATSC3_GI_RESERVED_15
} cxdref_atsc3_gi_t;

typedef enum {
    CXDREF_ATSC3_SP_3_2,
    CXDREF_ATSC3_SP_3_4,
    CXDREF_ATSC3_SP_4_2,
    CXDREF_ATSC3_SP_4_4,
    CXDREF_ATSC3_SP_6_2,
    CXDREF_ATSC3_SP_6_4,
    CXDREF_ATSC3_SP_8_2,
    CXDREF_ATSC3_SP_8_4,
    CXDREF_ATSC3_SP_12_2,
    CXDREF_ATSC3_SP_12_4,
    CXDREF_ATSC3_SP_16_2,
    CXDREF_ATSC3_SP_16_4,
    CXDREF_ATSC3_SP_24_2,
    CXDREF_ATSC3_SP_24_4,
    CXDREF_ATSC3_SP_32_2,
    CXDREF_ATSC3_SP_32_4,
    CXDREF_ATSC3_SP_RESERVED_16,
    CXDREF_ATSC3_SP_RESERVED_17,
    CXDREF_ATSC3_SP_RESERVED_18,
    CXDREF_ATSC3_SP_RESERVED_19,
    CXDREF_ATSC3_SP_RESERVED_20,
    CXDREF_ATSC3_SP_RESERVED_21,
    CXDREF_ATSC3_SP_RESERVED_22,
    CXDREF_ATSC3_SP_RESERVED_23
} cxdref_atsc3_sp_t;

typedef enum {
    CXDREF_ATSC3_PLP_FEC_BCH_LDPC_16K,
    CXDREF_ATSC3_PLP_FEC_BCH_LDPC_64K,
    CXDREF_ATSC3_PLP_FEC_CRC_LDPC_16K,
    CXDREF_ATSC3_PLP_FEC_CRC_LDPC_64K,
    CXDREF_ATSC3_PLP_FEC_LDPC_16K,
    CXDREF_ATSC3_PLP_FEC_LDPC_64K,
    CXDREF_ATSC3_PLP_FEC_RESERVED_6,
    CXDREF_ATSC3_PLP_FEC_RESERVED_7,
    CXDREF_ATSC3_PLP_FEC_RESERVED_8,
    CXDREF_ATSC3_PLP_FEC_RESERVED_9,
    CXDREF_ATSC3_PLP_FEC_RESERVED_10,
    CXDREF_ATSC3_PLP_FEC_RESERVED_11,
    CXDREF_ATSC3_PLP_FEC_RESERVED_12,
    CXDREF_ATSC3_PLP_FEC_RESERVED_13,
    CXDREF_ATSC3_PLP_FEC_RESERVED_14,
    CXDREF_ATSC3_PLP_FEC_RESERVED_15
} cxdref_atsc3_plp_fec_t;

typedef enum {
    CXDREF_ATSC3_PLP_MOD_QPSK,
    CXDREF_ATSC3_PLP_MOD_16QAM,
    CXDREF_ATSC3_PLP_MOD_64QAM,
    CXDREF_ATSC3_PLP_MOD_256QAM,
    CXDREF_ATSC3_PLP_MOD_1024QAM,
    CXDREF_ATSC3_PLP_MOD_4096QAM,
    CXDREF_ATSC3_PLP_MOD_RESERVED_6,
    CXDREF_ATSC3_PLP_MOD_RESERVED_7,
    CXDREF_ATSC3_PLP_MOD_RESERVED_8,
    CXDREF_ATSC3_PLP_MOD_RESERVED_9,
    CXDREF_ATSC3_PLP_MOD_RESERVED_10,
    CXDREF_ATSC3_PLP_MOD_RESERVED_11,
    CXDREF_ATSC3_PLP_MOD_RESERVED_12,
    CXDREF_ATSC3_PLP_MOD_RESERVED_13,
    CXDREF_ATSC3_PLP_MOD_RESERVED_14,
    CXDREF_ATSC3_PLP_MOD_RESERVED_15
} cxdref_atsc3_plp_mod_t;

typedef enum {
    CXDREF_ATSC3_PLP_COD_2_15,
    CXDREF_ATSC3_PLP_COD_3_15,
    CXDREF_ATSC3_PLP_COD_4_15,
    CXDREF_ATSC3_PLP_COD_5_15,
    CXDREF_ATSC3_PLP_COD_6_15,
    CXDREF_ATSC3_PLP_COD_7_15,
    CXDREF_ATSC3_PLP_COD_8_15,
    CXDREF_ATSC3_PLP_COD_9_15,
    CXDREF_ATSC3_PLP_COD_10_15,
    CXDREF_ATSC3_PLP_COD_11_15,
    CXDREF_ATSC3_PLP_COD_12_15,
    CXDREF_ATSC3_PLP_COD_13_15,
    CXDREF_ATSC3_PLP_COD_RESERVED_12,
    CXDREF_ATSC3_PLP_COD_RESERVED_13,
    CXDREF_ATSC3_PLP_COD_RESERVED_14,
    CXDREF_ATSC3_PLP_COD_RESERVED_15
} cxdref_atsc3_plp_cod_t;

typedef enum {
    CXDREF_ATSC3_PLP_TI_NONE,
    CXDREF_ATSC3_PLP_TI_CTI,
    CXDREF_ATSC3_PLP_TI_HTI,
    CXDREF_ATSC3_PLP_TI_RESERVED
} cxdref_atsc3_plp_ti_t;

typedef enum {
    CXDREF_ATSC3_PLP_CH_BOND_FMT_PLAIN,
    CXDREF_ATSC3_PLP_CH_BOND_FMT_SNR,
    CXDREF_ATSC3_PLP_CH_BOND_FMT_RESERVED_2,
    CXDREF_ATSC3_PLP_CH_BOND_FMT_RESERVED_3
} cxdref_atsc3_plp_ch_bond_fmt_t;

typedef enum {
    CXDREF_ATSC3_PLP_CTI_DEPTH_512,
    CXDREF_ATSC3_PLP_CTI_DEPTH_724,
    CXDREF_ATSC3_PLP_CTI_DEPTH_887_1254,
    CXDREF_ATSC3_PLP_CTI_DEPTH_1024_1448,
    CXDREF_ATSC3_PLP_CTI_DEPTH_RESERVED_4,
    CXDREF_ATSC3_PLP_CTI_DEPTH_RESERVED_5,
    CXDREF_ATSC3_PLP_CTI_DEPTH_RESERVED_6,
    CXDREF_ATSC3_PLP_CTI_DEPTH_RESERVED_7
} cxdref_atsc3_plp_cti_depth_t;

typedef enum {
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_0_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_0_5,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_1_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_1_5,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_2_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_2_5,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_3_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_3_5,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_4_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_4_5,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_5_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_6_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_7_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_8_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_9_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_10_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_11_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_12_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_13_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_14_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_15_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_16_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_17_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_18_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_19_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_20_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_21_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_22_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_23_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_24_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_25_0,
    CXDREF_ATSC3_PLP_LDM_INJ_LEVEL_RESERVED,
} cxdref_atsc3_plp_ldm_inj_level_t;

typedef struct cxdref_atsc3_bootstrap_t {
    uint8_t bw_diff;
    cxdref_atsc3_system_bw_t system_bw;
    uint8_t ea_wake_up;
} cxdref_atsc3_bootstrap_t;

typedef struct cxdref_atsc3_ofdm_t {
    cxdref_atsc3_system_bw_t      system_bw;
    uint8_t                     bs_num_symbol;
    uint8_t                     num_subframe;
    cxdref_atsc3_l1b_fec_type_t   l1b_fec_type;
    cxdref_atsc3_papr_t           papr;
    uint8_t                     pb_num_symbol;
    cxdref_atsc3_fft_size_t       pb_fft_size;
    cxdref_atsc3_preamble_pilot_t pb_pilot;
    uint8_t                     pb_reduced_carriers;
    cxdref_atsc3_gi_t             pb_gi;
    cxdref_atsc3_fft_size_t       sf0_fft_size;
    cxdref_atsc3_sp_t             sf0_sp;
    uint8_t                     sf0_reduced_carriers;
    cxdref_atsc3_gi_t             sf0_gi;
    uint8_t                     sf0_sp_boost;
    uint8_t                     sf0_sbs_first;
    uint8_t                     sf0_sbs_last;
    uint16_t                    sf0_num_ofdm_symbol;
} cxdref_atsc3_ofdm_t;

typedef struct cxdref_atsc3_l1basic_t {
    uint8_t                     version;
    uint8_t                     mimo_sp_enc;
    uint8_t                     lls_flg;
    cxdref_atsc3_time_info_flag_t time_info_flg;
    uint8_t                     return_ch_flg;
    cxdref_atsc3_papr_t           papr;
    uint8_t                     frame_length_mode;
    uint16_t                    frame_length;
    uint16_t                    excess_smp_per_sym;
    uint16_t                    time_offset;
    uint8_t                     additional_smp;
    uint8_t                     num_subframe;

    uint8_t                     pb_num_symbol;
    uint8_t                     pb_reduced_carriers;
    uint8_t                     l1d_content_tag;
    uint16_t                    l1d_size;
    cxdref_atsc3_l1d_fec_type_t   l1d_fec_type;
    uint8_t                     l1d_add_parity_mode;
    uint32_t                    l1d_total_cells;

    uint8_t                     sf0_mimo;
    cxdref_atsc3_miso_t           sf0_miso;
    cxdref_atsc3_fft_size_t       sf0_fft_size;
    uint8_t                     sf0_reduced_carriers;
    cxdref_atsc3_gi_t             sf0_gi;
    uint16_t                    sf0_num_ofdm_symbol;
    cxdref_atsc3_sp_t             sf0_sp;
    uint8_t                     sf0_sp_boost;
    uint8_t                     sf0_sbs_first;
    uint8_t                     sf0_sbs_last;

    uint8_t                     reserved[6];
} cxdref_atsc3_l1basic_t;

typedef struct cxdref_atsc3_l1detail_raw_t {
    uint16_t                    size;
    uint8_t                     data[8191];
} cxdref_atsc3_l1detail_raw_t;

typedef struct cxdref_atsc3_l1detail_common_t {
    uint8_t                     version;
    uint8_t                     num_rf;
    uint16_t                    bonded_bsid;
    uint32_t                    time_sec;
    uint16_t                    time_msec;
    uint16_t                    time_usec;
    uint16_t                    time_nsec;

    uint16_t                    bsid;
    uint16_t                    reserved_bitlen;
} cxdref_atsc3_l1detail_common_t;

typedef struct cxdref_atsc3_l1detail_subframe_t {
    uint8_t                     index;
    uint8_t                     mimo;
    cxdref_atsc3_miso_t           miso;
    cxdref_atsc3_fft_size_t       fft_size;
    uint8_t                     reduced_carriers;
    cxdref_atsc3_gi_t             gi;
    uint16_t                    num_ofdm_symbol;
    cxdref_atsc3_sp_t             sp;
    uint8_t                     sp_boost;
    uint8_t                     sbs_first;
    uint8_t                     sbs_last;
    uint8_t                     subframe_mux;

    uint8_t                     freq_interleaver;
    uint16_t                    sbs_null_cells;

    uint8_t                     num_plp;
} cxdref_atsc3_l1detail_subframe_t;

typedef struct cxdref_atsc3_l1detail_plp_t {
    uint8_t                     id;
    uint8_t                     lls_flg;
    uint8_t                     layer;
    uint32_t                    start;
    uint32_t                    size;
    uint8_t                     scrambler_type;
    cxdref_atsc3_plp_fec_t        fec_type;
    cxdref_atsc3_plp_mod_t        mod;
    cxdref_atsc3_plp_cod_t        cod;
    cxdref_atsc3_plp_ti_t         ti_mode;
    uint16_t                    fec_block_start;
    uint32_t                    cti_fec_block_start;

    uint8_t                     num_ch_bonded;
    cxdref_atsc3_plp_ch_bond_fmt_t ch_bonding_format;
    uint8_t                     bonded_rf_id_0;
    uint8_t                     bonded_rf_id_1;

    uint8_t                     mimo_stream_combine;
    uint8_t                     mimo_iq_interleave;
    uint8_t                     mimo_ph;

    uint8_t                     plp_type;
    uint16_t                    num_subslice;
    uint32_t                    subslice_interval;

    uint8_t                     ti_ext_interleave;

    cxdref_atsc3_plp_cti_depth_t  cti_depth;
    uint16_t                    cti_start_row;

    uint8_t                     hti_inter_subframe;
    uint8_t                     hti_num_ti_blocks;
    uint16_t                    hti_num_fec_block_max;
    uint16_t                    hti_num_fec_block[16];
    uint8_t                     hti_cell_interleave;

    
    cxdref_atsc3_plp_ldm_inj_level_t ldm_inj_level;
} cxdref_atsc3_l1detail_plp_t;


typedef struct cxdref_atsc3_plp_list_entry_t {
    uint8_t                     id;
    uint8_t                     lls_flg;
    uint8_t                     layer;
    uint8_t                     chbond;
} cxdref_atsc3_plp_list_entry_t;


typedef struct cxdref_atsc3_plp_fecmodcod_t {
    uint8_t                     valid;
    cxdref_atsc3_plp_fec_t        fec_type;
    cxdref_atsc3_plp_mod_t        mod;
    cxdref_atsc3_plp_cod_t        cod;
} cxdref_atsc3_plp_fecmodcod_t;

#endif
