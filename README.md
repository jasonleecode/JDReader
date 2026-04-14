# JDReader — 京东读书桌面客户端

基于 Qt6 + QtWebEngine 的京东读书 Linux 桌面应用，在嵌入式浏览器之上提供原生侧边栏书架、书签、笔记等功能。

![icon](src/resources/icons/app.svg)

## 功能

| 功能 | 说明 |
|------|------|
| 书架侧边栏 | 自动从 [ebooks.jd.com/bookshelf](https://ebooks.jd.com/bookshelf) 抓取书目 |
| 持久登录 | Cookie 跨重启保留，无需反复登录 |
| 本地书签 | 记录当前书 + 章节，侧边栏展示，右键可删除 |
| 本地笔记 | 按书 + 章节保存自由文本笔记（Ctrl+N） |
| 阅读模式 | F11 隐藏所有 UI，仅保留阅读区域 |
| 系统主题 | 自动跟随系统深色 / 浅色模式 |
| 键盘快捷键 | 见下表 |

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+D` | 保存书签 |
| `Ctrl+N` | 添加笔记 |
| `Ctrl+\` | 切换侧边栏 |
| `F11` | 阅读模式 |
| `Ctrl+=` / `Ctrl+-` / `Ctrl+0` | 放大 / 缩小 / 重置缩放 |
| `Alt+←` / `Alt+→` | 后退 / 前进 |

## 依赖

| 依赖 | 版本 |
|------|------|
| Qt | 6.5+ |
| CMake | 3.16+ |
| GCC / Clang | C++17 |

Qt 需包含以下模块：`Widgets` `WebEngineWidgets` `WebChannel` `Sql` `Svg`

## 构建

```bash
git clone https://github.com/jasonleecode/JDReader.git
cd JDReader

# 根据实际 Qt 安装路径修改 CMAKE_PREFIX_PATH
cmake -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.5.3/gcc_64"
cmake --build build -j$(nproc)
```

## 运行

```bash
LD_LIBRARY_PATH=$HOME/Qt/6.5.3/gcc_64/lib ./build/JDReader
```

首次运行会弹出京东读书网页，登录后书架数据自动同步到侧边栏。

> 关闭远程调试（默认开启在 9222 端口）：
> ```bash
> JDREADER_NO_DEVTOOLS=1 ./build/JDReader
> ```

## 数据存储

本地数据存放在 `~/.local/share/JDReader/`：

```
~/.local/share/JDReader/
├── jdreader.db          # SQLite：书目、书签、笔记
└── QtWebEngine/         # 浏览器 Cookie / 缓存（持久登录）
```

## 架构简介

```
MainWindow
├── BookshelfPanel          侧边栏（书架 + 书签）
└── ReaderView              嵌入式浏览器
    ├── QWebEngineProfile   持久 Cookie / UA 设置
    ├── QWebChannel         JS ↔ C++ 双向通信
    ├── WebBridge           暴露给 JS 的 C++ 对象
    └── inject.js / .css    注入页面的脚本与样式
```

inject.js 通过以下策略获取书目 ID：
1. 读取 Vue 3 组件内部状态（`__vueParentComponent`）
2. 拦截书目卡片点击后的 URL 变化

## 已知限制

- 依赖京东读书网页版，页面结构变更时选择器可能需要更新（见 `inject.js` 顶部注释）
- 仅在线阅读，不支持离线缓存
- 暂不支持打包分发（`.deb` / AppImage）

## License

MIT
