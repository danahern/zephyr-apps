FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
# All config fragments disabled for MRAM-only build (kernel too large)
# Re-enable when switching to OSPI boot
#SRC_URI += "file://usb-gadget-adb.cfg"
#SRC_URI += "file://display-drm.cfg"
