[English](README.md)

# FoloToy Usage

[下载固件](https://github.com/cfrs2005/folotoy-usage/releases/latest) · [构建状态](https://github.com/cfrs2005/folotoy-usage/actions/workflows/build.yml) · [MIT 许可](LICENSE)

面向 FoloToy AI Passport 的开源 UsageHub 屏幕端。硬件是 ESP32-C3、
240 × 320 屏幕、8 MB Flash，无 PSRAM。这是独立固件工程，构建不依赖
旁边的故事机目录或私有云端源码。

- 浅色纸底、Claude 红色、Codex 蓝色，中文界面和个人头像。
- 首页：头像、名称、本地年月日和时间，双平台额度大数字、进度条与重置倒计时。
- 第二页：双平台的今日 / 累计 Token 与费用。使用独立的 IBM Plex Mono 数字字体与 Noto Sans SC 中文标签，原生尺寸渲染。
- 首页按与网页相同的规则选择 5 小时和 7 天额度池。
- 上下键切换两页。首页轻按确认切换已用/剩余，长按确认刷新；用量页轻按确认也可刷新。
  不显示底部菜单，每分钟自动拉取，倒计时在设备持续更新。
- 时区默认 UTC，可通过私有 USB `timezone` 命令保存 POSIX TZ 规则，支持夏令时。
- 缺失值显示 `--`。联网失败保留内存中的上次数据；超过三分钟或被云端标记为旧数据，
  显示“数据较旧”。额度和 Token 的新鲜度分开判断。API 等价费用不代表订阅账单。
- 不使用麦克风、TTS 或模型密钥，不包含采集器与云服务实现。

## 构建和安装

安装并激活 ESP-IDF **5.5.3**，在本目录运行：

```sh
./tools/validate.sh
idf.py -p /dev/cu.YOUR_DEVICE flash
python tools/configure.py --port /dev/cu.YOUR_DEVICE
```

配置工具会询问 2.4 GHz Wi-Fi 和只读 Display Token，或一次性**显示设备**配对码。
登录 [UsageHub](https://u.80aj.com) 创建。采集器配对码不适用。
电脑通过校验证书的 HTTPS 兑换配对码，只把只读令牌发给设备。
配置前关闭串口监视器。ESP-IDF 环境已包含 pyserial。

通过 `--origin https://example.com` 可指定兼容服务。设备调用
`GET /v1/dashboard`，使用 Bearer 认证；先同步网络时间，再校验 HTTPS 证书。
拒绝重定向。没有 Internet 或无法完成 NTP 校时的 Wi-Fi 不能更新数据。

凭据保存在板上 NVS，不写入源码或共享固件。NVS 当前未加密，物理读取 Flash
可以取得凭据。转让设备前应撤销 Display Token 并清除 `usage` NVS 命名空间。
不要上传整片 Flash 备份或带配置的 NVS 镜像。


可以先写 Wi-Fi，稍后配对：`python tools/configure.py --port PORT --wifi-only`。
`--wifi-file wifi.local.json` 可读取含 `ssid` 和 `password` 的本机私有文件。
已有设备配对不会被覆盖；未配对设备会先连接 Wi-Fi，并在屏幕提示配置授权。

## 固件兼容

完整产物是 `build/FoloToy-AI-Passport-full.bin`。校验包括镜像偏移、分区 MD5、
3 MB 应用上限、`0x356000` 的设备身份、`0x700000` 的 Recovery，及开机长按上键五秒的恢复入口。
已有设备使用分段 `idf.py flash`，不要运行 `erase-flash`。
小程序安装兼容结构已纳入检查；USB 刷入、云端数据、头像同步、屏幕渲染和多次切页/
切换视角已在设备上检查。小程序实际安装和长期无人值守稳定性尚未验证。
Usage 固件不包含或使用故事机语音资源。

## 开源范围

只发布**本目录**，或 `python3 tools/package_source.py` 生成的源码 ZIP。
父目录有私有备份和带配置的故事机固件，不能整体发布。

公开内容：固件、板级驱动、字体子集、USB 配置工具、测试、构建流程和文档。
私有内容：SaaS、账号数据、配置值、本地构建目录和设备备份。
发布包只包含通用固件，不包含从已配置设备读回的 Flash 镜像。

现有开源采集器在 [UsageHub Open](https://github.com/cfrs2005/usagehub-open)。
按该项目接入说明上报到同一个工作空间，即可由本设备显示。

## 开发和许可

`./tools/validate.sh --static` 运行本机测试，`--firmware` 构建和校验固件。
测试覆盖缺失值与零值、大 Token 数、异常响应、重复平台、配置输入和受保护分区。

板级驱动与恢复入口来自 FoloToy AI Passport，遵循 [MIT 许可](LICENSE)。
新增固件代码也使用 MIT。Noto Sans SC 字体子集遵循
[SIL OFL 1.1](assets/fonts/OFL.txt)。第三方依赖各自保留原许可。
源码包不包含 ESP-IDF 和下载的组件。

## 真机截图与头像同步

`python tools/screenshot.py --port PORT --output screen.png` 通过 USB 获取设备
实际渲染的 LVGL 像素块，不使用效果图，也不在设备分配完整帧缓冲。
只在电脑接收工具连接时使用；USB 写入有超时限制。截图可能包含个人用量数据。

`python tools/sync_profile.py --port PORT --config device.local.json` 读取已配对
云端的头像，缩放为 40 像素，单独保存到 NVS。私有 JSON 包含 `origin` 与只读
`token`。该可选工具需要在电脑虚拟环境安装 Pillow 和 pyserial。
个人头像和令牌不会编入固件。内置字体覆盖 ASCII 和本界面中文；其他中文名称
可能需要补充字形。

用量页的 Token 使用 K/M/B/T/P 简写。低于 1,000 美元的费用保留小数，较大费用
显示四舍五入后的整数美元（或百万）。这些只是显示格式，云端保留完整值。

## 仪表设计

[设计研究与工作提示词](docs/design-prompt.zh_CN.md)记录了本次的信息层级和硬件约束。
首页每个平台内并排展示两类额度，使用分段刻度，并明确区分“数据较旧”与账号状态。
公开包包含两套字体的许可，不包含个人头像或配置。

IBM Plex Mono 数字字体也使用 [SIL OFL 1.1](assets/fonts/IBM-Plex-OFL.txt)。
USB 命令 `{"diagnostics":true}` 只返回当前页面、已用/剩余视角、数据是否到达和
空闲内存，不返回 Wi-Fi、账号数据或令牌。

## 贡献和安全

本机检查见 [CONTRIBUTING.md](CONTRIBUTING.md)，凭据边界和私密报告方式见
[SECURITY.md](SECURITY.md)。打包或推送前运行 `python3 tools/audit_public.py`。
