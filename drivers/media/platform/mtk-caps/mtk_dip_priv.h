/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include "mdrv_dip.h"
#include "utpa2_DIP.h"
#include "drv_scriptmgt.h"
#include "mdrv_dip_rm.h"

#define DIP_INPUT     (1 << 0)
#define DIP_OUTPUT    (1 << 1)

#define DIP_NAME "mtk-dip"

#define ST_OFF          (0x0)
#define ST_ON           (0x1)
#define ST_ABORT        (0x2)
#define ST_CONT         (0x4)

#define CONT_MIN_BUF    (2)

#define FRAMEWK_ASSIGN_NO    (0xFF)

#define DIP_TIMEOUT   (1000)

#define DIP_DEFAULT_WIDTH   (1920)
#define DIP_DEFAULT_HEIGHT   (1080)

#define IOMMU_BUF_TAG_SHIFT          (24)

#define ChromaFB_BITSHIFT            (16)
#define ChromaBL_BITSHIFT            (16)

#define DIP_ADDR_ALIGN               (32)
#define DIP_GNRL_FMT_WIDTH_MAX       (7680)
#define DIP_FHD_FMT_WIDTH_MAX        (1920)
#define DIP_IN_WIDTH_MIN             (128)
#define DIP_OUT_WIDTH_MIN            (128)
#define DIP_ROT_HV_ALIGN             (64)
#define DIP_RWSEP_420TILE_ALIGN      (64)
#define DIP_RWSEP_V1_VERSION         (1)
#define DIP_RWSEP_V0_VERSION         (0)
#define DIP_RWSEP_IN_WIDTH_MAX       (16384)
#define DIP_RWSEP_V1_IN_HEIGHT_MAX   (16383)
#define DIP_RWSEP_V0_IN_HEIGHT_MAX   (8190)
#define DIP_RWSEP_OUT_WIDTH_MAX      (8192)
#define DIP_RWSEP_OUT_HEIGHT_MAX     (8192)
#define DIP_CROP_WIDTH_ALIGN         (2)

#define DIP0_DI_IN_WIDTH_MAX         (0)
#define DIP0_DI_IN_WIDTH_ALIGN       (0)
#define DIP0_HVSP_IN_WIDTH_MAX       (0)

#define DIP1_420LINEAR_MAX_WIDTH     (3840)
#define DIP1_DI_IN_WIDTH_MAX         (1920)
#define DIP1_DI_IN_WIDTH_ALIGN       (2)
#define DIP1_HVSP_IN_WIDTH_MAX       (3840)

#define DIP2_420LINEAR_MAX_WIDTH     (3840)
#define DIP2_DI_IN_WIDTH_MAX         (0)
#define DIP2_DI_IN_WIDTH_ALIGN       (0)
#define DIP2_HVSP_IN_WIDTH_MAX       (0)

#define DIPR_420LINEAR_MAX_WIDTH     (3840)

#define DIPR_YUV422_ROT_WIDTH_MAX    (3840)
#define DIPW_YUV422_ROT_WIDTH_MAX    (2160)

#define ARGB32_HSIZE_ALIGN            (8)
#define ARGB32_PITCH_ALIGN            (ARGB32_HSIZE_ALIGN)
#define RGB24_HSIZE_ALIGN             (32)
#define RGB24_PITCH_ALIGN             (RGB24_HSIZE_ALIGN)
#define RGB16_HSIZE_ALIGN             (16)
#define RGB16_PITCH_ALIGN             (RGB16_HSIZE_ALIGN)
#define YUV422_10B_HSIZE_ALIGN        (64)
#define YUV422_10B_PITCH_ALIGN        (YUV422_10B_HSIZE_ALIGN)
#define YUV422_SEP_HSIZE_ALIGN        (32)
#define YUV422_SEP_PITCH_ALIGN        (YUV422_SEP_HSIZE_ALIGN)
#define YUV422_8BLKTILE_HSIZE_ALIGN   (64)
#define YUV422_8BLKTILE_PITCH_ALIGN   (YUV422_8BLKTILE_HSIZE_ALIGN)
#define YUV422_10BLKTILE_HSIZE_ALIGN  (64)
#define YUV422_10BLKTILE_PITCH_ALIGN  (YUV422_10BLKTILE_HSIZE_ALIGN)
#define YUV420_LINEAR_HSIZE_ALIGN     (32)
#define YUV420_LINEAR_PITCH_ALIGN     (YUV420_LINEAR_HSIZE_ALIGN)
#define YUV420_10BLINEAR_HSIZE_ALIGN  (128)
#define YUV420_10BLINEAR_PITCH_ALIGN  (YUV420_10BLINEAR_HSIZE_ALIGN)
#define YUV420_8BTILE_HSIZE_ALIGN     (16)
#define YUV420_R_8BTILE_PITCH_ALIGN   (16)
#define YUV420_W_8BTILE_PITCH_ALIGN   (64)
#define YUV420_10BTILE_HSIZE_ALIGN    (16)
#define YUV420_R_10BTILE_PITCH_ALIGN  (16)
#define YUV420_W_10BTILE_PITCH_ALIGN  (64)
#define MT21C_HSIZE_ALIGN    (32)
#define MT21C_PITCH_ALIGN    (16)
#define VSD_HSIZE_ALIGN       (32)
#define VSD_PITCH_ALIGN       (4)
#define DIPR_VSD_HSIZE_ALIGN       (16)
#define DIPR_VSD_PITCH_ALIGN       (16)
#define YV12_HSIZE_ALIGN    (16)
#define YV12_PITCH_ALIGN    (1)
#define YUV422_ROT_HSIZE_ALIGN    (2)
#define YUV422_ROT_PITCH_ALIGN    (2)
#define YUV422_ROT_HEIGHT_ALIGN	(32)

