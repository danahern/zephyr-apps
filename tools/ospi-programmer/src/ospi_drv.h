/*
 * OSPI driver header — merged from TF-A ospi_private.h + ospi_drv.h + dwc_spi.h
 *
 * Adapted for bare-metal M55_HP: volatile register access, no TF-A dependencies.
 * Uses OSPI1 controller (0x83002000) for the IS25WX256 flash on the E7 devkit.
 */

#ifndef OSPI_DRV_H
#define OSPI_DRV_H

#include <stdint.h>

/* ─── DWC SSI Register Map (ssi_regs_t) ─── */

typedef struct {
    volatile uint32_t ctrlr0;           /* 0x00 */
    volatile uint32_t ctrlr1;           /* 0x04 */
    volatile uint32_t ssienr;           /* 0x08 */
    volatile uint32_t mwcr;             /* 0x0C */
    volatile uint32_t ser;              /* 0x10 */
    volatile uint32_t baudr;            /* 0x14 */
    volatile uint32_t txftlr;           /* 0x18 */
    volatile uint32_t rxftlr;           /* 0x1C */
    volatile uint32_t txflr;            /* 0x20 */
    volatile uint32_t rxflr;            /* 0x24 */
    volatile uint32_t sr;               /* 0x28 */
    volatile uint32_t imr;              /* 0x2C */
    volatile uint32_t isr;              /* 0x30 */
    volatile uint32_t risr;             /* 0x34 */
    volatile uint32_t txoicr;           /* 0x38 */
    volatile uint32_t rxoicr;           /* 0x3C */
    volatile uint32_t rxuicr;           /* 0x40 */
    volatile uint32_t msticr;           /* 0x44 */
    volatile uint32_t icr;              /* 0x48 */
    volatile uint32_t dmacr;            /* 0x4C */
    volatile uint32_t dmatxdlr;         /* 0x50 */
    volatile uint32_t dmarxdlr;         /* 0x54 */
    volatile uint32_t spi_idr;          /* 0x58 */
    volatile uint32_t spi_ver_id;       /* 0x5C */
    volatile uint32_t data_reg;         /* 0x60 */
    volatile uint32_t drs[35];          /* 0x64–0xEC */
    volatile uint32_t rx_sample_dly;    /* 0xF0 */
    volatile uint32_t spi_ctrlr0;       /* 0xF4 */
    volatile uint32_t txd_drive_edge;   /* 0xF8 */
    volatile uint32_t xip_mode_bits;    /* 0xFC */
    volatile uint32_t xip_incr_inst;    /* 0x100 */
    volatile uint32_t xip_wrap_inst;    /* 0x104 */
    volatile uint32_t xip_ctrl;         /* 0x108 */
    volatile uint32_t xip_ser;          /* 0x10C */
    volatile uint32_t xrxoicr;          /* 0x110 */
    volatile uint32_t xip_cnt_time_out; /* 0x114 */
} ssi_regs_t;

/* AES decryption module registers */
typedef struct {
    volatile uint32_t aes_control;      /* 0x00 */
    volatile uint32_t aes_interrupt;    /* 0x04 */
    volatile uint32_t aes_interrupt_mask; /* 0x08 */
    volatile uint32_t aes_key_0;        /* 0x0C */
    volatile uint32_t aes_key_1;        /* 0x10 */
    volatile uint32_t aes_key_2;        /* 0x14 */
    volatile uint32_t aes_key_3;        /* 0x18 */
    volatile uint32_t aes_timeout_val;  /* 0x1C */
    volatile uint32_t aes_rxds_delay;   /* 0x20 */
} aes_regs_t;

/* ─── Flash configuration ─── */

typedef struct {
    ssi_regs_t *regs;
    aes_regs_t *aes_regs;
    volatile void *xip_base;
    uint32_t ospi_clock;
    uint32_t ser;               /* Slave select (1 or 2) */
    uint32_t addrlen;
    uint32_t ddr_en;
    uint32_t rx_req;
    uint32_t rx_cnt;
    uint32_t device_id;
    uint32_t wait_cycles;
} ospi_flash_cfg_t;

/* ─── Clock ─── */

#define AXI_CLOCK               400000000UL
#define OSPI_CLOCK_MHZ          100
#define OSPI_CLOCK              (OSPI_CLOCK_MHZ * 1000000UL)

/* ─── Memory map ─── */

#define OSPI1_BASE              0x83002000UL
#define AES1_BASE               0x83003000UL
#define OSPI1_XIP_BASE          0xC0000000UL

/* GPIO for flash reset */
#define PINMUX_BASE             0x1A603000UL
#define LPGPIO_BASE             0x42002000UL

/* ─── CTRLR0 bit fields ─── */

#define CTRLR0_IS_MST                   (1U << 31)
#define CTRLR0_SPI_FRF_OFFSET          22U
#define CTRLR0_SSTE_OFFSET             14U
#define CTRLR0_TMOD_OFFSET             10U
#define CTRLR0_TMOD_MASK               (3U << CTRLR0_TMOD_OFFSET)
#define CTRLR0_SCPOL_OFFSET            9U
#define CTRLR0_SCPH_OFFSET             8U
#define CTRLR0_DFS_OFFSET              0U

#define TMOD_TO                         0x1
#define TMOD_RO                         0x2

#define SINGLE                          0x0
#define OCTAL                           0x3

#define CTRLR0_DFS_8bit                 0x07U
#define CTRLR0_DFS_16bit                0x0FU
#define CTRLR0_DFS_32bit                0x1FU

/* ─── SPI_CTRLR0 bit fields ─── */

