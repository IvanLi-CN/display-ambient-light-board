#!/usr/bin/env python3
"""
简单的固件配置工具测试脚本
用于验证固件配置区域是否正确嵌入
"""

import sys
import struct

def find_config_marker(firmware_data):
    """查找配置标记"""
    marker = b"FWCFG_START"
    pos = 0

    # 查找所有标记，选择后面跟着魔数的那个
    while True:
        pos = firmware_data.find(marker, pos)
        if pos == -1:
            return None

        config_start = pos + len(marker)
        if config_start + 4 <= len(firmware_data):
            # 检查是否跟着魔数
            magic_bytes = firmware_data[config_start:config_start + 4]
            if magic_bytes == b'\x78\x56\x34\x12':  # 0x12345678 小端序
                return config_start

        pos += 1

    return None

def parse_config(firmware_data, offset):
    """解析配置数据"""
    if offset + 256 > len(firmware_data):
        print("错误：固件文件太小，无法包含完整配置")
        return None

    config_data = firmware_data[offset:offset + 256]

    # 显示前16字节用于调试
    print(f"配置数据前16字节: {config_data[:16].hex()}")

    # 解析配置结构
    magic, version = struct.unpack('<II', config_data[0:8])
    
    if magic != 0x12345678:
        print(f"错误：魔数不匹配 0x{magic:08x} (期望 0x12345678)")
        return None
    
    if version != 1:
        print(f"错误：版本不支持 {version} (期望 1)")
        return None
    
    # 读取字符串字段 - 找到第一个null字节并截断
    def read_cstring(data, start, length):
        """读取C风格字符串（以null结尾）"""
        end = start + length
        null_pos = data.find(b'\x00', start, end)
        if null_pos != -1:
            return data[start:null_pos].decode('utf-8', errors='ignore')
        return data[start:end].decode('utf-8', errors='ignore')

    wifi_ssid = read_cstring(config_data, 8, 64)
    wifi_password = read_cstring(config_data, 72, 64)

    # UDP Port (2字节) - 偏移136
    udp_port, = struct.unpack('<H', config_data[136:138])

    # mDNS hostname (32字节) - 偏移138
    mdns_hostname = read_cstring(config_data, 138, 32)

    # LED pin (1字节) - 偏移170
    led_pin, = struct.unpack('<B', config_data[170:171])

    # Max LEDs (2字节) - 偏移171
    max_leds, = struct.unpack('<H', config_data[171:173])

    # LED order (8字节) - 偏移173
    led_order = read_cstring(config_data, 173, 8)
    
    return {
        'magic': magic,
        'version': version,
        'wifi_ssid': wifi_ssid,
        'wifi_password': wifi_password,
        'udp_port': udp_port,
        'mdns_hostname': mdns_hostname,
        'led_pin': led_pin,
        'max_leds': max_leds,
        'led_order': led_order
    }

def main():
    if len(sys.argv) != 2:
        print("用法: python test-config-tool.py <固件文件.bin>")
        sys.exit(1)
    
    firmware_file = sys.argv[1]
    
    try:
        with open(firmware_file, 'rb') as f:
            firmware_data = f.read()
    except FileNotFoundError:
        print(f"错误：找不到文件 {firmware_file}")
        sys.exit(1)
    
    print(f"固件文件: {firmware_file}")
    print(f"文件大小: {len(firmware_data)} 字节")
    
    # 查找配置标记
    config_offset = find_config_marker(firmware_data)
    if config_offset is None:
        print("错误：未找到配置标记 'FWCFG_START'")
        sys.exit(1)
    
    print(f"配置区域偏移: 0x{config_offset:08x}")
    
    # 解析配置
    config = parse_config(firmware_data, config_offset)
    if config is None:
        print("错误：配置解析失败")
        sys.exit(1)
    
    # 显示配置
    print("\n=== 固件配置信息 ===")
    print(f"魔数: 0x{config['magic']:08x}")
    print(f"版本: {config['version']}")
    print(f"WiFi SSID: '{config['wifi_ssid']}'")
    print(f"WiFi 密码: '{config['wifi_password']}'")
    print(f"UDP 端口: {config['udp_port']}")
    print(f"mDNS 主机名: '{config['mdns_hostname']}'")
    print(f"LED 引脚: {config['led_pin']}")
    print(f"最大 LED 数: {config['max_leds']}")
    print(f"LED 颜色顺序: '{config['led_order']}'")
    
    print("\n✅ 配置区域验证成功！")

if __name__ == '__main__':
    main()
