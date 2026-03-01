SUMMARY = "USB CDC-ECM gadget for Ethernet-over-USB developer access"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://usb-ecm.sh \
           file://usb-ecm-init"

S = "${WORKDIR}"

RDEPENDS:${PN} = "busybox"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/usb-ecm.sh ${D}${bindir}/usb-ecm.sh

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/usb-ecm-init ${D}${sysconfdir}/init.d/usb-ecm

    # Create rc.d symlinks directly (update-rc.d may not be in tiny rootfs)
    for rl in 2 3 4 5; do
        install -d ${D}${sysconfdir}/rc${rl}.d
        ln -sf ../init.d/usb-ecm ${D}${sysconfdir}/rc${rl}.d/S90usb-ecm
    done
    for rl in 0 1 6; do
        install -d ${D}${sysconfdir}/rc${rl}.d
        ln -sf ../init.d/usb-ecm ${D}${sysconfdir}/rc${rl}.d/K90usb-ecm
    done
}