#define CTRLR0_SPI_DDR_EN_OFFSET       16U
#define CTRLR0_INST_DDR_EN_OFFSET      17U
#define CTRLR0_SPI_RXDS_EN_OFFSET      18U
#define CTRLR0_XIP_DFS_HC_OFFSET       19U
#define CTRLR0_XIP_INST_EN_OFFSET      20U
#define CTRLR0_XIP_CONT_EN_OFFSET      21U
#define CTRLR0_XIP_MBL_OFFSET          26U
#define CTRLR0_WAIT_CYCLES_OFFSET      11U
#define CTRLR0_INST_L_OFFSET           8U
#define CTRLR0_ADDR_L_OFFSET           2U
#define CTRLR0_TRANS_TYPE_OFFSET       0U

#define CTRLR0_INST_L_8bit             0x2U

#define TRANS_TYPE_STANDARD            0U
#define TRANS_TYPE_FRF_DEFINED         2U

/* ─── XIP_CTRL bit fields ─── */

#define XIP_CTRL_FRF_OFFSET             0U
#define XIP_CTRL_TRANS_TYPE_OFFSET      2U
#define XIP_CTRL_ADDR_L_OFFSET          4U
#define XIP_CTRL_INST_L_OFFSET          9U
#define XIP_CTRL_MD_BITS_EN_OFFSET      12U
#define XIP_CTRL_WAIT_CYCLES_OFFSET     13U
#define XIP_CTRL_DFC_HC_OFFSET          18U
#define XIP_CTRL_DDR_EN_OFFSET          19U
#define XIP_CTRL_INST_DDR_EN_OFFSET     20U
#define XIP_CTRL_RXDS_EN_OFFSET         21U
#define XIP_CTRL_INST_EN_OFFSET         22U
#define XIP_CTRL_CONT_XFER_EN_OFFSET   23U
#define XIP_CTRL_HYPERBUS_EN_OFFSET     24U
#define XIP_CTRL_RXDS_SIG_EN           25U
#define XIP_CTRL_XIP_MBL_OFFSET        26U
#define XIP_PREFETCH_EN_OFFSET          29U
#define XIP_CTRL_RXDS_VL_EN_OFFSET     30U

/* ─── Status register ─── */

#define SR_BUSY                         (1U << 0)
#define SR_TF_NOT_FULL                  (1U << 1)
#define SR_TF_EMPTY                     (1U << 2)
#define SR_RF_NOT_EMPT                  (1U << 3)
#define SR_RF_FULL                      (1U << 4)

/* ─── AES_CONTROL fields ─── */

#define AES_CONTROL_XIP_EN             (1U << 4)
#define AES_CONTROL_DECRYPT_EN         (1U << 0)

/* ─── Address length ─── */

#define ADDR_LENGTH_0_BITS              0x0
#define ADDR_LENGTH_8_BITS              0x2
#define ADDR_LENGTH_24_BITS             0x6
#define ADDR_LENGTH_32_BITS             0x8

#define ADDR_L32bit                     0x8
#define INST_L8bit                      0x2

/* ─── ISSI flash commands ─── */

#define ISSI_READ_ID                    0x9E
#define ISSI_WRITE_ENABLE               0x06
#define ISSI_READ_STATUS_REG            0x05
#define ISSI_READ_VOLATILE_CONFIG_REG   0x85
#define ISSI_READ_NONVOLATILE_CONFIG_REG 0xB5
#define ISSI_WRITE_VOLATILE_CONFIG_REG  0x81
#define ISSI_DDR_OCTAL_IO_FAST_READ     0xFD
#define ISSI_4BYTE_PAGE_PROGRAM         0x12
#define ISSI_4BYTE_SECTOR_ERASE         0xDC
#define ISSI_RESET_ENABLE               0x66
#define ISSI_RESET_MEMORY               0x99

#define DEFAULT_WAIT_CYCLES_ISSI        0x10
#define DEVICE_ID_ISSI_FLASH_IS25WX256  0x9D
#define WRAP_32_BYTE                    0xFD
#define OCTAL_DDR_DQS                   0xE7

#define OSPI_SECTOR_SIZE                0x10000  /* 64KB */
#define OSPI_PAGE_SIZE                  256

/* ─── Pad control ─── */

#define PAD_CTRL_REN                    0x01
#define PAD_CTRL_SMT                    0x02
#define PAD_CTRL_SR                     0x04
#define PAD_CTRL_12MA                   (3U << 5)

/* ─── Register access macros ─── */

#define ospi_readl(a, r)                ((a)->regs->r)
#define ospi_writel(a, r, v)            ((a)->regs->r = (v))

#define spi_enable(cfg)                 ospi_writel(cfg, ssienr, 1)
#define spi_disable(cfg)                ospi_writel(cfg, ssienr, 0)
#define spi_set_clk(cfg, div)           ospi_writel(cfg, baudr, div)

/* ─── Driver API ─── */

void ospi_init(ospi_flash_cfg_t *ospi_cfg);
void ospi_setup_read(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len,
                     uint32_t read_len, uint32_t wait_cycles);
void ospi_setup_read_sdr(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len,
                         uint32_t read_len, uint32_t wait_cycles);
void ospi_setup_write(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len);
void ospi_setup_write_sdr(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len);
void ospi_setup_write_ddr16(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len);
void ospi_push(ospi_flash_cfg_t *ospi_cfg, uint32_t data);
void ospi_send(ospi_flash_cfg_t *ospi_cfg, uint32_t data);
void ospi_recv(ospi_flash_cfg_t *ospi_cfg, uint32_t command, uint8_t *buffer);
void ospi_xip_enter(ospi_flash_cfg_t *ospi_cfg, uint16_t incr_command,
                    uint16_t wrap_command);
void ospi_xip_exit(ospi_flash_cfg_t *ospi_cfg, uint16_t incr_command,
                   uint16_t wrap_command);

#endif /* OSPI_DRV_H */
