# 固件配置说明

## 概述

本项目现在支持用户友好的固件配置方式。你不再需要在编译时设置 WiFi 密码，而是可以使用我们提供的 Web 配置工具来个性化固件。

## 🎯 解决的问题

- ✅ **用户友好**：无需编译，直接配置固件
- ✅ **安全性**：WiFi 密码不会暴露在代码仓库中
- ✅ **批量生产**：一个固件模板适用于所有用户
- ✅ **灵活配置**：支持所有参数的个性化设置

## 🚀 使用方法

### 1. 获取固件和配置工具

从 [Releases](../../releases) 页面下载：
- `display-ambient-light-board.bin` - 固件模板文件
- `firmware-config-tool.html` - 配置工具

### 2. 配置固件

1. **打开配置工具**
   - 在浏览器中打开 `firmware-config-tool.html`
   - 支持所有现代浏览器（Chrome、Firefox、Safari、Edge）

2. **上传固件**
   - 拖拽或选择 `display-ambient-light-board.bin` 文件
   - 工具会自动解析当前配置

3. **修改配置**
   - **WiFi 设置**：填写你的 WiFi SSID 和密码
   - **网络设置**：UDP 端口、mDNS 主机名
   - **LED 设置**：引脚、数量、颜色顺序
   - **呼吸效果**：启用/禁用、基础颜色

4. **下载配置后的固件**
   - 点击"更新配置"按钮
   - 点击"下载配置后的固件"
   - 获得个性化的 `display-ambient-light-board-configured.bin`

### 3. 烧录固件

使用 esptool.py 烧录配置后的固件：

```bash
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 display-ambient-light-board-configured.bin
```

## 📋 可配置参数

| 参数 | 说明 | 默认值 | 范围/格式 |
|------|------|--------|-----------|
| **WiFi SSID** | WiFi 网络名称 | TEMPLATE_SSID | 1-63 字符 |
| **WiFi 密码** | WiFi 密码 | TEMPLATE_PASS | 0-63 字符 |
| **UDP 端口** | UDP 服务器端口 | 23042 | 1024-65535 |
| **mDNS 主机名** | 设备网络名称 | board-rs | 1-31 字符 |
| **LED 引脚** | GPIO 引脚号 | 4 | 0-21 |
| **最大 LED 数** | 支持的 LED 数量 | 500 | 1-1000 |
| **LED 颜色顺序** | 颜色通道顺序 | RGBW | RGB/GRB/RGBW 等 |
| **呼吸效果** | 启用呼吸效果 | 启用 | 启用/禁用 |
| **呼吸基础色** | RGBW 基础颜色 | (20,20,50,0) | 0-255 |

## 🔧 技术原理

### 固件配置区域

固件中包含一个 256 字节的配置区域：
- **位置**：固件中的固定位置，由标记 `FWCFG_START` 标识
- **格式**：二进制结构体，包含魔数、版本、配置数据和校验和
- **校验**：CRC32 校验确保数据完整性

### 配置工具工作原理

1. **解析固件**：查找配置标记，读取当前配置
2. **验证数据**：检查魔数、版本和校验和
3. **修改配置**：用户输入新的配置参数
4. **写入固件**：将新配置写入固件的配置区域
5. **计算校验**：重新计算并写入校验和

### 运行时读取

设备启动时：
1. 查找固件中的配置区域
2. 验证配置的有效性
3. 如果配置有效，使用固件配置
4. 如果配置无效，使用编译时默认配置

## 🛠️ 开发者信息

### 构建包含配置的固件

项目现在会自动构建包含配置模板的固件：

```bash
# 正常构建
idf.py build

# 固件会包含默认的配置模板
# 用户可以使用配置工具修改这些配置
```

### 配置数据结构

```c
typedef struct {
    uint32_t magic;                 // 0x12345678
    uint32_t version;               // 配置版本
    char wifi_ssid[64];             // WiFi SSID
    char wifi_password[64];         // WiFi 密码
    uint16_t udp_port;              // UDP 端口
    char mdns_hostname[32];         // mDNS 主机名
    uint8_t led_pin;                // LED 引脚
    uint16_t max_leds;              // 最大 LED 数
    char led_order[8];              // LED 颜色顺序
    // ... 其他配置参数
    uint8_t reserved[48];           // 预留空间
    uint32_t checksum;              // CRC32 校验和
} firmware_config_t;
```

## 🔍 故障排除

### 配置工具无法识别固件

- **原因**：固件可能不包含配置区域
- **解决**：确保使用最新版本的固件

### 设备无法连接 WiFi

- **检查**：确认 WiFi SSID 和密码正确
- **重新配置**：使用配置工具重新设置 WiFi 参数

### 配置丢失

- **原因**：固件配置区域可能损坏
- **解决**：重新使用配置工具配置固件并烧录

## 📝 更新日志

### v1.0.0
- ✅ 实现固件配置区域
- ✅ 创建 Web 配置工具
- ✅ 支持运行时配置读取
- ✅ 更新 CI/CD 构建流程

## 🤝 贡献

如果你发现问题或有改进建议，欢迎：
1. 提交 Issue
2. 创建 Pull Request
3. 参与讨论

## 📄 许可证

本项目采用 GPLv3 许可证。详见 [LICENSE](LICENSE) 文件。
