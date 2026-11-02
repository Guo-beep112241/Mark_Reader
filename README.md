# Mark Reader

面向 macOS 的 Qt 6 Widgets 电子书阅读器。支持 TXT/EPUB 导入、按最近阅读排序、封面卡片书架、章节导航、书签、划线批注、全文搜索、复古纸页/夜读主题和阅读位置恢复。

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

Homebrew 将 Qt Svg 单独安装时，需要显式添加搜索路径；本项目的验证命令为：

```sh
xattr -cr "build/Mark Reader.app"
macdeployqt "build/Mark Reader.app" -libpath="$(brew --prefix qtsvg)/lib" -no-codesign
```

`-no-codesign` 适合本地验证；正式分发还需要代码签名和公证。

书籍副本和 `library.json` 保存在 Qt 的应用数据目录；字号和主题偏好保存在系统应用设置中。“同步文件夹”会把指定文件夹（含子文件夹）里的 TXT/EPUB 导入应用，默认来源是 iCloud Drive 的 `本机迁移/Documents/kindle`；右键按钮可更换文件夹。重复点击只会导入新书，书名和正文高度重复的副本会自动跳过或合并，只保留阅读痕迹最完整的一本。该功能不直接连接云服务，也不提供账号认证或后台自动同步。

| 改造前：同步 = 把书导出到文件夹 | 改造后：同步 = 从文件夹导入新书 |
| --- | --- |
| ![改造前的同步](docs/screenshots/sync-before.png) | ![改造后的同步](docs/screenshots/sync-after.png) |

## 代码导览

- `MainWindow` 管理书架/阅读页切换和按钮交互。
- `ReaderWidget` 显示纯文本、提供滚动位置并响应字号设置。
- `EpubDocument` 读取 EPUB 容器、OPF spine、内嵌封面和 XHTML 章节，抽取纯文本供阅读。
- `CoverArt` 生成布面精装封面；“封面文件缺失”会在加载书库时自动补回。
- `Theme` 集中管理纸色、墨色、烫金、宋体字体和全局样式表；`RetroWidgets` 绘制纸张背景与书架封面卡片。
- `LibraryStore` 导入 TXT/EPUB 应用内副本，以原子 JSON 文件保存书架、阅读进度、书签和批注；`importFolder` 从同步文件夹批量导入，并按书名和正文相似度合并重复书籍。
- `tests/test_librarystore.cpp` 验证导入、副本、书架重载、阅读进度、书签批注、EPUB 章节与封面、主题样式、封面卡片、文件夹导入和重复书籍合并。
