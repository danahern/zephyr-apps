SUMMARY = "OverlayFS writable layer for read-only rootfs (dev builds)"
DESCRIPTION = "Mounts a tmpfs-backed overlayfs over / at boot, making \
the entire filesystem writable. Enables adb push and iterative development \
without reflashing. Changes are volatile (lost on reboot)."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://overlayfs-dev-init"

S = "${WORKDIR}"

RDEPENDS:${PN} = "busybox"

do_install() {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/overlayfs-dev-init ${D}${sysconfdir}/init.d/overlayfs-dev

    # S10 — run early, before services that might write to rootfs
    for rl in 2 3 4 5; do
        install -d ${D}${sysconfdir}/rc${rl}.d
        ln -sf ../init.d/overlayfs-dev ${D}${sysconfdir}/rc${rl}.d/S10overlayfs-dev
    done
}
