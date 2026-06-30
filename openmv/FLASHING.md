# OpenMV 烧录手册 — 智能助盲眼镜 v7.0

> 适配 STM32 V0.1_LED 固件。当前 OpenMV 已安装 SD 卡，以下以 SD 卡方式为主。

---

## 你需要什么

- OpenMV Cam H7 Plus（已装 SD 卡） ×1
- Micro USB 数据线 ×1
- PC 已安装 [OpenMV IDE v4.8.0+](https://openmv.io/pages/download)

---

## SD 卡烧录（推荐，当前使用）

SD 卡已插入 OpenMV。按以下步骤操作：

### 方法一：IDE 直接写入（推荐）

```
1. USB 连接 OpenMV → 打开 IDE → 点击"连接"
2. 菜单：工具 → Reader/Writer
3. 切换到"SD Card"标签页
4. 按下方顺序表，逐个选中 .py 文件 → Write 到 SD 卡
5. main.py 最后写入
6. 同样将 tactile_binary_model.tflite Write 到 SD 卡
7. 验证：Reader/Writer 中应能看到全部文件
8. 断开 USB → 重新上电 → OpenMV 自动从 SD 卡运行 main.py
```

### 方法二：读卡器拷贝

```
1. 将 SD 卡从 OpenMV 取出，用读卡器连接 PC
2. 按顺序表将所有 .py 文件 + .tflite 拷贝到 SD 卡根目录
3. SD 卡插回 OpenMV → 上电即运行
```

> ⚠ OpenMV 上电后优先运行 SD 卡上的 main.py。如果 SD 卡和 Flash 中同时有 main.py，SD 卡版本优先。

---

## 烧录顺序（必须严格遵守）

MicroPython 在 `import` 时即时编译。被依赖的文件必须先烧录，否则编译期 `ImportError`。

| # | 文件 | 依赖 | 说明 |
|---|------|------|------|
| 1 | `config.py` | 无 | 阈值和参数 |
| 2 | `sensors.py` | config | UART 解析 + 状态机 |
| 3 | `fusion.py` | config, sensors | 距离融合 |
| 4 | `battery.py` | config, sensors | 电池管理 |
| 5 | `temporal_filter.py` | 无 | 时序滤波器 |
| 6 | `vision.py` | config, temporal_filter | 视觉检测 |
| 7 | `interaction.py` | 无 | UART 指令收发 |
| 8 | `decision.py` | config | 优先级仲裁 |
| 9 | `stats.py` | config | 误报统计 |
| 10 | `glasses.py` | 以上全部 | 主控制器 |
| **最后** | `main.py` | glasses | 启动入口 |

额外烧录：`tactile_binary_model.tflite`（盲道 CNN 模型，放到 SD 卡根目录）

---

## 自检 LED 闪码速查

上电后 OpenMV 板上两颗 LED 会依次闪烁，无需串口即可判断状态：

```
红绿同时快闪 ×3  🔄 自检启动
绿灯闪 ×5        📷 摄像头 OK
绿灯闪 ×1        📏 ToF OK
绿灯闪 ×2        📡 超声波 OK
绿灯闪 ×3        🧭 IMU OK
绿灯长亮 2 秒     ✅ 全部通过，进入主循环
红灯快闪 ×5      ❌ 某项自检失败
红灯持续闪       🚨 CAM/ToF 关键传感器失败，系统进入安全模式待机
```

---

## 故障排查

| 症状 | 优先检查 | 操作 |
|------|---------|------|
| 上电完全无反应 | SD 卡是否插好、main.py 是否存在 | 读卡器检查 SD 卡根目录 |
| `ImportError: no module named 'xxx'` | 烧录顺序/遗漏 | 按顺序重新烧录全部文件 |
| 自检后红灯持续闪 | 摄像头或 ToF 故障 | 检查硬件连接，无法恢复则需维修 |
| 自检后卡住，无串口输出 | STM32U5 是否上电 | 检查 UART 接线，STM32 供电 |
| `AttributeError` | sensor.py 命名冲突 | 确认文件名是 sensors.py（不是 sensor.py） |
| SD 卡不识别 | 格式/容量 | FAT32、≤32GB |

---

## 清空与重置

**清空 SD 卡：** 用读卡器格式化 SD 卡为 FAT32，重新按顺序拷贝全部文件。

---

## 文件清单

```
openmv/
├── main.py                     ← 启动入口 (最后烧录)
├── glasses.py                  ← 主控制器
├── config.py                   ← 配置参数
├── sensors.py                  ← 传感器读取 + 状态机
├── fusion.py                   ← 距离融合
├── vision.py                   ← 10项视觉检测
├── temporal_filter.py          ← 时序滤波器
├── decision.py                 ← 决策引擎
├── interaction.py              ← UART 指令收发
├── battery.py                  ← 电池管理
├── stats.py                    ← 误报统计
├── tactile_binary_model.tflite ← 盲道 CNN 模型
└── README.md / FLASHING.md     ← 文档 (不需烧录)
```

**需要烧录的文件：11 个 .py + 1 个 .tflite**