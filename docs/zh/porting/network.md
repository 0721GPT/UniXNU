# Wi-Fi 适配

先完成[平台中断和 DMA](/zh/porting/platform)。确认无线芯片、PCI ID、固件和 board-data 变体。
实现回调前阅读 [Ath12kWifi](/zh/drivers/ath12k-wifi)和 `include/nw_abi.h`。

## 1. 区分集成层次

| 源码 | 职责 |
| --- | --- |
| Ath12kWifi `include/nw_abi.h` | 板级回调与 station API |
| Ath12kWifi `src/` 和 `shim/` | ath12k、PCI/MHI、QRTR 和运行时集成 |
| sheng `package/WifiProbe.cpp` | PCIe 电源、PHY、BAR、MSI、DMA、固件访问和驱动循环 |
| sheng `package/WifiPcieTables.h` | 板级选择的 PCIe PHY 配置 |
| sheng `package/MhiBoot.c` | 板级 MHI 固件启动 probe |
| sheng `package/WifiNet.cpp` | XNU 网络接口、Apple80211 请求和事件 |

阅读[板级源码](https://github.com/UniXNU/UniXNU-sheng/tree/main/package)。
sheng 的 WCN7850 位于 SM8550 PCIe0。组件还包含 QCC2072 和 QCN9274 定义。
选择定义前，核对驱动中的端点硬件版本和固件支持。

## 2. 启动 PCIe 端点

记录控制器与 PHY 地址窗口、通道数、复位顺序、WLAN enable、PERST、参考时钟、稳压器、
互连投票、SMMU stream 和 MSI 路由。sheng 使用板级 package 中的 C++ 常量和表。

1. 检查已供电、已启用时钟的寄存器状态。
2. 执行目标板的电源、时钟、PHY 配置和复位顺序。
3. 训练链路并读取端点 vendor/device ID。
4. 分配并映射 BAR，检查 PCI capability，启用需要的 memory 和 bus-master 操作。
5. 建立端点 DMA 范围，确认分配缓冲区在选定缓存策略下可达。
6. 接入 MSI 并确认中断到达，再依赖中断推进驱动。

**预期结果：** 端点正确、BAR 可读、DMA 可用且中断到达。sheng 用 `sheng-wifi` 选择阶段：
1 检查交接状态，2 启动链路，4 检查端点。当前端点路径在阶段 5 及以上进入可复用协议栈，由协议栈执行 MHI 和固件启动。
在新设备启用前阅读对应阶段源码。

## 3. 提供组件回调

按 `nw_abi.h` 实现 `lx_platform` 的时间、休眠、轮询、日志、内存分配、DMA 分配/映射/同步
和 `fw_get`；实现 `nw_board` 的 BAR、PCI 身份、配置空间访问和 MSI 目标。

sheng 使用 `native_ufs_extra_*` 提供 DMA 内存区，使用 `native_ufs_part_*` 读取固件。
采用其他存储或 DMA 实现的移植，需要提供等效板级服务。
在 `proprietary-files.txt` 中记录固件文件名、board data 和校准数据来源。

调用 `nw_wifi_start`，再由板级工作线程调用 `nw_wifi_step`。事件回调在该线程执行，应及时返回。
停止推进时检查 `nw_wifi_stats`、`nw_wifi_where`、板级 MSI 计数和固件日志。

**预期结果：** 固件与 QRTR 服务启动，随后扫描产生 BSS 记录。分别记录固件启动和扫描成功。

## 4. 接入 OS 网络接口

按选定 XNU 和无线框架 ABI 适配 `WifiNet.cpp`，将 `nw_wifi_scan`、`nw_wifi_connect`、
`nw_wifi_tx` 以及接收和链路事件接入 OS。提供该 userland 所需的 MAC 身份、信道/监管数据和请求布局。

在完整组件列表中选择 Ath12kWifi。sheng 由 `build-gop-legacy-display.py` 调用
`build-ath12k-wifi.py` 构建组件，并链接 `WifiProbe.cpp` 与 `WifiNet.cpp`。
在组装后的构建树检查：

```sh
rg -n 'struct lx_platform|struct nw_board|nw_wifi_' drivers/Ath12kWifi/include/nw_abi.h
rg -n 'nativewifi|wifi_module_objs|WifiNet|WifiProbe' scripts/build-gop-legacy-display.py
```

按[构建指南](/zh/building)重建内核、payload 和启动镜像。

**预期结果：** 扫描、认证、关联、地址配置、接收和发送分别成功。
在网络记录中写明接入点配置、固件、重连行为和持续传输结果。

## 下一步

继续 [DSP、音频和电源](/zh/porting/power)，或返回[驱动清单](/zh/porting/drivers)。