#define YUV422_HEIGHT_ALIGN              (1)
#define YUV420_HEIGHT_ALIGN              (2)
#define YUV420TL_8B_HEIGHT_WRITE_ALIGN   (8)
#define YUV420TL_10B_HEIGHT_WRITE_ALIGN  (16)
#define YUV420TL_HEIGHT_BUF_ALIGN        (32)

#define DIP_HVSP_RATIO_MAX        (15)

#define DIP_CLK_720MHZ      (720000000ul)
#define DIP_MAX_CLK         DIP_CLK_720MHZ
#define BW_BASE_VAL         (128)
#define META_UNIT           (1000000)

#define BPP_4BYTE       (4)
#define BPP_3BYTE       (3)
#define BPP_2BYTE       (2)
#define BPP_1BYTE       (1)
#define BPP_NO_MOD      (1)
#define BPP_2_MOD       (2)
#define BPP_YUV422_10B_DIV     (5)
#define BPP_YUV422_10B_MOD     (2)
#define BPP_YUV420_10B_DIV     (5)
#define BPP_YUV420_10B_MOD     (4)

#define MOD2                   (2)

#define PLANE0_NUM        (0)
#define PLANE1_NUM        (1)
#define PLANE2_NUM        (2)
#define PLANE3_NUM        (3)
#define MAX_PLANE_NUM     (4)
#define PLANE_NUM_3     (3)

#define DIP_RWSEP_TIMES   (4)

#define DIP_CRC_FRMCNT    (2)

#define INVALID_ADDR      (0xFFFFFFFF)

#define DIP_DIDNR_BPP       (4)
#define DIP_DI_CNT          (4)
#define DIP_DNR_CNT         (2)

#define EFUSE_DIP_PCI_TRUE       (1)
#define EFUSE_DIP_SDC_SUPPORT    (0)

#define SDC_SD_WIDTH         (720)
#define SDC_SD_HEIGHT        (576)

#define SDC_CAP_B2R      (1)
#define SDC_CAP_DIPR     (2)

#define ROT90_ANGLE        (90)
#define ROT270_ANGLE       (270)
#define ROT90_M2M_SYS_BIT      (0x1)
#define ROT270_M2M_SYS_BIT     (0x2)
#define ROT90_CAP_SYS_BIT      (0x4)
#define ROT270_CAP_SYS_BIT     (0x8)

#define DIP_PQ_HBLANKING         (3)

/* UDMA level */
#define DIPW0_PREULTRA_VAL       (34)
#define DIPW0_ULTRA_VAL          (51)
#define DIPW0_URGENT_VAL         (62)
#define DIPW1_PREULTRA_VAL       (34)
#define DIPW1_ULTRA_VAL          (51)
#define DIPW1_URGENT_VAL         (62)
#define DIPW2_PREULTRA_VAL       (35)
#define DIPW2_ULTRA_VAL          (52)
#define DIPW2_URGENT_VAL         (63)
#define DIPR_PREULTRA_VAL        (35)
#define DIPR_ULTRA_VAL           (52)
#define DIPR_URGENT_VAL          (63)
#define DDIW_PREULTRA_VAL        (44)
#define DDIW_ULTRA_VAL           (66)
#define DDIW_URGENT_VAL          (79)
#define DDIR_PREULTRA_VAL        (44)
#define DDIR_ULTRA_VAL           (66)
#define DDIR_URGENT_VAL          (79)

