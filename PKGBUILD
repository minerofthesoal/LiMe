# Maintainer: LiMe OS <dev@lime-os.local>

pkgname=lime-cinnamon
pkgver=6.0.0
pkgrel=1
pkgdesc="LiMe Desktop Environment - Cinnamon Fork with AI Integration"
arch=('x86_64')
url="https://github.com/lime-os/lime-cinnamon"
license=('GPL2')

depends=(
    'glib2'
    'gtk3'
    'muffin'
    'clutter'
    'clutter-gtk'
    'python'
    'polkit'
    'upower'
    'accountsservice'
)

makedepends=(
    'meson'
    'ninja'
    'pkg-config'
    'gnome-common'
)

provides=('lime-de')
conflicts=('cinnamon')

source=("${pkgname}-${pkgver}.tar.gz")

build() {
    cd "${pkgname}-${pkgver}"

    meson setup builddir \
        --prefix=/usr \
        --buildtype=release \
        -Dman=false

    meson compile -C builddir
}

package() {
    cd "${pkgname}-${pkgver}"

    DESTDIR="$pkgdir" meson install -C builddir
}
