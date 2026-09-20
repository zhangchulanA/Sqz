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
BIN_INSTALL_DIR="/usr/local/bin"
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
echo "可执行目录: $BIN_INSTALL_DIR"

# 1. 收集所有子目录下的 .h 文件（保留目录结构），跳过根目录的 .h，忽略 test、sqzgen 目录
echo "收集头文件（仅子目录，保留目录结构，忽略 test、sqzgen 目录）..."
cd "$PRO_PWD"
find . -mindepth 2 -name "*.h" -type f \
    ! -path "./test/*" ! -path "*/test/*" \
    ! -path "./sqzgen/*" ! -path "*/sqzgen/*" | while read header; do
    header_clean="${header#./}"
    target_dir="$WORK_DIR/$PACKAGE_NAME/$(dirname "$header_clean")"
    mkdir -p "$target_dir"
    cp "$header" "$target_dir/"
done

# 2. 收集所有子目录（用于生成 pri 文件的 INCLUDEPATH），忽略 test、sqzgen 目录
echo "收集目录结构（用于生成 pri，忽略 test、sqzgen 目录）..."
INCLUDE_DIRS=$(find "$WORK_DIR/$PACKAGE_NAME" -type d \
    ! -path "*/test*" ! -path "*/test" \
    ! -path "*/sqzgen*" ! -path "*/sqzgen" \
    | sed "s|$WORK_DIR/$PACKAGE_NAME||" | grep -v "^$" | sort -u)

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

# 5. 检查并复制 sqzgen 可执行文件（仅从项目目录下的 sqzgen/sqzgen）
echo "检查 sqzgen 可执行文件..."
SQZ_CODEGEN_SRC="$PRO_PWD/sqzgen/sqzgen"

if [ -f "$SQZ_CODEGEN_SRC" ] && [ -x "$SQZ_CODEGEN_SRC" ]; then
    echo "找到 sqzgen 可执行文件: $SQZ_CODEGEN_SRC"
    cp "$SQZ_CODEGEN_SRC" "$WORK_DIR/sqzgen"
    chmod +x "$WORK_DIR/sqzgen"
    echo "已复制 sqzgen 到打包目录"
else
    echo "未找到 sqzgen 可执行文件（$SQZ_CODEGEN_SRC），跳过"
fi

# 6. 生成 install.sh（使用占位符 __VERSION__）
echo "生成 install.sh..."
cat > "$WORK_DIR/install.sh" << 'INSTALL_EOF'
#!/bin/bash

# ========== 版本信息 ==========
SQZ_VERSION="__VERSION__"
# ==============================

# ========== 可配置的系统路径（从环境变量读取） ==========
PREFIX=${PREFIX:-/usr}
HEADER_INSTALL_DIR="${PREFIX}/include/Sqz"
LIB_INSTALL_DIR="${PREFIX}/lib/Sqz"
BIN_INSTALL_DIR="/usr/local/bin"
# ===================================================

echo "=========================================="
echo "安装 Sqz 到系统目录"
echo "安装前缀: $PREFIX"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "可执行: $BIN_INSTALL_DIR"
echo "版本: ${SQZ_VERSION}"
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

