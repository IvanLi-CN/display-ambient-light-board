# ESP32-C3 氛围灯控制板 C 语言 + ESP-IDF 实现计划

## 项目概述

本项目旨在使用 C 语言和 ESP-IDF 框架重新实现 board-rs 项目的等价功能，构建一个基于 ESP32-C3 的 WiFi 氛围灯控制板。该控制板作为通用 LED 驱动器，接收 UDP 数据包并直接转发到 RGBW LED 灯带。

## 技术规格

### 硬件要求
- **MCU**: ESP32-C3 (RISC-V 单核, 160MHz)
- **开发框架**: ESP-IDF v5.0+
- **编程语言**: C 语言
- **连接性**: WiFi 802.11 b/g/n (2.4GHz)
- **LED 输出**: SK6812 RGBW LED 灯带 (G,R,B,W 通道顺序)
- **数据引脚**: GPIO4
- **电源**: 3.3V 操作，LED 灯带需要 5V 外部供电
- **支持 LED 数量**: 最大 500 个 RGBW LED

### 性能指标
- **数据传输延迟**: < 10ms
- **LED 刷新率**: 30fps (33.33ms 周期)
- **UDP 端口**: 23042
- **mDNS 服务**: `_atmosphere_light._udp.local.`
- **设备主机名**: `board-rs.local.`

## 核心功能模块

### 1. WiFi 网络管理模块 (wifi_manager.c/h)

**功能要求**:
- WiFi 网络连接建立
- DHCP 客户端自动获取 IP 地址
- 连接状态监控和自动重连
- 网络凭据管理

**实现要点**:
- 使用 ESP-IDF WiFi 驱动程序
- 实现异步连接处理
- 支持从环境变量读取 WiFi 凭据
- 连接失败时的重试机制
- DHCP 获取 IP 后触发后续服务启动

**关键 API**:
```c
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char* ssid, const char* password);
bool wifi_manager_is_connected(void);
esp_ip4_addr_t wifi_manager_get_ip(void);
```

### 2. mDNS 服务发现模块 (mdns_service.c/h)

**功能要求**:
- 广播 mDNS 服务信息
- 响应服务发现查询
- 定期发送服务公告
- 等待 DHCP 完成后启动

**实现要点**:
- 使用 ESP-IDF mDNS 组件
- 服务名称: `_atmosphere_light._udp.local.`
- 端口: 23042
- 30 秒周期性公告
- 与 WiFi 状态同步

**关键 API**:
```c
esp_err_t mdns_service_init(void);
esp_err_t mdns_service_start(esp_ip4_addr_t ip_addr);
esp_err_t mdns_service_stop(void);
```

### 3. UDP 通信服务器模块 (udp_server.c/h)

**功能要求**:
- UDP 套接字创建和绑定
- 数据包接收和协议解析
- 连接检查消息处理 (0x01)
- LED 数据包处理 (0x02)

**协议规范**:
```
连接检查包: [0x01]
LED 数据包: [0x02] [偏移高字节] [偏移低字节] [LED数据...]
忽略包: [0x03] [0x04] (完全忽略)
```

**实现要点**:
- 非阻塞 UDP 接收
- 4KB 最大包大小
- 协议头验证
- 偏移量解析 (16位大端序)
- 错误处理和数据验证

**关键 API**:
```c
esp_err_t udp_server_init(uint16_t port);
esp_err_t udp_server_start(void);
esp_err_t udp_server_receive_packet(uint8_t* buffer, size_t* len, uint32_t timeout_ms);
bool udp_server_parse_led_packet(const uint8_t* data, size_t len, uint16_t* offset, uint8_t** led_data, size_t* led_len);
```

### 4. LED 控制驱动模块 (led_driver.c/h)

**功能要求**:
- RMT 外设配置用于 SK6812 时序
- RGBW 数据直接转发
- 大容量 LED 缓冲区管理
- 偏移量数据段更新

**SK6812 时序参数**:
- 1-bit: 6 高电平/6 低电平周期 (10MHz)
- 0-bit: 3 高电平/9 低电平周期 (10MHz)
- 复位脉冲: 800 周期低电平

