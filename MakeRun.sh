#!/bin/bash
set -e
VERSION=$1
PRO_PWD=$2
if [ -z "$VERSION" ] || [ -z "$PRO_PWD" ]; then
    echo "错误: 用法 $0 <版本号> <源码目录>"
    exit 1
fi

# ========== 可配置的系统路径 ==========
PREFIX=${PREFIX:-/usr}
HEADER_INSTALL_DIR="${PREFIX}/include/Sqz"
LIB_INSTALL_DIR="${PREFIX}/lib/Sqz"
# =====================================

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
echo "安装前缀: $PREFIX"
echo "头文件目录: $HEADER_INSTALL_DIR"
echo "库文件目录: $LIB_INSTALL_DIR"

# 1. 收集所有子目录下的 .h 文件（保留目录结构），跳过根目录的 .h
echo "收集头文件（仅子目录，保留目录结构）..."
cd "$PRO_PWD"
find . -mindepth 2 -name "*.h" -type f | while read header; do
    header_clean="${header#./}"
    target_dir="$WORK_DIR/$PACKAGE_NAME/$(dirname "$header_clean")"
    mkdir -p "$target_dir"
    cp "$header" "$target_dir/"
done

# 2. 收集所有子目录（用于生成 pri 文件的 INCLUDEPATH）
echo "收集目录结构（用于生成 pri）..."
INCLUDE_DIRS=$(find "$WORK_DIR/$PACKAGE_NAME" -type d | sed "s|$WORK_DIR/$PACKAGE_NAME||" | grep -v "^$" | sort -u)

# 3. 复制 README.md 到 Sqz 目录下
echo "复制 README.md..."
if [ -f "$PRO_PWD/README.md" ]; then
    cp "$PRO_PWD/README.md" "$WORK_DIR/$PACKAGE_NAME/"
    echo "已复制 README.md"
else
    echo "警告: 找不到 README.md"
fi