sudo mkdir -p "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR" "$BIN_INSTALL_DIR"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 安装头文件（保留完整目录结构，排除 SqzLib、test、sqzgen）
echo "安装头文件（保留目录结构，排除 SqzLib、test、sqzgen）..."
cd "$SCRIPT_DIR/Sqz"
if command -v rsync &> /dev/null; then
    rsync -av --exclude="SqzLib" --exclude="test" --exclude="sqzgen" ./* "$HEADER_INSTALL_DIR/" 2>/dev/null || true
else
    find . -mindepth 1 \
        ! -path "./SqzLib" ! -path "./SqzLib/*" \
        ! -path "./test" ! -path "./test/*" \
        ! -path "./sqzgen" ! -path "./sqzgen/*" \
        -exec cp -r {} "$HEADER_INSTALL_DIR/" \; 2>/dev/null || true
    tar -cf - --exclude="SqzLib" --exclude="test" --exclude="sqzgen" . | (cd "$HEADER_INSTALL_DIR" && tar -xf -)
fi
echo "头文件安装完成（已排除 SqzLib、test、sqzgen）"

# 安装库文件
echo "安装库文件到 $LIB_INSTALL_DIR ..."
if [ -d "$SCRIPT_DIR/Sqz/SqzLib" ] && [ "$(ls -A "$SCRIPT_DIR/Sqz/SqzLib" 2>/dev/null)" ]; then
    sudo cp -r "$SCRIPT_DIR/Sqz/SqzLib"/* "$LIB_INSTALL_DIR/" 2>/dev/null || true
    echo "库文件安装完成"
else
    echo "警告: 未找到库文件，跳过库安装"
fi

# 安装 sqzgen（如果存在）
if [ -f "$SCRIPT_DIR/sqzgen" ]; then
    echo "安装 sqzgen 到 $BIN_INSTALL_DIR ..."
    sudo cp "$SCRIPT_DIR/sqzgen" "$BIN_INSTALL_DIR/sqzgen"
    sudo chmod 755 "$BIN_INSTALL_DIR/sqzgen"

    # 设置所有者为当前用户（如果是 sudo 执行，则使用 SUDO_USER）
    OWNER="${SUDO_USER:-$USER}"
    if [ -n "$OWNER" ] && [ "$OWNER" != "root" ]; then
        sudo chown "$OWNER" "$BIN_INSTALL_DIR/sqzgen" 2>/dev/null || true
    fi
    echo "sqzgen 安装完成，所有者: ${OWNER:-root}"
else
    echo "未找到 sqzgen，跳过安装"
fi

# 更新系统库缓存
echo "配置系统库搜索路径..."
echo "$LIB_INSTALL_DIR" | sudo tee /etc/ld.so.conf.d/sqz.conf > /dev/null 2>/dev/null || echo "警告: 无法写入 /etc/ld.so.conf.d/sqz.conf（可能需要root权限）"
sudo ldconfig 2>/dev/null || echo "警告: ldconfig 执行失败（可能需要root权限）"

# 写入环境变量
if ! grep -q "# Sqz" ~/.bashrc 2>/dev/null; then
    echo "" >> ~/.bashrc
    echo "# Sqz" >> ~/.bashrc
    echo "export LD_LIBRARY_PATH=$LIB_INSTALL_DIR:\$LD_LIBRARY_PATH" >> ~/.bashrc
    echo "已添加 LD_LIBRARY_PATH 到 ~/.bashrc"
fi

# 权限
sudo chmod -R 755 "$HEADER_INSTALL_DIR" "$LIB_INSTALL_DIR" 2>/dev/null || true

# ========== 为每个 .h 文件创建同名无后缀文件 ==========
echo "为头文件创建同名无后缀引用文件..."

find "$HEADER_INSTALL_DIR" -type f -name "*.h" \
    ! -path "*/SqzLib/*" ! -path "*/test/*" ! -path "*/sqzgen/*" | while read header_file; do
    header_dir=$(dirname "$header_file")
    header_basename=$(basename "$header_file" .h)
    link_file="$header_dir/$header_basename"

    if [ -f "$link_file" ] && [ ! -L "$link_file" ]; then
        echo "  警告: $link_file 已存在，跳过创建"
        continue
    fi

    echo "#include \"$header_basename.h\"" | sudo tee "$link_file" > /dev/null
    sudo chmod 644 "$link_file"
done

echo "头文件引用文件创建完成"
# ====================================================

# ========== 生成 Sqz.pri 文件 ==========
echo "生成 Sqz.pri 配置文件..."
PRI_FILE="$HEADER_INSTALL_DIR/Sqz.pri"

ALL_DIRS=$(find "$HEADER_INSTALL_DIR" -mindepth 1 -type d \
    ! -path "*/SqzLib*" ! -path "*/SqzLib" \
    ! -path "*/test*" ! -path "*/test" \
    ! -path "*/sqzgen*" ! -path "*/sqzgen" \
    | sed "s|$HEADER_INSTALL_DIR||" | grep -v "^$" | sort -u)

cat > "/tmp/Sqz.pri" << 'PRI_EOF'
# Sqz.pri
# 系统Sqz公共库

# 本地源码模块路径
SRC_ROOT = /usr/include/Sqz

PRI_EOF

echo "INCLUDEPATH += \\" >> "/tmp/Sqz.pri"

DIRS_ARRAY=()
while IFS= read -r dir; do
    DIRS_ARRAY+=("$dir")
done <<< "$ALL_DIRS"

