# Mark Reader

面向 macOS 的 Qt 6 Widgets 电子书阅读器。支持 TXT/EPUB 导入、按最近阅读排序、封面卡片书架、从书架移除书籍、章节导航、书签、划线批注、全文搜索、复古纸页/夜读主题和阅读位置恢复。

## 界面与封面

书架以封面卡片展示藏书：EPUB 优先读取书内声明的封面图；TXT 或没有封面的书会按书名生成一张配色稳定的布面精装封面。旧书库中的书在启动时也会自动补齐封面，同步文件夹会把封面一起复制。

![复古书架](docs/screenshots/shelf-after.png)

阅读页使用羊皮纸底色和宋体排版，工具栏、按钮、下拉框、输入框、滚动条都沿用同一套烫金与暗红配色；右上角“夜读/晨读”可在纸页与暖色夜间主题之间切换。

![复古阅读页](docs/screenshots/reader-after.png)

## 构建与运行

需要 Qt 6（Widgets、Test）、libzip 和 CMake 3.21 或更新版本。Homebrew 环境可安装依赖：

```sh
brew install qt cmake libzip
```

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

在 macOS 上，应用构建为 `build/Mark Reader.app`。CMake 生成的应用包仍使用本机 Qt/libzip 运行库；若要把 Qt 运行库和插件一起复制进包（便于在未安装 Qt 的 Mac 上运行），执行：

```sh
macdeployqt "build/Mark Reader.app"
```

Homebrew 把 Qt Svg 作为独立组件安装；本项目只用 PNG/ICNS 图标，不需要 SVG 图标插件。`macdeployqt` 会尝试部署该可选插件并报出 `Cannot resolve rpath ... QtSvg`，删除这两个插件即可得到自包含的应用包：

```sh
xattr -cr "build/Mark Reader.app"
macdeployqt "build/Mark Reader.app" -no-codesign
rm -f "build/Mark Reader.app/Contents/PlugIns/iconengines/libqsvgicon.dylib" \
      "build/Mark Reader.app/Contents/PlugIns/imageformats/libqsvg.dylib"
codesign --force --deep --sign - "build/Mark Reader.app"
./tests/verify_app_bundle.sh "build/Mark Reader.app"
```

`tests/verify_app_bundle.sh` 会检查可执行文件、Qt 平台/图片插件、签名，以及所有动态库依赖是否已经改成 `@executable_path`/`@rpath` 或系统路径（不允许残留 `/opt/homebrew` 绝对路径）。`-no-codesign` 适合本地验证；上面的 ad-hoc 签名只能证明包结构完整。正式分发还需要 Developer ID 签名、公证并 stapling：这一步需要 Apple 开发者证书和 `notarytool` 凭据，仓库和本机环境都没有配置，因此尚未实际执行。

书籍副本和 `library.json` 保存在 Qt 的应用数据目录；字号和主题偏好保存在系统应用设置中。“同步文件夹”会把指定文件夹（含子文件夹）里的 TXT/EPUB 导入应用，默认来源是 iCloud Drive 的 `本机迁移/Documents/kindle`；右键按钮可更换文件夹。重复点击只会导入新书，书名和正文高度重复的副本会自动跳过或合并，只保留阅读痕迹最完整的一本；阅读痕迹完全相同时按来源文件修改时间决定保留哪本。来源文件在另一端被改写时，会作为新书并存，不会覆盖已有阅读记录。该功能不直接连接云服务，也不提供账号认证或后台自动同步。

| 改造前：同步 = 把书导出到文件夹 | 改造后：同步 = 从文件夹导入新书 |
| --- | --- |
| ![改造前的同步](docs/screenshots/sync-before.png) | ![改造后的同步](docs/screenshots/sync-after.png) |

## TXT 编码

TXT 目前只接受 UTF-8。GBK/GB18030 等中文旧编码会在导入时被拒绝，并提示先用文本编辑器另存为 UTF-8；没有静默替换字符。EPUB 内部的 OPF/XHTML 按各自的 XML 编码声明读取，封面可以是 EPUB2 的 `meta name="cover"` 或 EPUB3 的 `properties="cover-image"`。

## 从书架移除

选中书架上的书后点击“从书架移除”，确认后会删除应用内副本、封面、书签和批注，并从 `library.json` 移除记录；导入前的原文件不会被删除。若移除的正是当前打开的书，阅读页会清空并停止保存进度。

| 改造前：书架没有移除入口 | 改造后：选中书籍后可“从书架移除” |
| --- | --- |
| ![改造前的书架](docs/screenshots/shelf-remove-before.png) | ![改造后的书架](docs/screenshots/shelf-remove-after.png) |

![移除确认对话框](docs/screenshots/shelf-remove-dialog.png)

## 代码导览

- `MainWindow` 管理书架/阅读页切换和按钮交互。
- `ReaderWidget` 显示纯文本、提供滚动位置并响应字号设置。
- `EpubDocument` 读取 EPUB 容器、OPF spine、内嵌封面和 XHTML 章节，抽取纯文本供阅读；支持嵌套 OPF 路径、百分号编码、EPUB2/EPUB3 封面声明，并跳过空章节或单个损坏章节。
- `CoverArt` 生成布面精装封面；“封面文件缺失”会在加载书库时自动补回。
- `Theme` 集中管理纸色、墨色、烫金、宋体字体和全局样式表；`RetroWidgets` 绘制纸张背景与书架封面卡片。
- `LibraryStore` 导入 TXT/EPUB 应用内副本，以原子 JSON 文件保存书架、阅读进度、书签和批注；`removeBook` 删除书籍记录、副本和封面；`importFolder` 从同步文件夹批量导入，并按书名和正文相似度合并重复书籍。
- `tests/test_librarystore.cpp` 验证导入、副本、书架重载、阅读进度、书签批注、书籍移除、EPUB 章节/封面/边界情况、主题样式、封面卡片、文件夹导入、来源文件修改时间与重复书籍合并。`tests/verify_app_bundle.sh` 用于 macdeployqt 后的应用包验证。
