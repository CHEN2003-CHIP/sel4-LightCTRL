# LightDemo

LightDemo 是一个基于 seL4 Microkit 的汽车灯光控制工程样板。当前最终交付版本为 **LightDemo Engineering Final v2.0**，目标不是只演示“能跑”，而是展示一个具备清晰结构、显式契约、故障治理、车辆状态模型和可归档验证证据的嵌入式工程项目。

## Final v2.0 结果

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| Host-side tests | PASS | `test-results/final-v2.0/host-summary.txt` |
| QEMU smoke | PASS | `test-results/final-v2.0/smoke.make.log` |
| QEMU fault integration | PASS | `test-results/final-v2.0/test-integration-fault.make.log` |
| QEMU serial E2E | PASS | `test-results/final-v2.0/test-serial-e2e.make.log` |
| Evidence manifest | PASS | `test-results/final-v2.0/manifest.txt` |
| Defense report | PASS | `reports/defense_report_final-v2.0.md` |
| Metrics dashboard | READY | `reports/final_v2_metrics.md` |

关键证据 token：

```text
STATUS_SNAPSHOT ... recovery_elapsed_ms=... layout=4 contract=OK
FAULTMG_EVENT ... severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window
FAULTMG_HISTORY ... lifecycle=ACTIVE/RECOVERING
SCHED_TARGET ... gear=3 ambient=0 hazard=0 drive_mode=0
```

## v1.1-v2.0 工程演进

| 版本 | 工程目标 | 当前状态 |
| --- | --- | --- |
| v1.1 | 用 elapsed-time recovery window 替代纯 observation tick | 已完成，证据字段 `recovery_elapsed_ms` |
| v1.2 | 补齐 fault taxonomy：来源、等级、恢复策略、输出策略、测试矩阵 | 已完成，证据字段 `FAULTMG_EVENT ... severity=...` |
| v1.3 | 自动归档 manifest、summary、QEMU log 和失败定位信息 | 已完成，`make evidence` 和 `make defense-report` |
| v2.0 | 扩展复杂车辆状态模型 | 已完成，覆盖速度、点火、制动、档位、环境光、危险警示、行驶模式 |

## 架构链路

```text
UART/input
  -> commandin
  -> scheduler
  -> lightctl
  -> gpio

fault reports / test injection
  -> faultmg
  -> scheduler + gpio

vehicle state updates
  -> vehicle_state
  -> scheduler
```

工程分层：

| 层级 | 组件 | 职责 |
| --- | --- | --- |
| 输入接入层 | `commandin`, `vehicle_state` | 串口输入、命令解析、车辆状态更新 |
| 策略决策层 | `scheduler`, `faultmg` | 灯光策略仲裁、fault lifecycle 所有权 |
| 执行与硬件层 | `lightctl`, `gpio` | 执行动作、运行时保护、GPIO 输出 |

`faultmg` 是 fault mode 和 lifecycle 的唯一 owner；其他组件只报告或消费故障状态，不直接修改全局故障生命周期。

## 目录结构

```text
lightdemo/
├── src/
│   ├── domains/          # Microkit protection domain 入口
│   ├── core/             # 可 host-test 的核心逻辑
│   └── support/          # printf/util 等支撑代码
├── include/              # 公共头文件和接口契约
├── systems/              # Microkit system 描述文件
├── tests/                # host-side 单元/场景测试
├── scripts/              # QEMU 测试和证据脚本
├── docs/                 # 工程说明文档
├── reports/              # 答辩报告和展示板
├── test-results/         # 验证证据归档
├── project-mds/          # 课程汇报 Markdown 源文档
├── project-docs/         # 课程汇报 DOCX 输出文档
└── reference-docs/       # 参考文档
```

## 构建与验证

默认环境：

| 项目 | 值 |
| --- | --- |
| Microkit SDK | 2.0.1 |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| 推荐环境 | Ubuntu 22.04 VM + QEMU |
| Shared layout | `LIGHT_SHARED_STATE_LAYOUT_V4` / `layout=4` |

常用命令：

```bash
make clean
make test TEST_RUN_ID=final-v2.0
make build MICROKIT_SDK=../microkit-sdk-2.0.1
make qemu-test TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1
make evidence TEST_RUN_ID=final-v2.0
make defense-report TEST_RUN_ID=final-v2.0
```

答辩用扩展数据 sweep：

```bash
make test-vehicle-sweep TEST_RUN_ID=final-v2.0
wc -l test-results/final-v2.0/vehicle-sweep.csv
cat test-results/final-v2.0/vehicle-sweep-summary.txt
```

