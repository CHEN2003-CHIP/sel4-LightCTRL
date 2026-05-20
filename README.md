# LightDemo

LightDemo 是一个基于 seL4 Microkit 的汽车灯光控制工程样板。当前版本的定位已经不是“能跑通的 demo”，而是一个可审查、可追踪、可回归验证的嵌入式工程实践项目。

默认目标平台：

- Microkit SDK：`2.0.1`
- Board：`qemu_virt_aarch64`
- Config：`debug`
- 推荐验证环境：Ubuntu 22.04 VM + QEMU

## 当前工程基线

当前已接受的验证基线是 **LightDemo Engineering Baseline v1.0**，对应运行编号 `report-v6`。完整 release 说明见 `docs/release_baseline.md`。

| 验证项 | 结果 | 证据 |
| --- | --- | --- |
| Host-side tests | PASS | `test-results/report-v6/host-summary.txt` |
| QEMU smoke | PASS | `test-results/report-v6/smoke.make.log` |
| QEMU fault integration | PASS | `test-results/report-v6/test-integration-fault.make.log` |
| QEMU serial E2E | PASS | `test-results/report-v6/test-serial-e2e.make.log` |
| Evidence manifest | PASS | `test-results/report-v6/manifest.txt` |

关键串口证据示例：

```text
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=ACTIVE ... layout=3 contract=OK
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=RECOVERING ... layout=3 contract=OK
STATUS_SNAPSHOT fault=DEGRADED lifecycle=RECOVERING ... layout=3 contract=OK
```

复现当前验收基线可使用：

```bash
make test TEST_RUN_ID=report-v6
make build
make qemu-test TEST_RUN_ID=report-v6
make evidence TEST_RUN_ID=report-v6
```

测试结果会归档到 `test-results/<run-id>/`，其中 `make evidence` 会生成 `manifest.txt`，用于汇报时说明“本次验证跑了什么、证据在哪里、预期日志是什么”。

## 架构分层

当前主链路如下：

```text
commandin -> scheduler -> lightctl -> gpio
lightctl  -> faultmg
faultmg   -> gpio
faultmg   -> scheduler -> lightctl
commandin -> faultmg
commandin -> vehicle_state -> scheduler
```

工程汇报时可以按三层说明：

| 层级 | 组件 | 职责 |
| --- | --- | --- |
| 输入接入层 | `commandin`, `vehicle_state` | UART 接入、命令解析、车辆状态更新 |
| 策略决策层 | `scheduler`, `faultmg` | 灯光策略裁决、fault lifecycle 所有权 |
| 执行与硬件层 | `lightctl`, `gpio` | 执行协调、运行时保护、GPIO 输出 |

`faultmg` 是 fault mode 和 lifecycle 的唯一 owner；其他组件只消费 fault 状态，不直接修改 fault lifecycle。

## Fault Lifecycle v2

README 中的 fault lifecycle 描述与当前代码同步。当前状态由 `include/light_fault_mode.h` 和 `light_fault_mode.c` 定义。

| Lifecycle | 含义 |
| --- | --- |
| `STABLE` | 没有 active fault，也没有恢复流程 |
| `ACTIVE` | 至少存在一个 active fault marker |
| `RECOVERING` | active fault 已清除，但系统仍在观察窗口内逐级降低故障等级 |

Fault mode 升级规则：

| 条件 | 结果 |
| --- | --- |
| 无 active fault | `NORMAL` |
| 任意已识别 fault | `WARN` |
| 连续 3 次 `LIGHT_ERR_MODE_CONFLICT` | `DEGRADED` |
| 2 次 `LIGHT_ERR_HW_STATE_ERR` | `SAFE_MODE` |

恢复规则：

1. fault event 进入 `ACTIVE`。
2. `C` 清除 active fault marker。
3. 如果当前 mode 高于 `NORMAL`，进入 `RECOVERING`。
4. 每个健康观察 tick 推进 `recovery_ticks`。
5. 满足观察窗口后只下降一个 fault mode 等级。
6. 恢复期间发生新 fault 会打断恢复并回到 `ACTIVE`。
7. 回到 `NORMAL` 后 lifecycle 变为 `STABLE`。

v2 诊断增强包括：

- `FAULTMG_HISTORY`：记录最近 fault event、clear、recovery tick 的结构化日志。
- `last_fault_name=`：`STATUS_SNAPSHOT` 中追加可读 fault code 名称。
- `*_CONTRACT_REJECT reason=...`：统一契约拒绝日志格式。

这些增强不改变 shared memory layout，本轮仍保持 `layout=3`。

## 接口契约

项目把隐含约定显式化为 contract：

| 契约 | 证据 |
| --- | --- |
| Shared memory layout | `LIGHT_SHARED_STATE_LAYOUT_V3`, `STATUS_SNAPSHOT ... layout=3` |
| Transport wire protocol | `light_contract_check_transport_message`, `make test-contract` |
| Fault snapshot bounds | `light_contract_check_fault_snapshot`, `contract=OK` |
| Microkit channel table | `light_contract_channel_is_known`, `make test-contract` |

运行时日志中可以看到：

```text
SCHED_CONTRACT shared_state=OK
LIGHTCTL_CONTRACT shared_state=OK
FAULTMG_CONTRACT fault_snapshot=OK
STATUS_SNAPSHOT ... layout=3 contract=OK
```

非法输入或非法通道会输出统一拒绝证据：