/* B2R */
#define B2R_CMPS_ADDR_OFT       (3)
#define B2R_UNCMPS_ADDR_OFT     (2)

#define B2R_LUMA_FB_CMPS_ADR_AL (512)
#define B2R_CHRO_FB_CMPS_ADR_AL (512)
#define B2R_LUMA_BL_CMPS_ADR_AL (64)
#define B2R_CHRO_BL_CMPS_ADR_AL (64)
#define B2R_LUMA_UNCMPS_ADR_AL  (16)
#define B2R_CHRO_UNCMPS_ADR_AL  (16)

#define DIP_B2R_MIN_FPS         (1)
#define DIP_B2R_WIDTH_MAX       (4096)
#define DIP_B2R_WIDTH_ALIGN     (32)
#define DIP_B2R_HBLK_SZ         (300)
#define DIP_B2R_VBLK_SZ         (45)
#define DIP_B2R_VBLK_STR        (0x1)

#define DIP_B2R_DEF_FPS         (60)

#define DIP_B2R_CLK_720MHZ      (720000000ul)
#define DIP_B2R_CLK_630MHZ      (630000000ul)
#define DIP_B2R_CLK_360MHZ      (360000000ul)
#define DIP_B2R_CLK_312MHZ      (312000000ul)
#define DIP_B2R_CLK_156MHZ      (156000000ul)
#define DIP_B2R_CLK_144MHZ      (144000000ul)

#define LOG_LEVEL_OFF         (0)
#define LOG_LEVEL_API         (1)
#define LOG_LEVEL_BUF         (2)
#define LOG_LEVEL_QUEUE       (4)
#define LOG_LEVEL_IOCTLTIME   (8)

#define EXE_LOG_CNT           (10)

#define SYS_SIZE              (255)
#define S_TO_US               (1000000)
#define NS_TO_US              (1000)

#define MIUBUS_256            (256)
#define MIUBUS_128            (128)

#define CLK_IDX_0 (0)
#define CLK_IDX_1 (1)
#define CLK_IDX_2 (2)
#define CLK_IDX_3 (3)
#define CLK_IDX_4 (4)
#define CLK_IDX_5 (5)
#define CLK_IDX_6 (6)

#define CLK_VER_1 (1)
#define CLK_VER_2 (2)
#define CLK_VER_3 (3)
#define CLK_VER_4 (4)

#define MAX_ML_CMD (256)

#define DIP_CHECK_RETURN(RET, NAME) \
	do { \
		if (RET != 0) \
			pr_err("%s fail (%d)\n", NAME, RET); \
	} while (0)

enum EN_TOP_SUBDC_CLK {
	E_TOP_SUBDC_CLK_720MHZ = 0,
	E_TOP_SUBDC_CLK_DC1_FREERUN,
	E_TOP_SUBDC_CLK_156MHZ,
	E_TOP_SUBDC_CLK_144MHZ,
	E_TOP_SUBDC_CLK_312MHZ,
	E_TOP_SUBDC_CLK_360MHZ,
	E_TOP_SUBDC_CLK_DC1_SYNC,
	E_TOP_SUBDC_CLK_630MHZ,
};

enum EN_DIP_B2R_CLK {
	E_B2R_CLK_TOP_SUBDC = 0,
	E_B2R_CLK_TOP_SMI,
	E_B2R_CLK_TOP_MFDEC,
	E_B2R_CLK_MAX,
};

typedef enum {
	E_PRIV_TILE_FMT_16_32     = 0x00,
	E_PRIV_TILE_FMT_32_16     = 0x01,
	E_PRIV_TILE_FMT_32_32     = 0x02,
} EN_PRIV_TILE_FMT;

enum EN_DIP_MTDT_TAG {
	E_DIP_MTDT_VDEC_FRM_TAG = 0,
	E_DIP_MTDT_VDEC_COMPRESS_TAG,
	E_DIP_MTDT_VDEC_COLOR_DESC_TAG,
	E_DIP_MTDT_VDEC_SVP_TAG,
	E_DIP_MTDT_DIP_FRAME_INFO_TAG,
	E_DIP_MTDT_MAX,
};

typedef enum {
	E_CAPPT_VDEC = 0x00,
	E_CAPPT_SRC,
	E_CAPPT_PQ,
	E_CAPPT_RNDR,
	E_CAPPT_OSD,
} EN_CAPPT_TYPE;

