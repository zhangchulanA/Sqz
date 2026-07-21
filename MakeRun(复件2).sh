#!/bin/bash
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

# 1. 收集所有 .h 文件（平铺）
echo "收集头文件（平铺）..."
cd "$PRO_PWD"
find . -mindepth 2 -name "*.h" -type f | while read header; do
    cp "$header" "$WORK_DIR/$PACKAGE_NAME/"
done

# 2. 复制 SqzLib【修复：保留 SqzLib 文件夹】
echo "收集库文件..."
if [ -d "$PRO_PWD/SqzLib" ]; then
    # 目标：WORK_DIR/Sqz/SqzLib
    mkdir -p "$WORK_DIR/$PACKAGE_NAME/SqzLib"
    cp -r "$PRO_PWD/SqzLib"/* "$WORK_DIR/$PACKAGE_NAME/SqzLib/" 2>/dev/null || true
    echo "已复制 SqzLib 完整目录"
else
    echo "错误: 找不到 SqzLib 目录"
    exit 1
fi

# 3. 生成 install.sh
echo "生成 install.sh..."
cat > "$WORK_DIR/install.sh" << 'EOF'
#!/bin/bash
HEADER_INSTALL_DIR="/usr/include/Sqz"
LIB_INSTALL_DIR="/usr/lib/Sqz"
echo "=========================================="
echo "安装 Sqz 到系统目录"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "=========================================="
# 清理旧文件
if [ -d "$HEADER_INSTALL_DIR" ]; then
    echo "检测到旧版本头文件，正在删除..."
    sudo rm -rf "$HEADER_INSTALL_DIR"
fi
if [ -d "$LIB_INSTALL_DIR" ]; then
    echo "检测到旧版本库文件，正在删除..."
    sudo rm -rf "$LIB_INSTALL_DIR"
fi
sudo mkdir -p "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 安装头文件
echo "安装头文件（平铺）..."
cd "$SCRIPT_DIR/Sqz"
find . -maxdepth 1 -name "*.h" -type f | while read header; do
    sudo cp "$header" "$HEADER_INSTALL_DIR/"
done

# 【关键修复】正确复制 SqzLib 内库文件
echo "安装库文件到 /usr/lib/Sqz ..."
sudo cp -r "$SCRIPT_DIR/Sqz/SqzLib"/* "$LIB_INSTALL_DIR/" 2>/dev/null || true

# 更新系统库缓存
echo "配置系统库搜索路径..."
echo "$LIB_INSTALL_DIR" | sudo tee /etc/ld.so.conf.d/sqz.conf > /dev/null
sudo ldconfig

# 写入环境变量
if ! grep -q "Sqz" ~/.bashrc 2>/dev/null; then
    echo "" >> ~/.bashrc
    echo "# Sqz" >> ~/.bashrc
    echo "export LD_LIBRARY_PATH=$LIB_INSTALL_DIR:\$LD_LIBRARY_PATH" >> ~/.bashrc
    echo "已添加 LD_LIBRARY_PATH 到 ~/.bashrc"
fi

# 权限
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

# 4. 直接构建自解压run，不生成tar.gz
echo "创建 .run 自解压包..."
cd "$WORK_DIR"
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
tar -czf - Sqz/ install.sh >> "$RUN_FILE"
chmod +x "$RUN_FILE"

# 清理
rm -rf "$WORK_DIR"
sync

echo "========== 打包完成 =========="
echo "仅输出自解压安装包:"
ls -lh "$RUN_FILE"
echo ""
echo "安装方法: sudo ./$RUN_FILE"
