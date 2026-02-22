SUMMARY = "USB ADB gadget for developer access"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://usb-ecm.sh \
           file://usb-ecm-init"

S = "${WORKDIR}"

RDEPENDS_${PN} = "busybox"
RDEPENDS_${PN}_append_devkit-e8 = " kernel-module-dwc3 kernel-module-dwc3-of-simple kernel-module-libcomposite kernel-module-usb-f-fs"

inherit update-rc.d

INITSCRIPT_NAME = "usb-ecm"
INITSCRIPT_PARAMS = "defaults 90"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/usb-ecm.sh ${D}${bindir}/usb-ecm.sh

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/usb-ecm-init ${D}${sysconfdir}/init.d/usb-ecm
}
