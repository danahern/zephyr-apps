/*
 * OSPI driver — ported from TF-A ospi_drv.c
 *
 * Adapted for bare-metal M55_HP: uses volatile register access directly,
 * no TF-A headers, no cache operations (TCM not cached).
 */

#include "ospi_drv.h"

/* ─── XIP enable/disable (AES control) ─── */

static void ospi_xip_disable(ospi_flash_cfg_t *ospi_cfg)
{
    ospi_cfg->aes_regs->aes_control &= ~AES_CONTROL_XIP_EN;
}

static void ospi_xip_enable(ospi_flash_cfg_t *ospi_cfg)
{
    ospi_cfg->aes_regs->aes_control |= AES_CONTROL_XIP_EN;
    /* No AES decryption — unencrypted flash */
}

/* ─── Controller init ─── */

void ospi_init(ospi_flash_cfg_t *ospi_cfg)
{
    ospi_xip_disable(ospi_cfg);
    spi_disable(ospi_cfg);
    ospi_writel(ospi_cfg, ser, 0);
    ospi_writel(ospi_cfg, rx_sample_dly, 4);
    ospi_writel(ospi_cfg, txd_drive_edge, 1);
    spi_set_clk(ospi_cfg, (AXI_CLOCK / ospi_cfg->ospi_clock));
    spi_enable(ospi_cfg);
}

/* ─── Setup functions ─── */

void ospi_setup_read(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len,
                     uint32_t read_len, uint32_t wait_cycles)
{
    uint32_t val;

    ospi_writel(ospi_cfg, ser, 0);
    spi_disable(ospi_cfg);

    val = CTRLR0_IS_MST
        | (OCTAL << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_RO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_8bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);
    ospi_writel(ospi_cfg, ctrlr1, read_len - 1);

    if (ospi_cfg->ddr_en) {
        val = TRANS_TYPE_FRF_DEFINED
            | (ospi_cfg->ddr_en << CTRLR0_SPI_DDR_EN_OFFSET)
            | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
            | (addr_len << CTRLR0_ADDR_L_OFFSET)
            | (wait_cycles << CTRLR0_WAIT_CYCLES_OFFSET);
    } else {
        val = TRANS_TYPE_STANDARD
            | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
            | (addr_len << CTRLR0_ADDR_L_OFFSET)
            | (wait_cycles << CTRLR0_WAIT_CYCLES_OFFSET);
    }

    ospi_writel(ospi_cfg, spi_ctrlr0, val);
    ospi_cfg->rx_req = read_len;
    spi_enable(ospi_cfg);
}

void ospi_setup_read_sdr(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len,
                         uint32_t read_len, uint32_t wait_cycles)
{
    uint32_t val;

    ospi_writel(ospi_cfg, ser, 0);
    spi_disable(ospi_cfg);

    val = CTRLR0_IS_MST
        | (SINGLE << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_RO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_8bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);
    ospi_writel(ospi_cfg, ctrlr1, read_len - 1);

    val = TRANS_TYPE_STANDARD
        | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
        | (addr_len << CTRLR0_ADDR_L_OFFSET)
        | (wait_cycles << CTRLR0_WAIT_CYCLES_OFFSET);

    ospi_writel(ospi_cfg, spi_ctrlr0, val);
    ospi_cfg->rx_req = read_len;
    spi_enable(ospi_cfg);
}

void ospi_setup_write(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len)
{
    uint32_t val;

    ospi_writel(ospi_cfg, ser, 0);
    spi_disable(ospi_cfg);

    val = CTRLR0_IS_MST
        | (OCTAL << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_TO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_8bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);
    ospi_writel(ospi_cfg, ctrlr1, 0);

    if (ospi_cfg->ddr_en) {
        val = TRANS_TYPE_FRF_DEFINED
            | (ospi_cfg->ddr_en << CTRLR0_SPI_DDR_EN_OFFSET)
            | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
            | (addr_len << CTRLR0_ADDR_L_OFFSET);
    } else {
        val = TRANS_TYPE_STANDARD
            | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
            | (addr_len << CTRLR0_ADDR_L_OFFSET);
    }

    ospi_writel(ospi_cfg, spi_ctrlr0, val);
    spi_enable(ospi_cfg);
}

void ospi_setup_write_sdr(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len)
{
    uint32_t val;

    spi_disable(ospi_cfg);
    ospi_writel(ospi_cfg, ser, 0);

    val = CTRLR0_IS_MST
        | (SINGLE << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_TO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_8bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);
    ospi_writel(ospi_cfg, ctrlr1, 0);

    val = TRANS_TYPE_FRF_DEFINED
        | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
        | (addr_len << CTRLR0_ADDR_L_OFFSET);

    ospi_writel(ospi_cfg, spi_ctrlr0, val);
    spi_enable(ospi_cfg);
}

void ospi_setup_write_ddr16(ospi_flash_cfg_t *ospi_cfg, uint32_t addr_len)
{
    uint32_t val;

    ospi_writel(ospi_cfg, ser, 0);
    spi_disable(ospi_cfg);

    val = CTRLR0_IS_MST
        | (OCTAL << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_TO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_16bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);
    ospi_writel(ospi_cfg, ctrlr1, 0);

    val = TRANS_TYPE_FRF_DEFINED
        | (ospi_cfg->ddr_en << CTRLR0_SPI_DDR_EN_OFFSET)
        | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
        | (addr_len << CTRLR0_ADDR_L_OFFSET);

    ospi_writel(ospi_cfg, spi_ctrlr0, val);
    spi_enable(ospi_cfg);
}