# 4. 复制 SqzLib（带检查）
echo "收集库文件..."
if [ -d "$PRO_PWD/SqzLib" ]; then
    LIB_COUNT=$(find "$PRO_PWD/SqzLib" -type f \( -name "*.so*" -o -name "*.a" -o -name "*.dylib" -o -name "*.dll" \) 2>/dev/null | wc -l)
    if [ "$LIB_COUNT" -eq 0 ]; then
        echo "警告: SqzLib 目录下未找到库文件（.so/.a/.dylib/.dll）"
        echo "      打包将继续，但安装时可能缺少库文件"
    else
        echo "找到 $LIB_COUNT 个库文件"
    fi

    mkdir -p "$WORK_DIR/$PACKAGE_NAME/SqzLib"
    cp -r "$PRO_PWD/SqzLib"/* "$WORK_DIR/$PACKAGE_NAME/SqzLib/" 2>/dev/null || true
    echo "已复制 SqzLib 完整目录"
else
    echo "错误: 找不到 SqzLib 目录"
    exit 1
fi

# 5. 生成 install.sh
echo "生成 install.sh..."
cat > "$WORK_DIR/install.sh" << "EOF"
#!/bin/bash

# ========== 版本号（由 MakeRun.sh 注入） ==========
VERSION="@@VERSION@@"
# =================================================

# ========== 可配置的系统路径（从环境变量读取） ==========
PREFIX=${PREFIX:-/usr}
HEADER_INSTALL_DIR="${PREFIX}/include/Sqz"
LIB_INSTALL_DIR="${PREFIX}/lib/Sqz"
# ===================================================

echo "=========================================="
echo "安装 Sqz 到系统目录"
echo "安装前缀: $PREFIX"
echo "安装 Sqz 版本: $VERSION"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "=========================================="

# 清理旧文件（但保留 Sqz.pri 文件，防止卸载后无法使用）
if [ -d "$HEADER_INSTALL_DIR" ]; then
    echo "检测到旧版本头文件，正在删除..."
    find "$HEADER_INSTALL_DIR" -mindepth 1 -maxdepth 1 ! -name "Sqz.pri" -exec rm -rf {} + 2>/dev/null || true
    echo "已清理旧头文件（保留 Sqz.pri）"
fi

if [ -d "$LIB_INSTALL_DIR" ]; then
    echo "检测到旧版本库文件，正在删除..."
    sudo rm -rf "$LIB_INSTALL_DIR"/*
    echo "已清理旧库文件"
fi

sudo mkdir -p "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 安装头文件（保留完整目录结构，排除 SqzLib）
echo "安装头文件（保留目录结构，排除 SqzLib）..."
cd "$SCRIPT_DIR/Sqz"
if command -v rsync &> /dev/null; then
    rsync -av --exclude="SqzLib" ./* "$HEADER_INSTALL_DIR/" 2>/dev/null || true
else
    tar -cf - --exclude="SqzLib" . | (cd "$HEADER_INSTALL_DIR" && tar -xf -)
fi
echo "头文件安装完成（已排除 SqzLib）"

# 安装库文件
echo "安装库文件到 $LIB_INSTALL_DIR ..."
if [ -d "$SCRIPT_DIR/Sqz/SqzLib" ] && [ "$(ls -A "$SCRIPT_DIR/Sqz/SqzLib" 2>/dev/null)" ]; then
    sudo cp -r "$SCRIPT_DIR/Sqz/SqzLib"/* "$LIB_INSTALL_DIR/" 2>/dev/null || true
    echo "库文件安装完成"
else
    echo "警告: 未找到库文件，跳过库安装"
fi

# 更新系统库缓存
echo "配置系统库搜索路径..."
echo "$LIB_INSTALL_DIR" | sudo tee /etc/ld.so.conf.d/sqz.conf > /dev/null 2>/dev/null || echo "警告: 无法写入 /etc/ld.so.conf.d/sqz.conf（可能需要root权限）"
sudo ldconfig 2>/dev/null || echo "警告: ldconfig 执行失败（可能需要root权限）"

# 写入环境变量
if ! grep -q "Sqz" ~/.bashrc 2>/dev/null; then
    echo "" >> ~/.bashrc
    echo "# Sqz" >> ~/.bashrc
    echo "export LD_LIBRARY_PATH=$LIB_INSTALL_DIR:\$LD_LIBRARY_PATH" >> ~/.bashrc
    echo "已添加 LD_LIBRARY_PATH 到 ~/.bashrc"
fi

# 权限
sudo chmod -R 755 "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR" 2>/dev/null || true

# 安装 pkg-config 文件
echo "安装 pkg-config 文件..."
PKGCONFIG_DIR="${PREFIX}/lib/pkgconfig"
sudo mkdir -p "$PKGCONFIG_DIR"
sudo cp "$SCRIPT_DIR/Sqz/sqz.pc" "$PKGCONFIG_DIR/"
echo "已安装 sqz.pc 到 $PKGCONFIG_DIR"

# ========== 生成 Sqz.pri 文件 ==========
echo "生成 Sqz.pri 配置文件..."
PRI_FILE="$HEADER_INSTALL_DIR/Sqz.pri"

# 收集所有子目录（排除 SqzLib 目录及其内容）
ALL_DIRS=$(find "$HEADER_INSTALL_DIR" -mindepth 1 -type d ! -path "*/SqzLib*" ! -path "*/SqzLib" | sed "s|$HEADER_INSTALL_DIR||" | grep -v "^$" | sort -u)

cat > "/tmp/Sqz.pri" << 'PRI_EOF'
# Sqz.pri
# 系统Sqz公共库

# 本地源码模块路径
SRC_ROOT = /usr/include/Sqz

PRI_EOF

# 添加 INCLUDEPATH 条目（排除 SqzLib）
echo "INCLUDEPATH += \\" >> "/tmp/Sqz.pri"

# 收集所有需要包含的目录（排除 SqzLib）
DIRS_ARRAY=()
while IFS= read -r dir; do
    DIRS_ARRAY+=("$dir")
done <<< "$ALL_DIRS"