**实现要点**:
- 使用 ESP-IDF RMT 驱动
- 支持 500 LED 的大容量缓冲区
- 批量传输优化
- 非阻塞传输处理
- 缓冲区偏移量更新机制

**关键 API**:
```c
esp_err_t led_driver_init(gpio_num_t data_pin);
esp_err_t led_driver_update_buffer(uint16_t offset, const uint8_t* data, size_t len);
esp_err_t led_driver_transmit_all(void);
esp_err_t led_driver_set_breathing_effect(bool enable);
```

### 5. 状态机管理模块 (state_machine.c/h)

**状态定义**:
```c
typedef enum {
    STATE_SYSTEM_INIT,
    STATE_WIFI_CONNECTING,
    STATE_DHCP_REQUESTING,
    STATE_NETWORK_READY,
    STATE_UDP_STARTING,
    STATE_UDP_LISTENING,
    STATE_OPERATIONAL,
    STATE_UDP_TIMEOUT,
    STATE_WIFI_ERROR,
    STATE_DHCP_ERROR,
    STATE_UDP_ERROR,
    STATE_RECONNECTING
} system_state_t;
```

**状态转换流程**:
```
SystemInit → WiFiConnecting → DHCPRequesting → NetworkReady → 
UDPStarting → UDPListening (启动mDNS) → Operational
```

**实现要点**:
- 事件驱动状态机
- 超时处理机制
- 错误恢复逻辑
- 0x01 消息监控
- 状态变化日志记录

**关键 API**:
```c
esp_err_t state_machine_init(void);
esp_err_t state_machine_handle_event(system_event_t event);
system_state_t state_machine_get_current_state(void);
```

## 项目结构

```
display-ambient-light-board/
├── main/
│   ├── main.c                  # 主程序入口
│   ├── wifi_manager.c/h        # WiFi 网络管理
│   ├── mdns_service.c/h        # mDNS 服务发现
│   ├── udp_server.c/h          # UDP 通信服务器
│   ├── led_driver.c/h          # LED 控制驱动
│   ├── state_machine.c/h       # 状态机管理
│   └── config.h                # 配置常量定义
├── components/                 # 自定义组件 (如需要)
├── docs/                       # 技术文档
├── tools/                      # 测试工具
├── CMakeLists.txt              # 构建配置
├── sdkconfig                   # ESP-IDF 配置
├── partitions.csv              # 分区表
└── README.md                   # 项目说明
```

## 开发环境配置

### ESP-IDF 环境
```bash
# 安装 ESP-IDF v5.0+
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c3
source export.sh
```

### 项目配置
```bash
# 创建项目
idf.py create-project display-ambient-light-board
cd display-ambient-light-board

# 配置目标芯片
idf.py set-target esp32c3

# 配置项目
idf.py menuconfig
```

### 关键配置项
- **WiFi 凭据**: 通过环境变量或 Kconfig 配置
- **分区表**: 适当的应用程序和数据分区大小
- **组件配置**: 启用 WiFi、mDNS、RMT 组件
- **内存优化**: 堆大小和栈大小调优

## 实现阶段

### 阶段 1: 基础架构 (1-2 天)
- [x] 项目创建和构建配置
- [ ] 基础 ESP32-C3 初始化
- [ ] GPIO 和外设配置
- [ ] 串口调试输出
- [ ] 基础状态机框架

### 阶段 2: 网络连接 (2-3 天)
- [ ] WiFi 驱动集成
- [ ] 网络连接建立
- [ ] DHCP 客户端实现
- [ ] 连接监控和重连机制
- [ ] 网络状态事件处理

### 阶段 3: 服务发现 (1-2 天)
- [ ] mDNS 服务配置
- [ ] 服务广播实现
- [ ] 查询响应处理
- [ ] 与网络状态同步

### 阶段 4: UDP 通信 (2-3 天)
- [ ] UDP 服务器实现
- [ ] 协议解析器
- [ ] 数据包验证
- [ ] 连接检查处理
- [ ] 错误处理机制