struct dip_cap_dev {
	struct v4l2_device  v4l2_dev;
	struct video_device *vdev;
	struct device   *dev;
	struct mutex    mutex;
	spinlock_t  lock;
	u32 u32DipSupport;
	u32 u32DSCL;
	u32 u32USCL;
	u32 u32Rotate;
	u32 u32RWSep;
	u32 u32B2R;
	u32 u32MFDEC;
	u32 u323DDI;
	u32 u32DNR;
	u32 u32HDR;
	u32 u32LetterBox;
	u32 u32DIPR;
	u32 u32DiprBlending;
	u32 u32DiprPixelNum;
	u32 u32B2rPixelNum;
	u32 u32DipPixelNum;
	u32 u32FrontPack;
	u32 u32CropAlign;
	u32 u32HMirSz;
	u32 u32RWSepVer;
	u32 u32VsdInW;
	u32 u32B2rHttPixelUnit;
	u32 u32MinorNo;
	u32 TileWidthMax;
	u32 TileHeightMax;
	u32 u32SDCCap;
	u32 u32YUV10bTileIova;
	u32 u32DevelopMode;
	u32 u32YUV42010bTileIova;
	u32 u32HvspInW;
	u32 u32ClkVer;
	u32 u32MiuBus;
	u32 u32FixY2R;
	u32 u32FillBlack;
	u32 u32DipwTile;
	u32 u32DipwBKTile;
	u32 u32DipwYV12;
	u32 u32DiprVSD;
	u32 u32DiprRgb3Plane;
	u32 u32CapOsdb0;
	u32 u32CapOSD;
	u32 u32CapHighway;
	u32 u32CapLRWrite;
	u32 u32CapGOP0;
	u32 u321RNW;
	u32 u32YUV420LinearWidthMax;
	u32 u32CapB2rMain;
};


struct dip_exe_log {
	struct timespec64 StartTime;
	struct timespec64 EndTime;
	EN_DIP_SOURCE enSource;
	u8 u8IsB2RLite;
	u64 u64RealBWVal;
	u8 u8BWLimitEna;
	u64 u64BWLimitVal;
	u64 u64B2RClockRate;
	u8 u8FlowCtrlEna;
	u16 u16FlowCtrlBase;
	u16 u16FlowCtrlLimit;
	u32 u32InputWidth;
	u32 u32InputHeight;
	u32 u32Inputfourcc;
	u32 u32outputWidth;
	u32 u32OutputHeight;
	u32 u32Outputfourcc;
	u32 u32PqWinTime;
	u64 u64PqRemainderBudget;
	u16 u16Rotation;
	u16 u16BlendDispX;
	u16 u16BlendDispY;
	EN_CLR_FMT enBlendDstClrFmt;
};

struct dip_dev {
	struct v4l2_device    v4l2_dev;
	struct v4l2_m2m_dev   *m2m_dev;
	struct video_device   *vfd;
	struct mutex          mutex;
	spinlock_t            ctrl_lock;
	atomic_t              num_inst;
	struct clk            *scip_dip_clk;
	struct clk            *B2rClk[E_B2R_CLK_MAX];
	struct dip_ctx        *ctx;
	struct dip_variant    *variant;
	int irq;
	struct device         *dev;
	wait_queue_head_t     irq_queue;
	struct dip_cap_dev    dipcap_dev;
	unsigned int di_buf_tag;
	int efuse_develop_mode;
	int sys_develop_mode;
	int SDCSupport;
	struct dip_exe_log exe_log[EXE_LOG_CNT];
	u8 exe_log_cur_idx;
};

struct dip_frame {
	/* Original dimensions */
	u32 width;
	u32 height;
	/* Crop size */
	u32 c_width;
	u32 c_height;
	/* Offset */
	u32 o_width;
	u32 o_height;
	/* Scaling Out */
	u32 u32SclOutWidth;
	u32 u32SclOutHeight;
	/* Image format */
	struct dip_fmt *fmt;
	/* Variables that can calculated once and reused */
	u32 u32OutWidth;
	u32 u32OutHeight;
	u32 u32BytesPerLine[MAX_PLANE_NUM];
	u32 size[MAX_PLANE_NUM];
	u64 u64PhyAddr[MAX_PLANE_NUM];
	enum v4l2_colorspace colorspace;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
};