# 输出目录列表
if [ ${#DIRS_ARRAY[@]} -gt 0 ]; then
    for i in "${!DIRS_ARRAY[@]}"; do
        dir="${DIRS_ARRAY[$i]}"
        dir_escaped=$(echo "$dir" | sed 's/ /\\ /g')
        if [ $i -eq $((${#DIRS_ARRAY[@]} - 1)) ]; then
            echo "                \$\$SRC_ROOT$dir_escaped" >> "/tmp/Sqz.pri"
        else
            echo "                \$\$SRC_ROOT$dir_escaped \\" >> "/tmp/Sqz.pri"
        fi
    done
else
    # 如果没有子目录，不加任何路径
    echo "                \$\$SRC_ROOT" >> "/tmp/Sqz.pri"
fi

cat >> "/tmp/Sqz.pri" << 'PRI_EOF'

# 库文件路径（仅当库存在时添加）
LIBS += -L/usr/lib/Sqz/

# 检查库文件是否存在，如果存在则链接
exists(/usr/lib/Sqz/libSqz.so) {
    LIBS += -lSqz
} else: exists(/usr/lib/Sqz/libSqz.a) {
    LIBS += -lSqz
} else {
    message("Warning: Sqz library not found in /usr/lib/Sqz/")
}

PRI_EOF

# 复制 pri 文件到目标目录
sudo cp "/tmp/Sqz.pri" "$PRI_FILE"
rm -f "/tmp/Sqz.pri"

echo "已生成 Sqz.pri: $PRI_FILE"

# 写入版本文件
echo "$VERSION" | sudo tee "$HEADER_INSTALL_DIR/version.txt" > /dev/null

echo "=========================================="
echo "安装完成！"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "Pri文件: $PRI_FILE"
echo "版本: $VERSION"
echo ""
echo "使用方式：在 .pro 文件中添加"
echo "  include(/usr/include/Sqz/Sqz.pri)"
echo "=========================================="
EOF

# 替换版本号占位符
sed -i "s|@@VERSION@@|${VERSION}|g" "$WORK_DIR/install.sh"
chmod +x "$WORK_DIR/install.sh"

# ========== 生成 uninstall.sh ==========
echo "生成 uninstall.sh..."
cat > "$WORK_DIR/uninstall.sh" << 'EOF'
#!/bin/bash

# ========== 配置 ==========
PREFIX=${PREFIX:-/usr}
HEADER_INSTALL_DIR="${PREFIX}/include/Sqz"
LIB_INSTALL_DIR="${PREFIX}/lib/Sqz"
PKGCONFIG_DIR="${PREFIX}/lib/pkgconfig"
# ==========================

echo "=========================================="
echo "卸载 Sqz"
echo "=========================================="

# 检查是否已安装
if [ ! -d "$HEADER_INSTALL_DIR" ] && [ ! -d "$LIB_INSTALL_DIR" ]; then
    echo "错误: 未检测到 Sqz 安装"
    echo "  头文件目录不存在: $HEADER_INSTALL_DIR"
    echo "  库文件目录不存在: $LIB_INSTALL_DIR"
    exit 1
fi

# 显示当前版本（如果存在）
if [ -f "$HEADER_INSTALL_DIR/version.txt" ]; then
    INSTALLED_VERSION=$(cat "$HEADER_INSTALL_DIR/version.txt")
    echo "当前安装版本: $INSTALLED_VERSION"
else
    echo "当前安装版本: 未知"
fi

echo ""
echo "将删除以下内容:"
[ -d "$HEADER_INSTALL_DIR" ] && echo "  - 头文件: $HEADER_INSTALL_DIR"
[ -d "$LIB_INSTALL_DIR" ] && echo "  - 库文件: $LIB_INSTALL_DIR"
[ -f "$PKGCONFIG_DIR/sqz.pc" ] && echo "  - pkg-config: $PKGCONFIG_DIR/sqz.pc"
[ -f "/etc/ld.so.conf.d/sqz.conf" ] && echo "  - ldconfig: /etc/ld.so.conf.d/sqz.conf"
echo ""

read -p "确认卸载 Sqz? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "取消卸载"
    exit 0
fi

echo "开始卸载..."

# 删除头文件目录
if [ -d "$HEADER_INSTALL_DIR" ]; then
    sudo rm -rf "$HEADER_INSTALL_DIR"
    echo "已删除头文件: $HEADER_INSTALL_DIR"
fi

# 删除库文件目录
if [ -d "$LIB_INSTALL_DIR" ]; then
    sudo rm -rf "$LIB_INSTALL_DIR"
    echo "已删除库文件: $LIB_INSTALL_DIR"
fi

# 删除 pkg-config 文件
if [ -f "$PKGCONFIG_DIR/sqz.pc" ]; then
    sudo rm -f "$PKGCONFIG_DIR/sqz.pc"
    echo "已删除 sqz.pc"
fi

# 删除 ld.so.conf.d 配置
if [ -f "/etc/ld.so.conf.d/sqz.conf" ]; then
    sudo rm -f "/etc/ld.so.conf.d/sqz.conf"
    echo "已删除 ldconfig 配置"
    sudo ldconfig 2>/dev/null || echo "警告: ldconfig 执行失败"
fi

# 清理 .bashrc 中的环境变量（可选）
if grep -q "# Sqz" ~/.bashrc 2>/dev/null; then
    cp ~/.bashrc ~/.bashrc.bak
    sed -i '/# Sqz/d' ~/.bashrc
    sed -i '/export LD_LIBRARY_PATH=.*lib\/Sqz/d' ~/.bashrc
    echo "已从 ~/.bashrc 中移除 Sqz 环境变量（备份已保存为 ~/.bashrc.bak）"
fi

echo ""
echo "=========================================="
echo "卸载完成！"
echo "=========================================="
EOF
chmod +x "$WORK_DIR/uninstall.sh"

# ========== 生成 sqz.pc 文件 ==========
echo "生成 pkg-config 文件 (sqz.pc)..."
cat > "$WORK_DIR/Sqz/sqz.pc" << EOF
prefix=${PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: Sqz
Description: The Sqz library
Version: ${VERSION}
Cflags: -I\${includedir}/Sqz
Libs: -L\${libdir}/Sqz -lSqz
EOF

echo "已生成 sqz.pc 文件"

# 6. 直接构建自解压run
echo "创建 .run 自解压包..."
cd "$WORK_DIR"
cat > "$RUN_FILE" << 'EOF'
#!/bin/bash

# 检查参数
if [ "$1" = "--uninstall" ] || [ "$1" = "-u" ]; then
    echo "启动卸载程序..."
    ARCHIVE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0;}' "$0")
    tail -n +$ARCHIVE "$0" | tar -xzv --wildcards "uninstall.sh" 2>/dev/null
    if [ -f uninstall.sh ]; then
        chmod +x uninstall.sh
        ./uninstall.sh
        rm -f uninstall.sh
    else
        echo "错误: 未找到卸载脚本"
        exit 1
    fi
    exit 0
fi

# 正常安装
ARCHIVE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0;}' "$0")
tail -n +$ARCHIVE "$0" | tar -xzv
if [ -f install.sh ]; then
    chmod +x install.sh
    ./install.sh
else
    echo "错误: install.sh 不存在"
    exit 1
fi
rm -rf Sqz/ install.sh uninstall.sh
exit 0
__ARCHIVE_BELOW__
EOF
tar -czf - Sqz/ install.sh uninstall.sh >> "$RUN_FILE"
chmod +x "$RUN_FILE"

# 清理
rm -rf "$WORK_DIR"
sync

echo "========== 打包完成 =========="
echo "仅输出自解压安装包:"
ls -lh "$RUN_FILE"
echo ""
echo "安装方法: sudo ./$RUN_FILE"
echo "卸载方法: sudo ./$RUN_FILE --uninstall  或  sudo ./$RUN_FILE -u"
echo ""
echo "自定义安装路径: PREFIX=/opt sudo ./$RUN_FILE"
echo "自定义卸载路径: PREFIX=/opt sudo ./$RUN_FILE --uninstall"
