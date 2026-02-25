/*
 * OSPI flash operations — ported from TF-A norflash_ospi_setup.c
 *
 * Adapted for bare-metal M55_HP: volatile register access, no TF-A debug
 * macros, no cache operations (TCM not cached).
 *
 * Key rules from knowledge base:
 * - DFS=16 for ALL DDR Octal operations
 * - Data packed as 16-bit LE frames
 * - Software reset (0x66 + 0x99) after programming before XIP re-enter
 */

#include "ospi_flash.h"
#include "ospi_drv.h"

/* Pad control helpers — volatile register writes */
#define PAD_CTRL_DATA (PAD_CTRL_12MA | PAD_CTRL_SR | PAD_CTRL_REN)
#define PAD_CTRL_CLK  (PAD_CTRL_12MA | PAD_CTRL_SR)

#define GPIO_INTMASK_OFFSET     0x34
#define GPIO_SWPORTA_DR_OFFSET  0x0
#define GPIO_SWPORTA_DDR_OFFSET 0x4
#define OSPI_RESET_PIN          7

static ospi_flash_cfg_t ospi_cfg;

/* ─── Pinmux setup ─── */

static void write_padctrl(uint32_t port, uint32_t pin,
                          uint32_t pad_val, uint32_t alt_func)
{
    volatile uint32_t *reg = (volatile uint32_t *)
        (PINMUX_BASE + (port * 32 + pin * 4));
    *reg = (pad_val << 16) | alt_func;
}

static void setup_pinmux(void)
{
    /* OSPI1 data lines */
    write_padctrl(9, 5, PAD_CTRL_DATA, 1);
    write_padctrl(9, 6, PAD_CTRL_DATA, 1);
    write_padctrl(9, 7, PAD_CTRL_DATA, 1);
    write_padctrl(10, 0, PAD_CTRL_DATA, 1);
    write_padctrl(10, 1, PAD_CTRL_DATA, 1);
    write_padctrl(10, 2, PAD_CTRL_DATA, 1);
    write_padctrl(10, 3, PAD_CTRL_DATA, 1);
    write_padctrl(10, 4, PAD_CTRL_DATA, 1);
    write_padctrl(10, 7, PAD_CTRL_DATA, 1);   /* DQS */
    write_padctrl(5, 5, PAD_CTRL_CLK, 1);     /* CLK */
    write_padctrl(5, 7, PAD_CTRL_12MA, 1);    /* CS */
    write_padctrl(5, 6, PAD_CTRL_DATA, 1);
    write_padctrl(8, 0, PAD_CTRL_12MA, 1);

    /* GPIO reset toggle */
    volatile uint32_t *gpio_base = (volatile uint32_t *)LPGPIO_BASE;
    uint32_t val;

    /* Mask interrupt */
    val = gpio_base[GPIO_INTMASK_OFFSET / 4];
    gpio_base[GPIO_INTMASK_OFFSET / 4] = val | (1U << OSPI_RESET_PIN);

    /* Set direction = output */
    val = gpio_base[GPIO_SWPORTA_DDR_OFFSET / 4];
    gpio_base[GPIO_SWPORTA_DDR_OFFSET / 4] = val | (1U << OSPI_RESET_PIN);

    /* Pulse low then high */
    val = gpio_base[GPIO_SWPORTA_DR_OFFSET / 4];
    gpio_base[GPIO_SWPORTA_DR_OFFSET / 4] = val & ~(1U << OSPI_RESET_PIN);
    gpio_base[GPIO_SWPORTA_DR_OFFSET / 4] = val | (1U << OSPI_RESET_PIN);
}

/* ─── Flash probe (SPI single mode) ─── */

static uint8_t issi_decode_id(uint8_t *buffer)
{
    uint8_t id = 0;
    /* In octal mode, 1 byte from flash distributed over 8 read bytes */
    for (int i = 0; i < 8; i++) {
        if (buffer[i] & 0x2)
            id |= 1;
        if (i < 7)
            id <<= 1;
    }
    return id;
}

