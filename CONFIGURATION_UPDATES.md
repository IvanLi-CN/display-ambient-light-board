# LED 灯带配置更新说明

本文档描述了对环境光板LED灯带配置系统的重要更新。

## 更新概览

### 1. LED灯带颜色通道配置（智能字符串输入）

在 `main/Kconfig.projbuild` 中添加了智能的LED颜色通道配置：

#### LED颜色通道顺序字符串
用户只需输入颜色通道字符串，系统自动计算通道数，无需手动配置。

**常见配置**:
- `"RGB"`: 红-绿-蓝 (自动识别为3通道，适用于部分WS2812B)
- `"GRB"`: 绿-红-蓝 (自动识别为3通道，大多数WS2812B的标准配置)
- `"RGBW"`: 红-绿-蓝-白 (自动识别为4通道，适用于部分SK6812)
- `"GRBW"`: 绿-红-蓝-白 (自动识别为4通道，大多数SK6812的标准配置，默认)

**智能特性**:
- 系统自动根据字符串长度计算LED通道数
- 编译时验证配置的正确性
- 运行时动态解析颜色通道映射
- 无需手动设置通道数，避免配置错误

**如何确定您的LED灯带顺序**:
1. 查看LED灯带数据手册或制造商规格说明
2. 测试方法：设置RGB(255,0,0)纯红色，观察实际显示颜色
3. 如果显示红色，您的灯带可能使用RGB顺序
4. 如果显示绿色，您的灯带可能使用GRB顺序
5. 对于RGBW灯带，单独测试白色通道

**重要提示**: 如果使用混合灯带，建议使用RGB顺序以获得更好的兼容性。

### 2. 呼吸灯效果配置增强（编译时十六进制解析）

#### 新增配置项
- `BREATHING_BASE_COLOR_HEX`: 十六进制颜色字符串（如"143250"）
- `BREATHING_MIN_BRIGHTNESS`: 呼吸灯最小亮度（默认30）
- `BREATHING_MAX_BRIGHTNESS`: 呼吸灯最大亮度（默认180）
- `BREATHING_STEP_SIZE`: 呼吸灯亮度变化步长（默认2）
- `BREATHING_TIMER_PERIOD_MS`: 呼吸灯定时器周期（默认33ms，约30Hz）

#### 编译时颜色解析
- **性能优化**: 十六进制颜色在编译时解析为RGBW分量，无运行时开销
- **自动验证**: 编译时检查颜色字符串长度是否匹配LED通道配置
- **智能映射**: 根据颜色通道顺序自动映射到正确的RGBW分量

#### 呼吸灯行为改进
- 上电时从亮度0开始增加
- 使用配置的最小/最大亮度范围
- 30Hz刷新频率提供更平滑的呼吸效果

### 3. 扩展的系统状态定义

新增了更详细的系统状态和对应的LED指示颜色：

| 状态 | 描述 | LED颜色 |
|------|------|---------|
| `LED_STATUS_INIT` | 系统初始化 | 白色 |
| `LED_STATUS_WIFI_CONFIG_ERROR` | WiFi配置异常 | 红色 |
| `LED_STATUS_WIFI_CONNECTING` | WiFi连接中 | 蓝色 |
| `LED_STATUS_WIFI_CONNECTED` | WiFi连接成功 | 青色 |
| `LED_STATUS_IP_REQUESTING` | IP获取中 | 黄色 |
| `LED_STATUS_IP_SUCCESS` | IP获取成功 | 绿色 |
| `LED_STATUS_IP_FAILED` | IP获取失败 | 橙色 |
| `LED_STATUS_NETWORK_READY` | 网络就绪 | 绿色 |
| `LED_STATUS_OPERATIONAL` | 正常运行 | 紫色 |
| `LED_STATUS_HOST_ONLINE_NO_DATA` | 上位机在线但未发送数据 | 淡紫色 |
| `LED_STATUS_WIFI_ERROR` | WiFi错误 | 红色 |
| `LED_STATUS_UDP_ERROR` | UDP错误 | 橙色 |
| `LED_STATUS_GENERAL_ERROR` | 一般错误 | 红色 |

### 4. 代码架构改进

#### 新增函数
- `set_led_color()`: 根据配置的颜色通道顺序设置LED颜色
- 支持动态颜色通道映射，兼容不同类型的LED灯带

#### 配置文件更新
- `main/config.h`: 移除硬编码的LED通道数，使用sdkconfig配置
- `sdkconfig`: 更新默认配置以支持新功能

## 使用说明

### 配置LED灯带颜色通道

1. 运行 `idf.py menuconfig`
2. 进入 "Ambient Light Board Configuration"
3. 找到 "LED Color Channel Order" 配置项
4. 输入您的LED灯带颜色通道顺序字符串（系统会自动计算通道数）

### 常见配置示例

#### WS2812B灯带（GRB顺序）
```
CONFIG_LED_COLOR_ORDER_STRING="GRB"
CONFIG_BREATHING_BASE_COLOR_HEX="143250"  # 编译时解析为RGB(20,50,80)
# 通道数自动计算为3
```

#### SK6812灯带（GRBW顺序）
```
CONFIG_LED_COLOR_ORDER_STRING="GRBW"
CONFIG_BREATHING_BASE_COLOR_HEX="14325000"  # 编译时解析为GRBW(20,50,80,0)
# 通道数自动计算为4
```

#### 自定义配置示例
```
CONFIG_LED_COLOR_ORDER_STRING="WRGB"
CONFIG_BREATHING_BASE_COLOR_HEX="00143250"  # 编译时解析为WRGB(0,20,50,80)
# 通道数自动计算为4
```

#### 测试和验证
您可以通过以下方式验证配置是否正确：
1. 设置纯红色 RGB(255,0,0)，观察是否显示红色
2. 设置纯绿色 RGB(0,255,0)，观察是否显示绿色
3. 设置纯蓝色 RGB(0,0,255)，观察是否显示蓝色
4. 对于RGBW灯带，测试白色通道

### 自动化特性
- **智能通道计算**: 系统根据字符串长度自动确定LED通道数
- **编译时验证**: 配置错误会在编译时被发现
- **运行时解析**: 动态解析颜色通道映射，支持任意合理组合
- **错误提示**: 提供详细的日志信息帮助调试配置问题

### 呼吸灯效果调整

可以通过以下配置项调整呼吸灯效果：
- 调整 `BREATHING_MIN_BRIGHTNESS` 和 `BREATHING_MAX_BRIGHTNESS` 改变亮度范围
- 调整 `BREATHING_STEP_SIZE` 改变呼吸速度
- 调整 `BREATHING_TIMER_PERIOD_MS` 改变刷新频率

## 兼容性说明

- 所有更改向后兼容现有配置
- 默认配置保持为SK6812 GRBW模式
- 混合灯带建议使用RGB配置以获得最佳兼容性

## 编译和部署

更新配置后，重新编译和烧录固件：

```bash
idf.py build
idf.py flash
```

编译成功，所有新功能已集成到系统中。