struct ST_DIP_B2R_MTDT {
	u32 u32Enable;
	u32 u32BitlenPitch;
	u32 u32fourcc;
	u64 u64LumaFbOft;
	u32 u32LumaFbSz;
	u64 u64ChromaFbOft;
	u32 u32ChromaFbSz;
	u64 u64LumaBitlenOft;
	u32 u32LumaBitlenSz;
	u64 u64ChromaBitlenOft;
	u32 u32ChromaBitlenSz;
};

struct ST_B2R_INFO {
	u64 u64ClockRate;
	u32 u32DeWidth;
	u32 u32DeHeight;
	u32 u32B2rPixelNum;
	u32 u32Fps;
	u32 u32B2rHttPixelUnit;
};

struct ST_DIP_DINR_BUF {
	dma_addr_t DmaAddr;
	u32 u32Size;
	unsigned long dma_attrs;
	void *virtAddr;
};

struct ST_DIP_DINR {
	u32 u32InputWidth;
	u32 u32InputHeight;
	u32 u32OutputWidth;
	u32 u32OutputHeight;
	u32 u32OpMode;
};

struct ST_DIP_WINID {
	__u8 u8Enable;
	__u16 u16WinId;
};

struct dip_secure_info {
	struct TEEC_Context *pstContext;
	struct TEEC_Session *pstSession;
	int optee_version;
	u32 u32PlaneCnt;
	u64 u64InputAddr[IN_PLANE_MAX];
	u32 u32BufCnt;
	u32 u32InputSize[IN_PLANE_MAX];
	u8 u8InAddrshift;
};

struct ST_DIP_SM_ML_INFO {
	int ml_fd;
	enum sm_ml_sync ml_sync_id;
};

struct dip_ctx {
	struct v4l2_fh fh;
	struct dip_dev      *dev;
	struct dip_frame    in;
	struct dip_frame    out;
	struct ST_SRC_INFO   stSrcInfo;
	struct ST_DIP_MFDEC stMfdecSet;
	struct ST_DIP_B2R_MTDT stB2rMtdt;
	struct ST_SCALING stScalingDown;
	struct ST_SCALING stScalingHVSP;
	struct ST_DIP_DINR stDiNrInfo;
	struct ST_RWSEP_INFO stRwSep;
	struct ST_DIP_WINID stWinid;
	struct v4l2_ctrl    *ctrl_hflip;
	struct v4l2_ctrl    *ctrl_vflip;
	struct v4l2_ctrl    *ctrl_alpha;
	struct v4l2_ctrl_handler ctrl_handler;
	struct dip_secure_info secure_info;
	struct ST_DIP_HIGHWAY_LR_CTRL stHighway;
	struct ST_DIP_BLENDING_DISP_INFO stBlendDispInfo;
	struct ST_DIP_SM_ML_INFO stDipMl;
	EN_DIP_SOURCE enSource;
	bool bHvspResource;
	u32 HMirror;
	u32 VMirror;
	u32 alpha;
	u32 u32Rotation;
	u32 u32M2mStat;
	u32 u32CapStat;
	u32 u32SrcBufMode;
	u32 u32DstBufMode;
	u32 u32SourceFps;
	u32 u32FrcInRatio;
	u32 u32FrcOutRatio;
	u32 u32B2rFps;
	u8 u8aid;
	u32 u32pipeline_id;
	u32 u32OutputSecutiry;
	u8 u8ContiBufCnt;
	u8 u8WorkingAid;
	u8 u8ReqIrq;
	u8 u8Bwlimit;
	u64 u64BytePerSec;
	u8 u8STR;
	u8 u8SecureSTR;
	u32 u32PqWinTime;
	u64 u64PqRemainderBudget;
	u8 u8FlowCtrlEna;
	u16 u16FlowCtrlBase;
	u16 u16FlowCtrlLimit;
	u64 u64B2RClockRate;
};

struct dip_fmt {
	char *name;
	u32 fourcc;
};

struct dip_variant {
	u16 u16DIPIdx;
};

struct dip_1rnw_info {
	u32 u32WriteFlag;
	u32 u32Fired;
};

int Dip_Menuload_CreateRes(struct dip_ctx *ctx, u8 bufcnt, u16 cmd_cnt, enum sm_ml_sync sync_id);
int Dip_Menuload_ReleaseRes(struct dip_ctx *ctx);
int Dip_Menuload_Begin(struct dip_ctx *ctx);
int Dip_Menuload_End(struct dip_ctx *ctx, struct hwregOut *pstDipOut);
int Dip_Menuload_Fire(struct dip_ctx *ctx, struct hwregOut *pstDipOut);