static uint8_t issi_flash_read_id(void)
{
    uint8_t buffer[8];
    ospi_setup_read(&ospi_cfg, ADDR_LENGTH_0_BITS, 8, 0);
    ospi_recv(&ospi_cfg, ISSI_READ_ID, buffer);
    ospi_cfg.device_id = issi_decode_id(buffer);
    return ospi_cfg.device_id;
}

static void issi_write_enable(void)
{
    ospi_setup_write(&ospi_cfg, ADDR_LENGTH_0_BITS);
    ospi_send(&ospi_cfg, ISSI_WRITE_ENABLE);
}

/* ─── SDR configuration register access (used during probe) ─── */

static void issi_set_config_reg_sdr(uint8_t cmd, uint8_t address, uint8_t value)
{
    issi_write_enable();
    ospi_setup_write_sdr(&ospi_cfg, ADDR_LENGTH_24_BITS);
    ospi_push(&ospi_cfg, cmd);
    ospi_push(&ospi_cfg, 0x00);
    ospi_push(&ospi_cfg, 0x00);
    ospi_push(&ospi_cfg, address);
    ospi_send(&ospi_cfg, value);
}

static int issi_flash_probe(void)
{
    if (issi_flash_read_id() == DEVICE_ID_ISSI_FLASH_IS25WX256) {
        /* Set wrap configuration to 32 bytes */
        issi_set_config_reg_sdr(ISSI_WRITE_VOLATILE_CONFIG_REG, 0x07, WRAP_32_BYTE);
        /* Switch to Octal DDR mode */
        issi_set_config_reg_sdr(ISSI_WRITE_VOLATILE_CONFIG_REG, 0x00, OCTAL_DDR_DQS);
        return 0;
    }
    return -1;
}

/* ─── DDR Octal mode operations ─── */

static void ospi_write_en_ddr16(void)
{
    ospi_setup_write_ddr16(&ospi_cfg, ADDR_LENGTH_0_BITS);
    ospi_send(&ospi_cfg, ISSI_WRITE_ENABLE);
}

static void issi_set_config_reg_ddr(uint8_t cmd, uint8_t address, uint8_t value)
{
    ospi_write_en_ddr16();
    ospi_setup_write(&ospi_cfg, ADDR_LENGTH_32_BITS);
    ospi_push(&ospi_cfg, cmd);
    ospi_push(&ospi_cfg, address);
    ospi_push(&ospi_cfg, value);
    ospi_send(&ospi_cfg, value);
}

static uint32_t issi_read_config_reg_ddr(uint32_t cmd)
{
    uint8_t rbuf[4];
    ospi_setup_read(&ospi_cfg, ADDR_LENGTH_32_BITS, 1, 8);
    ospi_push(&ospi_cfg, ISSI_READ_VOLATILE_CONFIG_REG);
    ospi_recv(&ospi_cfg, cmd, rbuf);
    return (uint32_t)rbuf[0];
}

/* Read Status Register in DDR Octal mode. Bit 0 = WIP. */
static uint8_t issi_read_status(void)
{
    uint8_t sr;
    ospi_setup_read(&ospi_cfg, ADDR_LENGTH_0_BITS, 1, 8);
    ospi_recv(&ospi_cfg, ISSI_READ_STATUS_REG, &sr);
    return sr;
}

static int issi_wait_erase(void)
{
    /* IS25WX256 sector erase typical: 100ms, max: 2000ms */
    uint32_t timeout = 2000000;
    while (timeout--) {
        if (!(issi_read_status() & 0x01))
            return 0;
    }
    return -1;
}

static int issi_wait_program(void)
{
    /* IS25WX256 page program typical: 0.3ms, max: 5ms */
    uint32_t timeout = 200000;
    while (timeout--) {
        if (!(issi_read_status() & 0x01))
            return 0;
    }
    return -1;
}

/* ─── Public API ─── */

