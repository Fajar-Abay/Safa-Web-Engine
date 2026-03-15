#!/bin/bash
# ============================================
# Skrip Pembuat Paket Installer .deb
# untuk Safa Web Server
# ============================================

set -e

VERSION="4.0.0"
PKG_NAME="safa-web-server"
PKG_DIR="${PKG_NAME}_${VERSION}"
ARCH="amd64"

echo "=== Building Safa Web Server .deb Package ==="

# 1. Build terlebih dahulu
echo "[1/5] Compiling..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
make -j$(nproc) > /dev/null 2>&1
cd ..

# 2. Buat struktur direktori Debian
echo "[2/5] Creating package structure..."
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/safa-web-server/www"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/doc/safa-web-server"

# 3. Copy binaries dan file-file
echo "[3/5] Copying files..."
cp build/server       "$PKG_DIR/usr/bin/safa-server"
cp build/server_gui   "$PKG_DIR/usr/bin/safa-server-gui"
cp -r www/*           "$PKG_DIR/usr/share/safa-web-server/www/"
cp DOCS.md            "$PKG_DIR/usr/share/doc/safa-web-server/"
cp README.md          "$PKG_DIR/usr/share/doc/safa-web-server/"

# 4. Buat file DEBIAN/control
cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: web
Priority: optional
Architecture: ${ARCH}
Depends: libssl3t64, zlib1g, libsfml-graphics2.6, libsfml-window2.6, libsfml-system2.6
Maintainer: Fajar-Abay <fajar.bayu752@smk.belajar.id>
Description: Safa Web Server - High Performance C++ Web Server
 Web server HTTP/HTTPS berperforma tinggi dengan fitur:
 Epoll event-driven I/O, GZIP compression, Virtual Hosting,
 PHP CGI, SSL/TLS, Range Requests, dan Desktop GUI Dashboard.
Homepage: https://github.com/Fajar-Abay/Safa-Web-Engine
EOF

# 5. Buat file .desktop untuk shortcut GUI
cat > "$PKG_DIR/usr/share/applications/safa-web-server.desktop" << EOF
[Desktop Entry]
Name=Safa Web Server
Comment=High Performance C++ Web Server Dashboard
Exec=safa-server-gui
Terminal=false
Type=Application
Categories=Development;Network;WebDevelopment;
Keywords=web;server;http;https;
EOF

# Post-install script
cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e

# Lokasi standar Document Root Safa
WWW_ROOT="/var/www/safa-web-server"

echo "Mempersiapkan lingkungan Safa Web Server..."

if [ ! -d "$WWW_ROOT" ]; then
    echo "Membuat direktori $WWW_ROOT..."
    mkdir -p "$WWW_ROOT"
    
    # Salin file default jika folder baru dibuat
    cp -r /usr/share/safa-web-server/www/* "$WWW_ROOT/"
fi

# Berikan izin tulis ke grup 'www-data' atau user saat ini agar bisa edit file via GUI
# Kita asumsikan user yang menginstall ingin bisa mengelola foldernya
chown -R $SUDO_USER:$SUDO_USER "$WWW_ROOT" || true
chmod -R 755 "$WWW_ROOT"

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  Safa Web Server berhasil diinstall!     ║"
echo "║                                          ║"
echo "║  CLI:  safa-server -p 8000               ║"
echo "║  GUI:  safa-server-gui                   ║"
echo "║                                          ║"
echo "║  WWW:  $WWW_ROOT (Default)               ║"
echo "║  Doc:  /usr/share/doc/safa-web-server/   ║"
echo "╚══════════════════════════════════════════╝"
echo ""
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

# 6. Build .deb
echo "[4/5] Building .deb package..."
dpkg-deb --build "$PKG_DIR" > /dev/null 2>&1

echo "[5/5] Done!"
echo ""
echo "=== Package created: ${PKG_DIR}.deb ==="
echo "Size: $(du -h ${PKG_DIR}.deb | cut -f1)"
echo ""
echo "Install with:  sudo dpkg -i ${PKG_DIR}.deb"
echo "Remove with:   sudo dpkg -r ${PKG_NAME}"

# Cleanup
rm -rf "$PKG_DIR"
