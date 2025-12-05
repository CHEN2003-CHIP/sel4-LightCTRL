# LightDemo

#### 介绍
A simple 车灯控制系统演示项目，基于 seL4 微内核和 Microkit 框架实现基本的车灯开关控制功能。

##### 项目简介
本项目是一个基于 seL4 微内核和 Microkit 组件化框架的车灯控制演示系统，主要实现以下功能：
1、通过定时任务自动切换车灯状态（亮 / 灭）
2、基于 GPIO 组件控制硬件引脚输出
3、组件间通过 Microkit 的通道机制进行通信
4、支持在 QEMU 模拟器中运行和调试
5、后续会不断优化

#### 软件架构
软件架构说明
![输入图片说明](imgimage.png)


#### 安装教程

1.  准备环境
```Bash
# 安装交叉编译器
sudo apt install gcc-aarch64-linux-gnu

# 安装QEMU
sudo apt install qemu-system-aarch64
```

2.  构建项目
```Bash
# 克隆仓库（假设已获取代码）
git clone <仓库地址>
cd lightdemo

# 构建项目（默认构建part2，包含lightctl和gpio组件）
make

# 如需指定Microkit SDK路径
make MICROKIT_SDK=/path/to/microkit-sdk-2.0.1
```
3.  运行
```Bash
# 启动QEMU模拟器运行系统
make run
```

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 作者声明
```

lightdemo/
├── Makefile          # 构建配置文件
├── lightctl.c        # 车灯控制组件（定时逻辑+命令发送）
├── gpio.c            # GPIO驱动组件（硬件控制）
├── include/
│   └── printf.h      # 轻量printf实现头文件
├── printf.c          # 轻量printf实现
└── vmm/              # 可选VMM相关代码（未启用）
```

USTC-CHEN ------AUTHOR