```text
CMD_CONTRACT_REJECT reason=...
SCHED_CONTRACT_REJECT reason=...
VEHICLE_STATE_CONTRACT_REJECT reason=...
FAULTMG_CONTRACT_REJECT reason=...
LIGHTCTL_CONTRACT_REJECT reason=...
```

## 构建与运行

默认 SDK 路径：

```text
../microkit-sdk-2.0.1
```

如果 SDK 不在默认位置：

```bash
make build MICROKIT_SDK=/path/to/microkit-sdk-2.0.1
```

推荐构建：

```bash
make build
```

运行 QEMU：

```bash
make run
```

历史兼容目标仍保留：

```bash
make part1
make part2
make part3
make part4
make part5
make legacy
```

其中当前完整系统等价于 `make build` / `make part5`。

## 验证命令

Host 侧：

```bash
make test
make test-contract
make test-fault
make test-runtime
make test-transport
make test-snapshot
```

QEMU 侧：

```bash
make build
make smoke
make test-integration-fault
make test-serial-e2e
make qemu-test
```

证据归档：

```bash
make evidence TEST_RUN_ID=report-v6
```

验收重点：

- 构建稳定通过。
- 串口日志稳定命中 `STATUS_SNAPSHOT ... layout=3 contract=OK`。
- fault 降级、清除、恢复路径可重复复现。
- `FAULTMG_HISTORY` 能证明最近故障事件和恢复观察过程。
- 每个需求都能映射到实现证据、测试证据和日志证据。

## 串口输入

| 功能 | 打开 | 关闭 | Opcode |
| --- | --- | --- | --- |
| 近光灯 | `L` | `l` | `0x01` / `0x00` |
| 远光灯 | `H` | `h` | `0x11` / `0x10` |
| 左转向灯 | `Z` | `z` | `0x21` / `0x20` |
| 右转向灯 | `Y` | `y` | `0x31` / `0x30` |
| 示廓灯 | `P` | `p` | `0x41` / `0x40` |
| 制动灯 | `B` | `b` | `0x51` / `0x50` |

Fault 调试输入：

| 输入 | 含义 |
| --- | --- |
| `!` | 注入 `LIGHT_ERR_MODE_CONFLICT` |
| `#` | 注入 `LIGHT_ERR_HW_STATE_ERR` |
| `C` | 清除 active faults；恢复中再次输入可推进观察 tick |
| `?` | 输出 `STATUS_SNAPSHOT` |

`STATUS_SNAPSHOT` 当前包含 fault mode、lifecycle、recovery tick、active fault mask、target output、`last_fault_name`、layout 和 contract 状态。

## 文档入口

- `docs/release_baseline.md`：Release Baseline v1.0、当前边界和验收证据
- `docs/engineering_upgrade.md`：工程化升级说明
- `docs/engineering_roadmap.md`：v1.1 / v1.2 / v1.3 / v2.0 后续工程路线
- `docs/architecture.md`：架构与保护域边界
- `docs/safety_case.md`：故障治理与安全说明
- `docs/requirements.md`：需求追踪
- `docs/test_plan.md`：测试计划
- `docs/validation_report.md`：最新验证报告
- `项目汇报文档/`：课程汇报用标准软件工程文档

## 目录结构

```text
lightdemo/
├── include/              # 公共头文件和接口契约
├── commandin.c           # UART 输入、parser、dispatch、状态查询
├── scheduler.c           # 策略裁决和 target_output 生成
├── lightctl.c            # 执行协调和运行时保护
├── gpio.c                # GPIO / 硬件侧输出
├── faultmg.c             # fault lifecycle owner
├── vehicle_state.c       # 车辆状态更新
├── light.system          # Microkit 系统描述
├── docs/                 # 工程文档
├── scripts/              # smoke / integration / serial E2E 脚本
├── tests/                # host-side 单元测试
├── test-results/         # 验证证据归档
├── Makefile              # 构建和验证入口
├── README.md
└── README.en.md
```

## 当前边界

- 当前恢复窗口是 observation tick，不是真实时间。
- fault taxonomy 仍是课程项目规模，不等同于完整车规故障体系。
- 当前仅完成 Ubuntu 22.04 VM + QEMU 验证，未完成真实板卡 GPIO 验证。
- 本项目是工程实践基线，不声明 ISO 26262 等汽车功能安全认证合规。
- shared memory layout 固定为 V3，当前 baseline 不升级 layout。
- `test-results/report-v6/` 是 release evidence；`build/` 和 `build-test-hooks/` 属于生成或拷贝产物，不是设计源文件。
- `vmm/` 目录保留在仓库中，但不属于默认主线构建路径。
- 最终验证以 Ubuntu 22.04 VM + QEMU 为准；Windows 本地主要用于查看和编辑。

## 后续路线

| 版本 | 主线 | 目标 |
| --- | --- | --- |
| `v1.1` | 真实时间恢复窗口 | 用 elapsed-time recovery window 替代 observation tick。 |
| `v1.2` | 更完整 fault taxonomy | 补齐故障来源、等级、恢复策略和测试矩阵。 |
| `v1.3` | CI 自动保存测试证据 | 自动归档 manifest、summary、QEMU log 和失败定位信息。 |
| `v2.0` | 真实板卡或车辆状态模型 | 完成真实板卡 GPIO 验证，或扩展更复杂车辆状态模型。 |
