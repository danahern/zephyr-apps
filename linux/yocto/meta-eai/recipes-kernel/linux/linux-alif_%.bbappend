FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://usb-gadget-adb.cfg"
SRC_URI += "file://overlayfs.cfg"
SRC_URI += "file://jffs2.cfg"
SRC_URI += "file://phram-ospi.cfg"
#SRC_URI += "file://display-drm.cfg"

# Generate a patched DTB for OSPI phram boot alongside the standard one.
# The standard DTB keeps the original MRAM bootargs (used by linux-boot-e8.json).
# devkit-e8-ospi-phram.dtb has phram bootargs for OSPI cramfs at 0xC0000000.
OSPI_PHRAM_BOOTARGS = "console=ttyS0,115200n8 root=mtd:ospi_root rootfstype=cramfs ro loglevel=9 phram=ospi_root,0xC0000000,0x2F0000"

do_deploy:append() {
    local src_dtb="${DEPLOYDIR}/devkit-e8.dtb"
    local ospi_dtb="${DEPLOYDIR}/devkit-e8-ospi-phram.dtb"
    if [ -f "${src_dtb}" ]; then
        cp "${src_dtb}" "${ospi_dtb}"
        fdtput -t s "${ospi_dtb}" /chosen bootargs "${OSPI_PHRAM_BOOTARGS}"
        bbdebug 1 "Generated ${ospi_dtb} with phram OSPI bootargs"
    fi
}
