FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
# USB gadget config enabled for OSPI boot (kernel fits in 32MB NOR)
SRC_URI += "file://usb-gadget-adb.cfg"
#SRC_URI += "file://display-drm.cfg"

# Force-disable g_serial after config merge — it auto-binds UDC, blocking configfs composite
do_configure_append() {
    sed -i 's/^CONFIG_USB_G_SERIAL=.*/# CONFIG_USB_G_SERIAL is not set/' ${B}/.config
}
