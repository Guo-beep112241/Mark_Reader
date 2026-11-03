#!/usr/bin/env bash
# 验证 macdeployqt 处理后的应用包：依赖是否可移植、Qt 插件是否齐全、签名是否有效。
set -euo pipefail

bundle="${1:?用法: tests/verify_app_bundle.sh <Mark Reader.app>}"
executable="$bundle/Contents/MacOS/Mark Reader"

if [[ ! -x "$executable" ]]; then
    echo "缺少可执行文件：$executable" >&2
    exit 1
fi
if [[ ! -d "$bundle/Contents/Frameworks" ]]; then
    echo "缺少 Contents/Frameworks，请先运行 macdeployqt。" >&2
    exit 1
fi

status=0
for plugin in platforms/libqcocoa.dylib imageformats/libqjpeg.dylib; do
    if [[ ! -f "$bundle/Contents/PlugIns/$plugin" ]]; then
        echo "缺少 Qt 插件：$plugin" >&2
        status=1
    fi
done

temporary=$(mktemp)
trap 'rm -f "$temporary"' EXIT
find "$bundle/Contents" -type f \( -perm -111 -o -name '*.dylib' \) -print0 > "$temporary"
while IFS= read -r -d '' binary; do
    # dylib 的 otool -L 会把自身 install name 也列出来，它不是运行时依赖。
    install_name=$(otool -D "$binary" 2>/dev/null | tail -n +2 | head -n 1)
    while IFS= read -r dependency; do
        if [[ -z "$dependency" || "$dependency" == "$install_name" ]]; then
            continue
        fi
        case "$dependency" in
            @executable_path/*|@loader_path/*|@rpath/*|/System/*|/usr/lib/*) ;;
            *)
                echo "不可移植的依赖：${dependency}（来自 ${binary#"$bundle"/}）" >&2
                status=1
                ;;
        esac
    done < <(otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}')
done < "$temporary"

if ! codesign --verify --deep --strict "$bundle" >/dev/null 2>&1; then
    echo "代码签名校验失败；本地验证可先执行：" >&2
    echo "  codesign --force --deep --sign - \"$bundle\"" >&2
    status=1
fi

if [[ $status -eq 0 ]]; then
    echo "应用包验证通过：$bundle"
fi
exit $status