void ospi_flash_init(void)
{
    ospi_cfg.regs = (ssi_regs_t *)OSPI1_BASE;
    ospi_cfg.aes_regs = (aes_regs_t *)AES1_BASE;
    ospi_cfg.xip_base = (volatile void *)OSPI1_XIP_BASE;
    ospi_cfg.ser = 1;
    ospi_cfg.addrlen = ADDR_LENGTH_32_BITS;
    ospi_cfg.ospi_clock = OSPI_CLOCK;
    ospi_cfg.ddr_en = 0;
    ospi_cfg.wait_cycles = DEFAULT_WAIT_CYCLES_ISSI;

    setup_pinmux();
    ospi_init(&ospi_cfg);

    if (issi_flash_probe() == 0) {
        ospi_cfg.ddr_en = 1;

        /* Ensure wait cycles are set correctly in DDR mode */
        if (issi_read_config_reg_ddr(0x01) != ospi_cfg.wait_cycles) {
            issi_set_config_reg_ddr(ISSI_WRITE_VOLATILE_CONFIG_REG, 0x01,
                                    ospi_cfg.wait_cycles);
        }

        /* Set wrap config to 64 bytes for XIP */
        if (issi_read_config_reg_ddr(0x07) != 0xFE) {
            issi_set_config_reg_ddr(ISSI_WRITE_VOLATILE_CONFIG_REG, 0x07, 0xFE);
        }
    }
    /* If probe fails, we stay in SPI single mode.
     * host can detect via read_id response. */
}

uint8_t ospi_flash_read_id(void)
{
    return ospi_cfg.device_id;
}

int ospi_flash_erase_sector(uint32_t addr)
{
    ospi_write_en_ddr16();
    ospi_setup_write_ddr16(&ospi_cfg, ADDR_LENGTH_32_BITS);
    ospi_push(&ospi_cfg, ISSI_4BYTE_SECTOR_ERASE);
    ospi_send(&ospi_cfg, addr);
    return issi_wait_erase();
}

int ospi_flash_program_page(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t frames;

    if (len == 0 || len > OSPI_PAGE_SIZE)
        return -1;

    ospi_write_en_ddr16();
    ospi_setup_write_ddr16(&ospi_cfg, ADDR_LENGTH_32_BITS);
    ospi_push(&ospi_cfg, ISSI_4BYTE_PAGE_PROGRAM);
    ospi_push(&ospi_cfg, addr);

    /* Pack data as 16-bit LE frames for DDR Octal */
    frames = (len + 1) / 2;
    for (uint32_t i = 0; i + 1 < frames; i++)
        ospi_push(&ospi_cfg, data[i * 2] | (data[i * 2 + 1] << 8));

    /* Last frame */
    if (len & 1)
        ospi_send(&ospi_cfg, data[len - 1]);
    else
        ospi_send(&ospi_cfg, data[len - 2] | (data[len - 1] << 8));

    return issi_wait_program();
}

void ospi_flash_enter_xip(void)
{
    ospi_xip_enter(&ospi_cfg, ISSI_DDR_OCTAL_IO_FAST_READ,
                   ISSI_DDR_OCTAL_IO_FAST_READ);

    /* Clear AES decrypt for unencrypted flash */
    ospi_cfg.aes_regs->aes_control &= ~AES_CONTROL_DECRYPT_EN;
}

void ospi_flash_exit_xip(void)
{
    ospi_xip_exit(&ospi_cfg, ISSI_DDR_OCTAL_IO_FAST_READ,
                  ISSI_DDR_OCTAL_IO_FAST_READ);

    /* Re-init controller for command mode */
    ospi_init(&ospi_cfg);
}

void ospi_flash_software_reset(void)
{
    ospi_setup_write_ddr16(&ospi_cfg, ADDR_LENGTH_0_BITS);
    ospi_send(&ospi_cfg, ISSI_RESET_ENABLE);
    ospi_setup_write_ddr16(&ospi_cfg, ADDR_LENGTH_0_BITS);
    ospi_send(&ospi_cfg, ISSI_RESET_MEMORY);

    /* Reset takes up to 30us per datasheet. ddr_en=0 for next init cycle. */
    ospi_cfg.ddr_en = 0;
}
