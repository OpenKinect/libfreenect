# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI="5"

inherit cmake-utils git-2 multilib


DESCRIPTION="Core library for accessing the Microsoft Kinect."
HOMEPAGE="https://github.com/OpenKinect/${PN}"
EGIT_REPO_URI="git://github.com/OpenKinect/${PN}.git"
EGIT_COMMIT="v0.2.0"

LICENSE="Apache-2.0 GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="bindist +c_sync +cpp doc examples fakenect opencv python"

COMMON_DEP="
virtual/libusb:1
examples? (	media-libs/freeglut
			virtual/opengl
			x11-libs/libXi 
			x11-libs/libXmu )
opencv? ( media-libs/opencv )
python? ( dev-python/numpy )"

RDEPEND="${COMMON_DEP}"
DEPEND="${COMMON_DEP}
dev-util/cmake
virtual/pkgconfig
doc? ( app-doc/doxygen )
python? ( dev-python/cython )"


src_configure() {
	local mycmakeargs=(
		$(cmake-utils_use_build bindist BUILD_REDIST_PACKAGE)
		$(cmake-utils_use_build c_sync BUILD_C_SYNC)
		$(cmake-utils_use_build cpp BUILD_CPP)
		$(cmake-utils_use_build examples BUILD_EXAMPLES)
		$(cmake-utils_use_build fakenect BUILD_FAKENECT)
		$(cmake-utils_use_build opencv BUILD_CV)
		$(cmake-utils_use_build python BUILD_PYTHON)
	)
	cmake-utils_src_configure
}

src_install() {
	cmake-utils_src_install
	# Rename record example so it does not collide with xawtv
	if use examples; then
		mv "${D}"/usr/bin/record "${D}"/usr/bin/frecord || die
	fi
	
	# udev rules
	insinto /lib/udev/rules.d/
	doins "${S}"/platform/linux/udev/51-kinect.rules

	# documentation
	dodoc HACKING README.asciidoc
	if use doc; then
		cd doc
		doxygen || ewarn "doxygen failed"
		dodoc -r html || ewarn "dodoc failed"
		cd -
	fi
}

pkg_postinst() {
	if use bindist; then
		ewarn "You have enabled audio via the bindist USE flag. Resulting binaries may not be legal to re-distribute."
	fi
	elog "Make sure your user is in the 'video' group"
	elog "Just run 'gpasswd -a <USER> video', then have <USER> re-login."
}
