# 🚀 快速开始指南

## 概述

本项目现在支持用户友好的固件配置方式！你不再需要修改代码或重新编译，只需使用我们的 Web 配置工具即可个性化你的环境光板。

## 📥 获取文件

### 方法1：从 Releases 下载（推荐）

1. 访问 [Releases 页面](../../releases)
2. 下载最新版本的以下文件：
   - `display-ambient-light-board.bin` - 固件模板
   - `firmware-config-tool.html` - 配置工具

### 方法2：从开发版本下载

1. 访问 [Actions 页面](../../actions)
2. 选择最新的成功构建
3. 下载 Artifacts 中的文件

## ⚙️ 配置固件

### 1. 打开配置工具

在浏览器中打开 `firmware-config-tool.html` 文件

### 2. 上传固件

- 拖拽 `display-ambient-light-board.bin` 到上传区域
- 或点击"选择固件文件"按钮选择文件

### 3. 配置参数

**WiFi 设置**（必填）：
- WiFi SSID：你的 WiFi 网络名称
- WiFi 密码：你的 WiFi 密码

**网络设置**（可选）：
- UDP 端口：默认 23042
- mDNS 主机名：默认 board-rs（访问地址为 board-rs.local）

**LED 设置**（可选）：
- LED 引脚：默认 GPIO 4
- 最大 LED 数：默认 500
- LED 颜色顺序：默认 RGBW

**呼吸效果**（可选）：
- 启用/禁用呼吸效果
- 基础颜色设置（RGBW）

### 4. 下载配置后的固件

1. 点击"更新配置"按钮
2. 点击"下载配置后的固件"按钮
3. 保存 `display-ambient-light-board-configured.bin` 文件

## 🔥 烧录固件

### 使用 esptool.py

```bash
# 安装 esptool（如果还没安装）
pip install esptool

# 烧录固件
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 display-ambient-light-board-configured.bin
```

### 端口说明

- **Linux**: `/dev/ttyUSB0` 或 `/dev/ttyACM0`
- **macOS**: `/dev/cu.usbserial-*` 或 `/dev/cu.usbmodem*`
- **Windows**: `COM3`、`COM4` 等

## 🌐 使用设备

### 1. 首次启动

设备启动后会：
1. 连接到你配置的 WiFi 网络
2. 启动 UDP 服务器（端口 23042）
3. 启用 mDNS 服务（board-rs.local）
4. 开始呼吸效果（如果启用）

### 2. 发送环境光数据

使用 UDP 协议发送 RGB 数据到设备：

```python
import socket

# 连接到设备
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# 发送 RGB 数据（每个 LED 3字节：R, G, B）
rgb_data = bytes([255, 0, 0] * 10)  # 10个红色 LED
sock.sendto(rgb_data, ('board-rs.local', 23042))
```

### 3. 网络发现

设备支持 mDNS，可以通过主机名访问：
- 默认：`board-rs.local`
- 自定义：`你的主机名.local`

## 🔧 故障排除

### 配置工具无法识别固件

**问题**：上传固件后显示"未找到配置区域"

**解决**：
1. 确保使用的是最新版本的固件
2. 检查文件是否完整下载
3. 尝试重新下载固件文件

### 设备无法连接 WiFi

**问题**：设备启动后无法连接网络

**解决**：
1. 检查 WiFi SSID 和密码是否正确
2. 确认 WiFi 网络是 2.4GHz（ESP32-C3 不支持 5GHz）
3. 检查 WiFi 网络是否有特殊字符
4. 重新配置固件并烧录

### 无法发现设备

**问题**：无法通过 mDNS 找到设备

**解决**：
1. 确认设备已连接到同一网络
2. 尝试使用 IP 地址而不是主机名
3. 检查防火墙设置
4. 使用网络扫描工具查找设备 IP

### LED 不亮或颜色错误

**问题**：LED 显示异常

**解决**：
1. 检查 LED 引脚配置是否正确
2. 确认 LED 颜色顺序设置（RGB/GRB/RGBW等）
3. 检查 LED 数量设置
4. 验证硬件连接

## 📋 配置参数说明

| 参数 | 默认值 | 说明 | 范围 |
|------|--------|------|------|
| WiFi SSID | TEMPLATE_SSID | WiFi 网络名称 | 1-63 字符 |
| WiFi 密码 | TEMPLATE_PASS | WiFi 密码 | 0-63 字符 |
| UDP 端口 | 23042 | UDP 服务器端口 | 1024-65535 |
| mDNS 主机名 | board-rs | 设备网络名称 | 1-31 字符 |
| LED 引脚 | 4 | GPIO 引脚号 | 0-21 |
| 最大 LED 数 | 500 | 支持的 LED 数量 | 1-1000 |
| LED 颜色顺序 | RGBW | 颜色通道顺序 | RGB/GRB/RGBW 等 |

## 🎯 下一步

1. **测试连接**：确认设备能正常连接 WiFi
2. **发送数据**：尝试发送 RGB 数据到设备
3. **调整参数**：根据需要重新配置固件
4. **集成应用**：将设备集成到你的环境光系统中

## 💡 提示

- 配置工具完全在浏览器中运行，不会上传任何数据
- 可以保存多个配置版本的固件
- 建议在配置前备份原始固件文件
- 如果配置错误，可以重新配置并烧录

## 🆘 获取帮助

如果遇到问题：
1. 查看 [故障排除](#故障排除) 部分
2. 检查 [Issues](../../issues) 中的已知问题
3. 创建新的 Issue 描述你的问题