for i in "${!DIRS_ARRAY[@]}"; do
    dir="${DIRS_ARRAY[$i]}"
    dir_escaped=$(echo "$dir" | sed 's/ /\\ /g')
    if [ $i -eq $((${#DIRS_ARRAY[@]} - 1)) ]; then
        echo "                \$\$SRC_ROOT$dir_escaped" >> "/tmp/Sqz.pri"
    else
        echo "                \$\$SRC_ROOT$dir_escaped \\" >> "/tmp/Sqz.pri"
    fi
done

cat >> "/tmp/Sqz.pri" << 'PRI_EOF'

# 库文件路径（仅当库存在时添加）
LIBS += -L/usr/lib/Sqz/

exists(/usr/lib/Sqz/libSqz.so) {
    LIBS += -lSqz
} else: exists(/usr/lib/Sqz/libSqz.a) {
    LIBS += -lSqz
} else {
    message("Warning: Sqz library not found in /usr/lib/Sqz/")
}

PRI_EOF

sudo cp "/tmp/Sqz.pri" "$PRI_FILE"
rm -f "/tmp/Sqz.pri"

echo "已生成 Sqz.pri: $PRI_FILE"

# ========================================

# ========== 创建版本查看命令 ==========
echo "创建 sqz-version 命令..."

sudo mkdir -p /etc
sudo cat > /etc/sqz_version << VER_EOF
Package: Sqz
Version: ${SQZ_VERSION}
Build Date: $(date +%Y%m%d_%H%M%S)
System: $(uname -s)
Install Prefix: ${PREFIX}
Header Dir: ${HEADER_INSTALL_DIR}
Lib Dir: ${LIB_INSTALL_DIR}
VER_EOF

sudo cat > /usr/local/bin/sqz-version << 'CMD_EOF'
#!/bin/bash
if [ -f /etc/sqz_version ]; then
    echo "=== Sqz 版本信息 ==="
    echo ""
    cat /etc/sqz_version
    echo ""
else
    echo "错误: 未找到 Sqz 版本信息"
    echo "请确认 Sqz 已正确安装"
    exit 1
fi
CMD_EOF

sudo chmod +x /usr/local/bin/sqz-version

echo "版本信息已保存到 /etc/sqz_version"
echo "版本查看命令: sqz-version"
# ========================================

# ========== 创建卸载命令 ==========
echo "创建 sqz-uninstall 命令..."

sudo cat > /usr/local/bin/sqz-uninstall << 'UNINSTALL_CMD_EOF'
#!/bin/bash
# Sqz 卸载命令
PREFIX=${PREFIX:-/usr}
HEADER_INSTALL_DIR="${PREFIX}/include/Sqz"
LIB_INSTALL_DIR="${PREFIX}/lib/Sqz"
BIN_INSTALL_DIR="/usr/local/bin"

echo "=========================================="
echo "卸载 Sqz"
echo "=========================================="

if [ -d "$HEADER_INSTALL_DIR" ]; then
    echo "删除头文件目录: $HEADER_INSTALL_DIR"
    sudo rm -rf "$HEADER_INSTALL_DIR"
fi

if [ -d "$LIB_INSTALL_DIR" ]; then
    echo "删除库文件目录: $LIB_INSTALL_DIR"
    sudo rm -rf "$LIB_INSTALL_DIR"
fi

for f in "sqzgen" "sqz-version" "sqz-uninstall"; do
    if [ -f "$BIN_INSTALL_DIR/$f" ]; then
        echo "删除可执行文件: $BIN_INSTALL_DIR/$f"
        sudo rm -f "$BIN_INSTALL_DIR/$f"
    fi
done

if [ -f "/etc/sqz_version" ]; then
    echo "删除版本信息: /etc/sqz_version"
    sudo rm -f "/etc/sqz_version"
fi

if [ -f "/etc/ld.so.conf.d/sqz.conf" ]; then
    echo "删除库路径配置: /etc/ld.so.conf.d/sqz.conf"
    sudo rm -f "/etc/ld.so.conf.d/sqz.conf"
fi

sudo ldconfig 2>/dev/null || true

if grep -q "# Sqz" ~/.bashrc 2>/dev/null; then
    echo "清理 ~/.bashrc 中的 Sqz 配置..."
    sed -i '/^# Sqz$/,+1d' ~/.bashrc 2>/dev/null || true
fi

echo ""
echo "=========================================="
echo "Sqz 卸载完成"
echo "=========================================="
UNINSTALL_CMD_EOF

sudo chmod +x /usr/local/bin/sqz-uninstall
echo "卸载命令: sqz-uninstall"
# ========================================

echo "=========================================="
echo "安装完成！"
echo "头文件: $HEADER_INSTALL_DIR"
echo "库文件: $LIB_INSTALL_DIR"
echo "Pri文件: $PRI_FILE"
echo ""
echo "使用方式：在 .pro 文件中添加"
echo "  include(/usr/include/Sqz/Sqz.pri)"
echo ""
echo "查看版本: sqz-version"
echo "卸载 Sqz: sqz-uninstall"
echo "=========================================="
INSTALL_EOF

# 替换占位符为实际版本号
sed -i "s/__VERSION__/${VERSION}/g" "$WORK_DIR/install.sh"

chmod +x "$WORK_DIR/install.sh"

# 7. 直接构建自解压run
echo "创建 .run 自解压包..."
cd "$WORK_DIR"
cat > "$RUN_FILE" << 'RUN_EOF'
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
rm -rf Sqz/ install.sh sqzgen
exit 0
__ARCHIVE_BELOW__
RUN_EOF

tar -czf - Sqz/ install.sh sqzgen 2>/dev/null >> "$RUN_FILE" || tar -czf - Sqz/ install.sh >> "$RUN_FILE"
chmod +x "$RUN_FILE"

# 清理
rm -rf "$WORK_DIR"
sync

echo "========== 打包完成 =========="
echo "仅输出自解压安装包:"
ls -lh "$RUN_FILE"
echo ""
echo "安装方法: sudo ./$RUN_FILE"
echo ""
echo "卸载方法: 安装后执行 sudo sqz-uninstall"
echo ""
echo "自定义安装路径: PREFIX=/opt sudo ./$RUN_FILE"
