# 原版实现核查

基线提交：`319b2a2de3a456f1dbcd1ee9c9244c41f76d30fd`，仓库
https://github.com/enderLuoxh/ClassRollCall 。逐项读取 README、完整 Python 源文件和
三个 tools 构建 / 安装脚本。恢复后的文件树和签名提交均通过 Git SHA-1 校验。

| 原版实现 | 2.0 对应处理 |
|---|---|
| DraggableButton：120×60 逻辑基准、Call 文字、透明工具窗、置顶 | 保留 Call 单按钮交互，缩为 52 DIP 卡片及阴影边距，按规格默认约 50% |
| eventFilter + mouseMoveEvent，主屏范围夹取 | 单一手势状态区分点击、6 DIP 拖动、500 ms 长按；屏幕坐标及显示器工作区 |
| GlassPopup：主屏居中、大字、3000 ms 关闭 | 按按钮所在显示器居中；1 秒滚动、3 秒停留、240 ms 淡出；默认不激活且穿透输入 |
| RippleEffect：20 ms 定时器常驻，即使没有动画 | 删除全屏特效；仅抽取或长按等待期间存在 UI 定时器 |
| reset_round：random.shuffle + 索引 | 保留整轮均衡；C++ Fisher–Yates 和系统随机种子 |
| random.choice：独立随机 | C++ uniform_int_distribution，允许重复 |
| anti_spam：抽取期间禁用按钮，弹窗关闭恢复 | 保留防连点；按钮约 95% 不透明，结果消失后恢复 |
| CSV：每行首个非空、非 # 单元格；只支持 UTF-8-sig | 改为六种导入入口及显式预览；多列表格不得猜首列 |
| QSystemTrayIcon：双击切换显示，右键设置 / 关于 / 退出 | 原生托盘；保留双击与右键；Explorer 重启后重建 |
| settings.json / position.json，主屏绝对像素位置 | 新 INI 及逐行 UTF-8 名单，原子替换；显示器 + 相对 DIP 位置 |
| HKCU Run，“点名器”值，路径加引号，静默修复 | 新 ClassRollCall 值，不干扰旧值；路径变化由用户选择修复 |
| 设置中的渐变、背景图片、字体和全屏动画 | 按本次规格移除复杂皮肤与高成本特效 |
| 全局异常自动重启且退出计数会重置 | 原生边界捕获报错，不循环拉起进程 |
| tools 三脚本：pip / PyInstaller，无其他逻辑 | 保留历史文件；新 scripts/build.ps1 + CMake 原生构建 |

原 LICENSE 保留。新 C++ EXE 不加载 Python / PyQt，也不访问网络。
