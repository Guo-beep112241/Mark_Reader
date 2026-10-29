# Mark Reader

面向 macOS 的 Qt 6 Widgets 电子书阅读器。支持 TXT/EPUB 导入、按最近阅读排序、查看阅读百分比、章节导航、书签、划线批注、全文搜索、深色主题和阅读位置恢复。

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

书籍副本和 `library.json` 保存在 Qt 的应用数据目录；字号和主题偏好保存在系统应用设置中。同步按钮会把书籍与元数据合并到用户选定的文件夹；可选择 iCloud Drive 等已由系统同步的目录，在另一台设备选择同一目录即可手动同步。该功能不直接连接云服务，也不提供账号认证或后台自动同步。

## 代码导览

- `MainWindow` 管理书架/阅读页切换和按钮交互。
- `ReaderWidget` 显示纯文本、提供滚动位置并响应字号设置。
- `EpubDocument` 读取 EPUB 容器、OPF spine 和 XHTML 章节，抽取纯文本供阅读。
- `LibraryStore` 导入 TXT/EPUB 应用内副本，以原子 JSON 文件保存书架、阅读进度、书签和批注，并实现同步文件夹合并。
- `tests/test_librarystore.cpp` 验证导入、副本、书架重载、阅读进度、书签批注、EPUB 章节和同步合并。
