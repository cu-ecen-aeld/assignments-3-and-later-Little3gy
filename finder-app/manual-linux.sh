#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
	echo "Starting kernel build for ${ARCH}"
	# clean any previous builds
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  mrproper
	# set up default configuration
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
	# build everything 
	make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

if [ ! -e ${OUTDIR}/Image ]
then
	echo "Error: Kernel Image not found in ${OUTDIR}"
	exit 1
fi


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs

mkdir -p bin dev etc home lib lib64 proc sbin tmp sys usr 
mkdir -p usr/bin usr/sbin
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
	make distclean
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

# Check if busybox is installed successfully
ls ${OUTDIR}/rootfs/bin/busybox
ls ${OUTDIR}/rootfs/bin/ls 


echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo $(${CROSS_COMPILE}gcc -print-sysroot)
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
# TODO: Make device nodes
echo "Creating device nodes"
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m  600 ${OUTDIR}/rootfs/dev/console c 5 1

ls -l ${OUTDIR}/rootfs/dev/

# TODO: Clean and build the writer utility
echo "building writer utility"
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

echo "Copying finder-app utilities to rootfs/home"
mkdir -p ${OUTDIR}/rootfs/home

cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home




# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz

echo "Creating initramfs.cpio.gz archive"
# cd into rootfs
cd ${OUTDIR}/rootfs
# create the initram archive 
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ${OUTDIR}/initramfs.cpio.gz

echo "initramfs.cpio.gz created at ${OUTDIR}"

ls -lh ${OUTDIR}/initramfs.cpio.gz
