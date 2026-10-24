# DSP、音频与电源适配

先完成[平台与内存](/zh/porting/platform)。板级实现负责固件加载、资源投票和策略。
以 [sheng package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package)为实现参考，
使用目标设备的 DTS 和固件输入。

## 1. 启动 DSP 服务

`AdspProbe.cpp` 连接固件加载、SMEM、SMP2P、GLINK、IPCC 和 RPMh。
辅助实现包括 `QcomMdt.c`、`QcomSmem.c`、`QcomSmp2p.c`、`QcomGlink.c`、
`QcomQmp.c`、`QcomRpmh.c`，`ShengAdsp.h` 向电池和音频提供通道接口。

1. 记录签名固件、PAS service ID、DSP 内存/保留区、command database、SMEM item、邮箱中断和电源/时钟投票。
2. 在板级代码中适配地址与 ID，在 DTS 和固件内存映射中保留固件及安全环境拥有的区域。
3. 确认固件加载和认证后再启动 DSP。
4. 检查 ready/crash 通知，再检查 GLINK 通道协商与消息。
5. 使用有界队列接入服务回调，回调应及时返回。

sheng 的服务客户端使用 `sheng-adsp=2`，启用前检查 `native_adsp_stats` 和通道状态。
其他 SoC 需要重新确认固件、保留区和远端 item ID。

**预期结果：** DSP 就绪，通道可双向传输，记录可反映失败和重启。

## 2. 适配音频路径

`AudioProbe.cpp` 打开 `adsp_apps`，通过 `ShengGpr.c` 查询 DSP 音频框架。
`sheng-audio=1` 接受远端打开通道，`sheng-audio=2` 从本地主动打开，两者都依赖前述 DSP 路径。
`native_audio_stats[0] == 5` 表示收到框架就绪响应。

为目标实现播放或采集时：

1. 收集 codec、功放、总线、时钟、电源和端点连接。
2. 为选定固件实现 DSP graph 和端点路由。
3. 接入 OS 音频接口，提供需要的采样格式和缓冲。
4. 检查路由启动/停止、缓冲完成、可听播放或采集样本，再检查欠载与恢复。

**当前范围：** sheng 源码提供 DSP 加载、GLINK/GPR 传输和框架就绪 probe。
音频移植还需实现端点路由、OS 音频发布并验证播放/采集。

## 3. 发布电池状态

`ShengBattery.cpp` 实现 `IOPMPowerSource`，`ShengBattmgr.c` 通过 DSP 的 PMIC GLINK
通道读取电池管理器，`BatteryProbe.cpp` 注册服务。
使用 `sheng-battery=1` 与 `sheng-adsp=2` 启用。

1. 匹配目标电池管理协议、property ID、单位和符号，包括温度、电流和容量。
2. 适配通道、请求超时和通知处理。
3. 通过选定 OS power-source ABI 发布容量、充电/外接电源状态、电压和电流。
4. 与已知参考值对比，观察接入/拔出电源和容量变化时的更新。

**预期结果：** OS 收到单位正确的电池值和通知。该实现的充电控制由 DSP 固件负责。
充电策略和电池信息发布分别记录。

## 4. 适配 CPU 调频和平台电源

`CpuFreqProbe.cpp` 读取 Qualcomm EPSS 频率表，控制 CPU/L3 性能状态。
sheng 的 domain 地址、CPU 数量和参考时钟属于板级输入。
将目标 CPU affinity 映射到频率域，检查 LUT 与使能状态，再按目标功耗和温控策略选择运行点。
应用模式前阅读 `sheng-cpufreq` 和 `sheng-cpufreq-cap` 的解析代码。

`ShengPlatformPower.cpp` 提供由 `CONFIG_XNU_SHENG_PMGR` 和 `sheng-pmgr` 选择的 PMGR 集成。
适配其他平台前检查其初始化与所有调用方。Qualcomm RPMh/QMP/SCM 辅助接口需要匹配目标固件接口和资源所有权。

**预期结果：** 负载下可观察到请求的频率变化，且处于选定限制内。
逐项验证外设电源转换和系统休眠/唤醒，包括依赖设备的恢复。

## 5. 构建并记录

sheng 显示归档包含 DSP、音频和电池源码，独立平台电源配方构建 PMGR 集成。在目标构建树执行：

```sh
rg -n 'adsp_|audio_|battery_|cpufreq_' scripts/build-gop-legacy-display.py
rg -n 'sheng-cpufreq|sheng-pmgr' kits/sheng/CpuFreqProbe.cpp kits/sheng/ShengPlatformPower.cpp
```

适配源码列表和调用方，再[重建内核与镜像](/zh/building)。分别记录 DSP 就绪、音频路由、
电池发布、调频和休眠/唤醒结果，返回[启动验证](/zh/porting/bring-up)。