### 阶段 5: LED 控制 (3-4 天)
- [ ] RMT 驱动配置
- [ ] SK6812 时序实现
- [ ] 大容量缓冲区管理
- [ ] 批量传输优化
- [ ] 呼吸效果实现

### 阶段 6: 集成测试 (2-3 天)
- [ ] 端到端功能测试
- [ ] 性能优化
- [ ] 错误恢复测试
- [ ] 长期稳定性测试
- [ ] 文档完善

## 测试策略

### 单元测试
- 各模块独立功能测试
- 协议解析正确性验证
- 状态机转换逻辑测试
- LED 时序参数验证

### 集成测试
- WiFi 连接稳定性测试
- UDP 通信可靠性测试
- LED 显示效果验证
- 系统恢复能力测试

### 性能测试
- 数据传输延迟测量
- LED 刷新率验证
- 内存使用监控
- 长期运行稳定性

## 风险评估与缓解

### 技术风险
1. **RMT 时序精度**: 使用示波器验证时序参数
2. **大数据量传输**: 实现分段传输和缓冲区管理
3. **WiFi 稳定性**: 实现健壮的重连机制
4. **内存管理**: 优化缓冲区使用和内存分配

### 兼容性风险
1. **协议兼容**: 严格按照现有协议规范实现
2. **硬件兼容**: 确保 GPIO 配置和时序参数正确
3. **网络兼容**: 测试不同路由器环境下的稳定性

## 成功标准

### 功能完整性
- ✅ WiFi 自动连接和 DHCP 获取
- ✅ mDNS 服务发现正常工作
- ✅ UDP 数据接收和处理无误
- ✅ LED 显示效果与原版一致
- ✅ 状态机正确处理各种场景

### 性能指标
- ✅ 数据传输延迟 < 10ms
- ✅ 支持 500 个 LED 稳定显示
- ✅ 30fps 刷新率稳定维持
- ✅ 24/7 长期运行稳定

### 代码质量
- ✅ 模块化设计，职责清晰
- ✅ 错误处理完善
- ✅ 代码注释充分
- ✅ 符合 ESP-IDF 编码规范

## 许可证

本项目采用 GPLv3 许可证，与原 board-rs 项目保持一致。

## 参考资料

### 技术文档
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32-C3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf)
- [SK6812 LED Datasheet](https://cdn-shop.adafruit.com/product-files/1138/SK6812+LED+datasheet+.pdf)

### 原项目参考
- **board-rs 项目**: `/Users/ivan/Projects/Ivan/board-rs`
- **协议文档**: `/Users/ivan/Projects/Ivan/display-ambient-light/desktop/docs`
- **状态机设计**: `/Users/ivan/Projects/Ivan/board-rs/docs/state_machine_design.md`

### 开发工具
- **ESP-IDF**: v5.0+ 官方开发框架
- **esptool**: 固件烧录工具
- **espflash**: Rust 生态烧录工具 (可选)
- **示波器**: 用于验证 LED 时序参数

## 实现注意事项

### 关键设计原则
1. **严格协议兼容**: 确保与现有桌面应用程序完全兼容
2. **性能优先**: 优化数据传输路径，减少延迟
3. **稳定性保证**: 实现健壮的错误恢复机制
4. **模块化设计**: 便于测试和维护
5. **资源优化**: 合理使用 ESP32-C3 有限的内存资源

### 特殊要求
1. **LED 数据处理**: ESP32 作为纯透传设备，不进行颜色转换
2. **缓冲区管理**: 支持大容量 LED 数据的高效传输
3. **状态指示**: 通过 LED 呼吸效果显示系统状态
4. **网络恢复**: 自动处理网络断线和重连
5. **调试支持**: 提供详细的串口日志输出

### 测试验证
1. **硬件测试**: 使用实际 500 LED 灯带验证
2. **网络测试**: 在不同网络环境下测试稳定性
3. **性能测试**: 验证延迟和刷新率指标
4. **兼容性测试**: 与现有桌面应用程序集成测试
5. **长期测试**: 24/7 连续运行稳定性验证
