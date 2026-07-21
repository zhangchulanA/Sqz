#!/bin/bash
# ============================================================================
# 脚本名称: MakeRun_2.sh
# 功能: 打包 Sqz 库，生成 .tar.gz 和 .run 安装包
#      安装后所有头文件平铺于 /usr/include/Sqz，库文件置于 /usr/lib/Sqz
#      安装时会自动删除旧版本目录
# 使用: 在 .pro 文件中添加 QMAKE_POST_LINK += $$PWD/MakeRun_2.sh $$VERSION $$PWD
# ============================================================================

set -e

VERSION=$1
PRO_PWD=$2

if [ -z "$VERSION" ] || [ -z "$PRO_PWD" ]; then
    echo "错误: 用法 $0 <版本号> <源码目录>"
    exit 1
fi

PACKAGE_NAME="Sqz"
SYS_NAME=$(uname -s)
TIMESTAMP=$(date +%Y%m%d)
BASE_NAME="${PACKAGE_NAME}_${SYS_NAME}_v${VERSION}"

OUTPUT_DIR="$PRO_PWD/../Packages"
TAR_FILE="$OUTPUT_DIR/${BASE_NAME}.tar.gz"
RUN_FILE="$OUTPUT_DIR/${BASE_NAME}.run"
WORK_DIR="$PRO_PWD/package_work"

mkdir -p "$OUTPUT_DIR"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/$PACKAGE_NAME"

echo "========== 开始打包 Sqz =========="
echo "源码目录: $PRO_PWD"
echo "版本号: $VERSION"
echo "输出目录: $OUTPUT_DIR"
echo "包名: $BASE_NAME"

# 1. 收集所有 .h 文件（平铺到 Sqz 根目录，不保留子目录结构）
echo "收集头文件（平铺）..."
cd "$PRO_PWD"
find . -mindepth 2 -name "*.h" -type f | while read header; do
    cp "$header" "$WORK_DIR/$PACKAGE_NAME/"
done

# 2. 复制 SqzLib 全部文件（库文件）
echo "收集库文件..."
if [ -d "$PRO_PWD/SqzLib" ]; then
    mkdir -p "$WORK_DIR/$PACKAGE_NAME/SqzLib"
    cp -r "$PRO_PWD/SqzLib"/* "$WORK_DIR/$PACKAGE_NAME/SqzLib/" 2>/dev/null || true
    echo "已复制 SqzLib 内容"
else
    echo "错误: 找不到 SqzLib 目录"
    exit 1
fi

# 3. 生成 install.sh（安装脚本，含删除旧版本逻辑）
echo "生成 install.sh..."
cat > "$WORK_DIR/install.sh" << 'EOF'
#!/bin/bash
# Sqz 安装脚本 - 平铺头文件到 /usr/include/Sqz，库文件到 /usr/lib/Sqz
# 安装前会删除旧版本的所有文件

HEADER_INSTALL_DIR="/usr/include/Sqz"
LIB_INSTALL_DIR="/usr/lib/Sqz"

echo "=========================================="
echo "安装 Sqz 到系统目录"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "=========================================="

# 删除旧的头文件目录（确保完全清理）
if [ -d "$HEADER_INSTALL_DIR" ]; then
    echo "检测到旧版本头文件，正在删除..."
    sudo rm -rf "$HEADER_INSTALL_DIR"
fi

# 删除旧的库目录（确保完全清理）
if [ -d "$LIB_INSTALL_DIR" ]; then
    echo "检测到旧版本库文件，正在删除..."
    sudo rm -rf "$LIB_INSTALL_DIR"
fi

# 重新创建目录
sudo mkdir -p "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "安装头文件（平铺）..."
cd "$SCRIPT_DIR/Sqz"
# 复制所有 .h 文件到目标目录（不保留子目录）
find . -maxdepth 1 -name "*.h" -type f | while read header; do
    sudo cp "$header" "$HEADER_INSTALL_DIR/"
done

echo "安装库文件..."
sudo cp -r "$SCRIPT_DIR/Sqz/SqzLib"/* "$LIB_INSTALL_DIR/" 2>/dev/null || true

echo "配置系统库搜索路径..."
echo "$LIB_INSTALL_DIR" | sudo tee /etc/ld.so.conf.d/sqz.conf > /dev/null
sudo ldconfig

# 添加到 ~/.bashrc（如果尚未添加）
if ! grep -q "Sqz" ~/.bashrc 2>/dev/null; then
    echo "" >> ~/.bashrc
    echo "# Sqz" >> ~/.bashrc
    echo "export LD_LIBRARY_PATH=$LIB_INSTALL_DIR:\$LD_LIBRARY_PATH" >> ~/.bashrc
    echo "已添加 LD_LIBRARY_PATH 到 ~/.bashrc"
fi

# 设置目录权限（所有用户可读可执行，但不可写）
echo "设置目录权限..."
sudo chmod -R 755 "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR"

echo "=========================================="
echo "安装完成！"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "使用方式：在 .pro 文件中添加"
echo "  INCLUDEPATH += /usr/include/Sqz"
echo "  LIBS += -L/usr/lib/Sqz -lSqz"
echo "=========================================="
EOF

chmod +x "$WORK_DIR/install.sh"

# 4. 打包
echo "创建 tar.gz..."
cd "$WORK_DIR"
tar -czf "$TAR_FILE" Sqz/ install.sh

echo "创建 .run 自解压包..."
cat > "$RUN_FILE" << 'EOF'
#!/bin/bash
ARCHIVE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0;}' "$0")
tail -n +$ARCHIVE "$0" | tar -xzv
if [ -f install.sh ]; then
    chmod +x install.sh
    ./install.sh
else
    echo "错误: install.sh 不存在"
    exit 1
fi
rm -rf Sqz/ install.sh
exit 0
__ARCHIVE_BELOW__
EOF
cat "$TAR_FILE" >> "$RUN_FILE"
chmod +x "$RUN_FILE"

# 5. 清理
rm -rf "$WORK_DIR"

# 6. 同步
sync

echo "========== 打包完成 =========="
echo "输出文件:"
ls -lh "$TAR_FILE" "$RUN_FILE"
echo ""
echo "安装方法: sudo ./$RUN_FILE"
