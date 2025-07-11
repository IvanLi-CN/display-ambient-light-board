# GitHub Actions 工作流说明

本项目包含三个 GitHub Actions 工作流，用于自动化构建、测试和发布流程。

## 工作流概述

### 1. 代码检查 (`check.yml`)

**触发条件:**
- 向 `main` 或 `develop` 分支推送代码
- 创建针对 `main` 或 `develop` 分支的 Pull Request

**功能:**
- 使用 ESP-IDF v5.1.2 构建项目
- 验证构建是否成功
- 上传构建产物（保留7天）

### 2. 开发版本发布 (`dev-release.yml`)

**触发条件:**
- 向 `main` 分支推送代码

**功能:**
- 自动构建并发布开发版本
- 版本号格式：`dev-YYYYMMDD-HHMMSS-短commit哈希`
- 自动清理旧的开发版本（保留最新10个）
- 标记为预发布版本

**发布内容:**
- `display-ambient-light-board.bin` - 主固件文件
- `display-ambient-light-board.elf` - 调试符号文件
- `display-ambient-light-board.map` - 内存映射文件

### 3. 正式版本发布 (`release.yml`)

**触发条件:**
- 手动触发（workflow_dispatch）

**参数:**
- `version_type`: 版本类型（patch/minor/major）
- `prerelease`: 是否为预发布版本

**功能:**
- 使用 `paulhatch/semantic-version` 生成语义化版本号
- 自动生成更新日志
- 创建正式发布版本
- 支持预发布版本

## 使用方法

### 自动触发

1. **代码检查**: 创建 PR 或推送到 main/develop 分支时自动运行
2. **开发版本**: 推送到 main 分支时自动发布开发版本

### 手动发布

1. 进入 GitHub 仓库的 Actions 页面
2. 选择 "Release" 工作流
3. 点击 "Run workflow"
4. 选择版本类型和是否为预发布版本
5. 点击 "Run workflow" 确认

## 版本号规则

### 语义化版本

- **Major**: 不兼容的 API 更改
- **Minor**: 向后兼容的功能性新增
- **Patch**: 向后兼容的问题修正

### 提交信息规范

为了让 semantic-version 正确识别版本类型，建议使用以下提交信息格式：

- `feat: 新功能` - 触发 minor 版本更新
- `fix: 修复问题` - 触发 patch 版本更新
- `BREAKING CHANGE: 描述` - 触发 major 版本更新

## 构建环境

- **操作系统**: Ubuntu Latest
- **ESP-IDF 版本**: v5.1.2
- **目标芯片**: ESP32-C3

## 注意事项

1. 确保仓库有足够的权限创建 releases
2. 开发版本会自动清理，只保留最新10个
3. 所有工作流都会上传构建产物
4. 预发布版本不会触发自动部署（如果有的话）