该目标会生成 `vehicle-sweep.csv`，覆盖 46,080 行车辆状态/故障模式组合数据。

一条命令跑最终证据：

```bash
make final-evidence TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1
```

## 串口输入

灯光命令：

| 功能 | 打开 | 关闭 |
| --- | --- | --- |
| 近光灯 | `L` | `l` |
| 远光灯 | `H` | `h` |
| 左转向灯 | `Z` | `z` |
| 右转向灯 | `Y` | `y` |
| 示廓灯 | `P` | `p` |
| 制动灯 | `B` | `b` |

车辆状态命令：

| 字段 | 示例 |
| --- | --- |
| 速度 | `speed=80` |
| 点火 | `ignition=1` |
| 制动踏板 | `brake=1` |
| 档位 | `gear=drive`, `gear=reverse`, `gear=park` |
| 环境光 | `ambient=day`, `ambient=dusk`, `ambient=night` |
| 危险警示 | `hazard=1` |
| 行驶模式 | `mode=city`, `mode=highway`, `mode=parking`, `mode=emergency` |

诊断命令：

| 输入 | 含义 |
| --- | --- |
| `!` | 注入 `LIGHT_ERR_MODE_CONFLICT` |
| `#` | 注入 `LIGHT_ERR_HW_STATE_ERR` |
| `C` | 清除 active faults 或推进恢复观察 |
| `?` | 输出 `STATUS_SNAPSHOT` |

手动串口演示时可以重点展示 `DEMO_` 前缀日志：

```text
DEMO_FLOW   输入命令和跨保护域路由
DEMO_RESULT scheduler / vehicle_state / faultmg 的处理结果
DEMO_FAULT  故障升级、清除和恢复窗口进度
```

`make run` 手动测试时的看法：

| 输入 | 先看 | 再看 |
| --- | --- | --- |
| `L` | `[INPUT] light command LOW_BEAM_ON` | `>>> RESULT ... lamps LOW=ON ...` |
| `H` | `[INPUT] light command HIGH_BEAM_ON` | `>>> RESULT ... HIGH=ON` 或被策略限制 |
| `ambient=night` | `[INPUT] vehicle ambient=2` | `DEMO_RESULT stage=vehicle_state ... ambient:NIGHT` |
| `#` | `[INPUT] fault inject HW_STATE_ERR` | `DEMO_FAULT ...` 和 `>>> RESULT mode=SAFE_MODE ...` |
| `C` | `[INPUT] fault clear / recovery tick` | `DEMO_FAULT event=CLEAR` 或 `RECOVERY_TICK` |
| `?` | `STATUS_SNAPSHOT ...` | 彩色 `Live Status` 面板中的 `lamps` 行 |

## 文档入口

- `project-mds/14.文档质量审查报告.md`：软件工程文档完整性、格式和图规范审查结果。
- `project-mds/15.测试计划.md`：课程提交版测试计划。
- `project-mds/16.图表清单与规范说明.md`：用例图、顺序图、状态机图和工程图规范说明。
- `project-docs/`：由 `project-mds` 生成的 Word 文档输出目录；可使用 `python scripts/generate_project_docx.py` 重新生成。
- `reports/final_v2_showcase.md`：答辩展示板，一屏说明结果。
- `reports/final_v2_metrics.md`：数据图展示页，包含验证通过率、故障升级曲线、恢复窗口曲线和车辆模型增长图。
- `reports/defense_report_final-v2.0.md`：由证据脚本生成的最终答辩报告。
- `docs/release_baseline.md`：v2.0 release baseline。
- `docs/validation_report.md`：最终验证报告。
- `docs/architecture.md`：架构和保护域边界。
- `docs/safety_case.md`：故障治理和安全说明。
- `docs/requirements.md`：需求追踪。
- `docs/test_plan.md`：测试计划。
- `docs/demo_script.md`：答辩演示脚本。
- `docs/real_board_validation_template.md`：真实板卡 GPIO 后续验证模板。

## 当前边界

- 最终验收基于 Ubuntu 22.04 VM + QEMU，不声明真实板卡电气验证完成。
- 项目展示工程实践和安全论证能力，不声明 ISO 26262 等正式车规认证。
- Shared memory 已升级为 `layout=4`，用于承载 elapsed-time recovery 和 v2.0 车辆状态模型。
- `build/` 与 `build-test-hooks/` 是生成物，不作为设计源文件；最终证据以 `test-results/final-v2.0/` 和 `reports/` 为准。
