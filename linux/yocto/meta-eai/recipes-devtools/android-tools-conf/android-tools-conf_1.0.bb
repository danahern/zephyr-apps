SUMMARY = "Android tools configuration - composite gadget setup for SysVinit"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# This is a virtual/conf provider — the actual gadget setup is in usb-ecm recipe
PROVIDES += "android-tools-conf"
RPROVIDES_${PN} = "android-tools-conf"
ALLOW_EMPTY_${PN} = "1"
