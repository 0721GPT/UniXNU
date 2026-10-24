# 源码与补丁

所选 OS 配置固定 XNU 和 ABI 输入。iPadOS 17.7.11 配置使用 `xnu-10063.141.1`、
Darwin 23.6.0、`xnu-10063.121.3` 的 DriverKit 头文件和 `xnu-8796.101.5` 的 TrustCache 头文件。

`feeds.toml` 提供 libplatform、libdispatch、AvailabilityVersions、libpthread、
corecrypto、IOStorageFamily 和 edk2。格式 2 中 OS 配置显式指定的 XNU 版本优先于源码 lock。

## 补丁应用

格式 2 按声明顺序解析 `[kernel] patch_series`。series 每行一个补丁，路径相对于
series 文件，或使用 `arch:`、`soc:`、`os:`、`device:`、`component:<ID>/` 前缀。
组件补丁参与同一个应用顺序，`unixnu status --json` 显示最终序列。

内核准备在所选目标下创建 `tree/.cache/xnu-work`，检查并应用各补丁，随后准备缺失
的源码头文件。生成头文件和 `tree/build` 输出也归该目标所有。

格式 1 按文件名顺序选择 `patches/xnu/*.patch` 和 `kits/XnuPrelink/patches/*.patch`。
其 shell 准备流程处理后置 `zzw`、`zzy`、`zzz` 补丁，并检查已应用的补丁。

## 归属

| 位置 | 内容 |
| --- | --- |
| ARM64 配置 | 通用架构配置和补丁系列 |
| SoC 仓库 | 硬件操作和 SoC 补丁系列 |
| OS 配置 | 版本相关修复和预链接补丁选择 |
| 设备仓库 | 板级补丁系列 |
| 组件仓库 | 组件所需补丁 |
| `patches/libpthread/`、`patches/mesa/` | 库和编译器源码补丁 |

## 新增补丁

1. 先构建选定内核，生成目标的 `tree/.cache/xnu-work`。
   将 `XNU_WORK` 设为该目录，`DEVICE_DIR` 设为设备仓库。
2. 将待修改文件按 XNU 相对路径复制到临时 Git 目录，编辑前暂存基线。例如修改 `start.s`：

```sh
PATCH_WORK=$(mktemp -d)
git -C "$PATCH_WORK" init
mkdir -p "$PATCH_WORK/osfmk/arm64"
cp "$XNU_WORK/osfmk/arm64/start.s" "$PATCH_WORK/osfmk/arm64/start.s"
git -C "$PATCH_WORK" add osfmk/arm64/start.s
```

3. 编辑临时副本，再导出修改：

```sh
git -C "$PATCH_WORK" diff -- osfmk/arm64/start.s > "$DEVICE_DIR/patches/xnu/my-change.patch"
```

4. 检查补丁，把 `my-change.patch` 加入设备 series，从 UniXNU 执行 `./unixnu build kernel`。
   共享修改放入所属 SoC、架构、OS 或组件 series，位置应与编辑基线一致。
5. 用 `./unixnu status --json` 检查补丁顺序并记录构建结果。
   补丁输入变化时，内核准备步骤会重新生成工作源码。

新增构建选项可使用以下 personality 模式：

```c
#if defined(XNU_MY_OPTION) && XNU_MY_OPTION
    "   ,{"
    "     'IOClass'         = MyDriver;"
    "     'IOProviderClass' = IOPlatformDevice;"
    "   }"
#endif
```

对应的 `-DXNU_MY_OPTION=1` 来自内核脚本的 `EXTRA_MAKE_ARGS`。

## 构建开关

| 开关 | 值 |
| --- | --- |
| `SDKROOT` | `iphoneos` |
| `ARCH_CONFIGS` | `ARM64` |
| `MACHINE_CONFIGS` | `BCM2837` |
| `KERNEL_CONFIGS` | `DEVELOPMENT` |
| `ARM64_LINK_ADDR` | `0xfffffff007004000` |
| `ARM64_PLKSEG_ADDR` | `0xfffffff004004000` |
| `DEPLOYMENT_TARGET_FLAGS` | `-miphoneos-version-min=17.0 -DXNU_TARGET_OS_IOS` |
| `BUILD_LTO` | `0` |
| `BOUND_CHECKS` | `0` |

`BCM2837` 携带 4 KiB 页布局。`BUILD_LTO=0` 保持目标文件和编译记录可读，并与外部静态库链接兼容。

## defconfig 映射

每个 `CONFIG_XNU_<NAME>` 变成 `XNU_<NAME>`：

| defconfig 值 | 构建值 |
| --- | --- |
| `y` | `1` |
| `n` | `0` |
| `"text"` | `text` |
| `123` | `123` |
| 字符串中的 `{tree}` | 构建树路径 |

内核脚本读取这些变量并加入目标文件开关、静态库和链接输入。

## 源码树

```
out/<device>/<os_profile>/<config>/tree/
  refer/xnu/                 固定源码
  .cache/xnu-work/           工作克隆
  .cache/fakeroot/           内核 SDK 根
  .cache/sources/            恢复头文件清单
  build/xnu/ios-obj/         目标文件和编译记录
  build/xnu/ios-sym/         符号
  build/xnu/ios-dst/         安装头文件
  patches/xnu/               链接的补丁
  kits/XnuPrelink/        预链接工具和补丁
```

## 更新源码

1. 在 `feeds.toml` 中设置新的 `ref` 和 `commit`。
2. 更新 `darwin`、`version`、`driverkit_tag` 和 `trustcache_tag`。
3. 把补丁 rebase 到新 tag。
4. 按新的编译记录更新 `[kernel] archives` 中的静态库脚本。
5. 重建预链接并重新验证 kernelcache 固定。

预链接输出依赖原始 kernelcache ABI。新的源码版本可能需要新的 kernelcache 固定和 roots。

## 下一步

- [4 KiB 页表](/zh/xnu/four-k-page-tables)
- [补丁参考](/zh/xnu/patch-reference)
- [构建内核](/zh/guide/build-kernel)
