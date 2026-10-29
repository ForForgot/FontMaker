# 架构说明

## 目标

把原来的 VS2005/MFC 单体程序拆成可测试的核心、明确的 Windows 平台层和轻量 UI/CLI。核心算法不依赖 Win32，UI 不直接操作 GDI 位图。

## 分层

```text
+-----------------------------+
|  FontMaker.exe (Win32 UI)   |
|  fontmaker_cli.exe          |
+--------------+--------------+
               |
+--------------v--------------+
|  platform/windows           |
|  GdiGlyphRasterizer         |
|  FontCatalog / Win32 utils  |
+--------------+--------------+
               |
+--------------v--------------+
|  core (portable C++20)      |
|  Charset / packing / writer |
+-----------------------------+
```

### core

- `charset`：UTF-8 / UTF-16LE 解码，`.cst` 文件加载
- `glyph`：`Glyph` 和 `GlyphRasterizer` 接口
- `packing`：扫描、位序、可变宽度前缀、空/溢出/overhang 统计和输出上限
- `writer`：C 源码和 BIN 输出
- `metadata` / `sha256`：可复现性 sidecar 和 payload 哈希
- `io` / `utf` / `error`：基础设施

核心只依赖 C++ 标准库。`tests/` 用假渲染器测试取模和输出，不需要 Windows/GDI。

### platform/windows

- `GdiGlyphRasterizer`：32 位 top-down DIB，四周增加至少 4px 探测边带（随偏移扩展），`TextOutW` 渲染后区分真实裁剪像素与单元内像素
- `enumerateFontFamilies`：`EnumFontFamiliesExW`，排除 `@` 竖排字体
- `win32_utils`：UTF-8/UTF-16 转换、文件对话框、错误文本、窗口辅助

### ui

- `MainWindow`：主对话框、状态、控件事件、预览、导出
- `CharsetEditorDialog`：自定义码表编辑、去重、控制字符过滤和实时计数
- `AboutDialog`：版本和署名
- `ui_settings`：用户级设置持久化（`HKCU\Software\FontMaker\2.0`）
- `resources.rc`：纯 Win32 对话框资源和 manifest

### cli

`fontmaker_cli` 复用同一套核心和 GDI 渲染器，用于脚本化生成和集成测试。

## 数据流

```text
Charset (.cst / UTF-8)
        |
        v
GlyphRasterizer.rasterize(codepoint)
        |
        v
Glyph { width, height, advance, coverage }
        |
        v
packGlyph(options)
        |
        v
EncodedFont { recordSize, glyphs[] }
        |
        +--> toCSource()
        +--> toBinary()
```

## 所有权和错误处理

- GDI 对象由 `GdiGlyphRasterizer` 的 RAII 析构函数释放；构造失败会先 `release()` 再重新抛出
- `HBITMAP` / `HFONT` 在替换时先选回旧对象再删除
- 核心使用异常报告错误；UI 在对话框过程和保存路径中捕获并显示
- 字符集加载失败会清空当前字符集，而不是保留半加载状态
- 文件对话框返回完整路径；文件写入使用同目录临时文件 + 原子替换
- 请求字体不存在、字号超过点阵高度、全空输出都会告警；真实裁剪由边带探测报告，CLI 可用 `--strict-font` / `--allow-empty` 控制

## 初始化顺序

旧版依赖 `EN_CHANGE` 副作用完成初始化，存在未初始化成员风险。新版在 `onInitDialog` 中：

1. 建立默认值并设置控件
2. 加载字体列表
3. 创建渲染器
4. 加载字符集
5. 关闭 `initializing_` 守卫
6. 刷新预览

控件事件在 `initializing_ == true` 时不会触发模型更新。

## 扩展点

- **新渲染后端**：实现 `GlyphRasterizer`
- **新输出格式**：在 `writer` 中增加序列化函数，复用 `EncodedFont`
- **新字符集格式**：扩展 `Charset` 的加载函数
- **跨平台核心**：仅编译 `core` + `tests`，UI/CLI 保持 Windows-only

## 测试策略

CTest 运行 `fontmaker_tests`：

- UTF-8/UTF-16 边界和代理对
- `.cst` BOM、奇数字节、代理项来源和临时文件往返
- 字符清理（去重、控制字符）和原子覆盖写
- 横/纵扫描、MSB/LSB、补齐位、空/溢出/overhang 统计
- 可变宽度大端前缀和 SHA-256/JSON 元数据
- C 输出结构、单引号注释和 BIN 拼接
- 渲染器进度回调、异常路径和全空输出保护

UI 通过手工冒烟验证（启动、预览、字符切换、用户码表、关于框）；GDI 输出依赖系统字体，因此不做跨机器逐字节断言。