/* ─── Data transfer ─── */

void ospi_push(ospi_flash_cfg_t *ospi_cfg, uint32_t data)
{
    ospi_writel(ospi_cfg, data_reg, data);
}

void ospi_send(ospi_flash_cfg_t *ospi_cfg, uint32_t data)
{
    ospi_writel(ospi_cfg, data_reg, data);
    ospi_writel(ospi_cfg, ser, ospi_cfg->ser);

    while ((ospi_readl(ospi_cfg, sr) & (SR_TF_EMPTY | SR_BUSY)) != SR_TF_EMPTY)
        ;
}

void ospi_recv(ospi_flash_cfg_t *ospi_cfg, uint32_t command, uint8_t *buffer)
{
    uint32_t val;

    ospi_writel(ospi_cfg, data_reg, command);
    ospi_writel(ospi_cfg, ser, ospi_cfg->ser);

    ospi_cfg->rx_cnt = 0;

    while (ospi_cfg->rx_cnt < ospi_cfg->rx_req) {
        while (ospi_readl(ospi_cfg, rxflr) > 0) {
            val = ospi_readl(ospi_cfg, data_reg);
            *buffer++ = (uint8_t)val;
            ospi_cfg->rx_cnt++;
        }
    }
}

/* ─── XIP enter/exit ─── */

void ospi_xip_enter(ospi_flash_cfg_t *ospi_cfg, uint16_t incr_command,
                    uint16_t wrap_command)
{
    uint32_t val;

    spi_disable(ospi_cfg);

    val = CTRLR0_IS_MST
        | (OCTAL << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_RO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_16bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);

    val = (OCTAL << XIP_CTRL_FRF_OFFSET)
        | (0x2 << XIP_CTRL_TRANS_TYPE_OFFSET)
        | (ADDR_L32bit << XIP_CTRL_ADDR_L_OFFSET)
        | (INST_L8bit << XIP_CTRL_INST_L_OFFSET)
        | (0x10 << XIP_CTRL_WAIT_CYCLES_OFFSET)
        | (0x1 << XIP_CTRL_DFC_HC_OFFSET)
        | (0x1 << XIP_CTRL_DDR_EN_OFFSET)
        | (0x1 << XIP_CTRL_RXDS_EN_OFFSET)
        | (0x1 << XIP_CTRL_INST_EN_OFFSET);

    ospi_writel(ospi_cfg, xip_ctrl, val);

    ospi_writel(ospi_cfg, rx_sample_dly, 0);
    ospi_cfg->aes_regs->aes_rxds_delay = 11;

    ospi_writel(ospi_cfg, xip_mode_bits, 0x0);
    ospi_writel(ospi_cfg, xip_incr_inst, incr_command);
    ospi_writel(ospi_cfg, xip_wrap_inst, wrap_command);
    ospi_writel(ospi_cfg, xip_ser, ospi_cfg->ser);

    spi_enable(ospi_cfg);
    ospi_xip_enable(ospi_cfg);
}

void ospi_xip_exit(ospi_flash_cfg_t *ospi_cfg, uint16_t incr_command,
                   uint16_t wrap_command)
{
    uint32_t val;

    spi_disable(ospi_cfg);

    val = CTRLR0_IS_MST
        | (OCTAL << CTRLR0_SPI_FRF_OFFSET)
        | (TMOD_RO << CTRLR0_TMOD_OFFSET)
        | (CTRLR0_DFS_32bit << CTRLR0_DFS_OFFSET);

    ospi_writel(ospi_cfg, ctrlr0, val);

    val = TRANS_TYPE_FRF_DEFINED
        | (ospi_cfg->ddr_en << CTRLR0_SPI_DDR_EN_OFFSET)
        | (2 << CTRLR0_XIP_MBL_OFFSET)
        | (1 << CTRLR0_XIP_DFS_HC_OFFSET)
        | (1 << CTRLR0_XIP_INST_EN_OFFSET)
        | (CTRLR0_INST_L_8bit << CTRLR0_INST_L_OFFSET)
        | (ospi_cfg->addrlen << CTRLR0_ADDR_L_OFFSET)
        | (ospi_cfg->wait_cycles << CTRLR0_WAIT_CYCLES_OFFSET);

    ospi_writel(ospi_cfg, spi_ctrlr0, val);

    ospi_writel(ospi_cfg, xip_mode_bits, 0x1);
    ospi_writel(ospi_cfg, xip_incr_inst, incr_command);
    ospi_writel(ospi_cfg, xip_wrap_inst, wrap_command);
    ospi_writel(ospi_cfg, xip_ser, ospi_cfg->ser);
    ospi_writel(ospi_cfg, ser, ospi_cfg->ser);
    ospi_writel(ospi_cfg, xip_cnt_time_out, 100);

    spi_enable(ospi_cfg);
    ospi_xip_enable(ospi_cfg);
    ospi_xip_disable(ospi_cfg);
}
