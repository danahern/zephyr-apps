FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
# USB gadget config enabled for OSPI boot (kernel fits in 32MB NOR)
SRC_URI += "file://usb-gadget-adb.cfg"
#SRC_URI += "file://display-drm.cfg"
