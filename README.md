# LightDemo

#### 介绍
A simple 车灯控制系统演示项目，基于 seL4 微内核和 Microkit 框架实现基本的车灯开关控制功能。

##### 项目简介
本项目是一个基于 seL4 微内核和 Microkit 组件化框架的车灯控制演示系统，主要实现以下功能：
1、通过相应的信号进行操作灯光
2、基于 GPIO 组件控制硬件引脚输出
3、组件间通过 Microkit 的通道机制进行通信
4、支持在 QEMU 模拟器中运行和调试
5、原生支持线程安全和内存安全

#### 软件架构
软件架构说明
![输入图片说明](imgimage.png)


#### 安装教程

1.  准备环境
```Bash
# First make a directory for the tutorial
mkdir microkit_tutorial
cd microkit_tutorial
# Then download and extract the SDK
curl -L https://github.com/seL4/microkit/releases/download/2.0.1/microkit-sdk-2.0.1-linux-x86-64.tar.gz -o sdk.tar.gz
tar xf sdk.tar.gz

```

2.  构建项目
```Bash
# 克隆仓库（假设已获取代码）
git clone <仓库地址>
cd lightdemo

# 构建项目
make part5（目前项目推进到part5,基本完成基本功能，未来会持续更新，记得关注这个命令的变更)

# 如需指定Microkit SDK路径
make MICROKIT_SDK=/path/to/microkit-sdk-2.0.1
```
3.  运行
```Bash
# 启动QEMU模拟器运行系统
make run
```
#### 使用教程
启动完成以后可以按照这个操作手册进行测试和使用，你只需要按动键盘相应的按钮即可测试
```Bash
* 指令列表：
 * ┌──────────────┬──────────┬──────────┬──────────┐
 * │ 灯光类型     │ 开启指令 │ 关闭指令 │ 操作码   │
 * ├──────────────┼──────────┼──────────┼──────────┤
 * │ 近光灯       │ L        │ l        │ 0x01/0x00│
 * │ 远光灯       │ H        │ h        │ 0x11/0x10│
 * │ 左转向灯     │ Z        │ z        │ 0x21/0x20│
 * │ 右转向灯     │ Y        │ y        │ 0x31/0x30│
 * │ 示廓灯       │ P        │ p        │ 0x41/0x40│
 * │ 刹车灯       │ B        │ b        │ 0x51/0x50│
 * └──────────────┴──────────┴──────────┴──────────┘
 * 
```

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 作者声明
```
注意lightdemo项目必须和microkit-sdk-2.0.1在同一级，否则需要自行修改makefile
/*
 * lightdemo-copy
 * ├── build/
 * ├── include/
 * ├── vmm/
 * ├── commandin.c
 * ├── faultmg.c
 * ├── gpio.c
 * ├── light.system
 * ├── lightctl.c
 * ├── Makefile
 * ├── scheduler.c
 * microkit-sdk-2.0.1/
 */
```

USTC-CHEN ------AUTHOR
