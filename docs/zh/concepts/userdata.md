# userdata 布局

userdata 分区承载 Darwin 根文件系统。UniXNU 保留原始磁盘镜像不变，并添加 manifest 把每个镜像映射到分区窗口。

## manifest

```
struct shngroot_header {
  char     magic[8];        /* "SHNGROOT" */
  uint32_t version;         /* 1 */
  uint32_t count;           /* 窗口数 */
  uint64_t image_bytes;     /* 镜像总大小 */
  struct {
    char     name[32];
    uint64_t offset;
    uint64_t size;
    uint32_t block_size;    /* 512 */
    uint32_t flags;         /* bit 0: 只读 */
    uint8_t  sha256[32];
    uint8_t  reserved[40];
  } window[count];
  uint8_t  reserved[32];
  uint8_t  manifest_sha256[32];
};
```

头部占 4096 字节，零填充到 1 MiB。第一个窗口从 0x100000 开始，每个窗口按 1 MiB 对齐。

## 窗口

| 窗口 | IPSW 组件 | 内容 |
| --- | --- | --- |
| `OS` | `OS` | 系统卷 |
| `SystemOS` | `Cryptex1,SystemOS` | cryptex 系统卷 |

镜像按字节从 IPSW 磁盘镜像复制。userdata 步骤用 `hdiutil` 只读挂载每个磁盘镜像并读取原始设备。

## 容量

```
(last_lba - first_lba + 1) * lba_bytes
```

步骤拒绝超过容量的镜像。报告记录容量和镜像大小。

## 根卷匹配

XNU 通过 `/chosen/root-matching` 选择根卷：

```xml
<dict>
  <key>IOProviderClass</key><string>IOMedia</string>
  <key>IOPropertyMatch</key>
  <dict>
    <key>UUID</key><string>00000000-0000-0000-0000-000000000000</string>
  </dict>
</dict>
```

UUID 是 OS 窗口的卷 UUID。原生块驱动发布 userdata 分区，分区方案发布卷，XNU 匹配 UUID。

## Data 卷

可写 Data 卷独立于 userdata 镜像，来自负载中的 Data ramdisk 种子。种子是负载步骤的输入，负载报告记录其哈希。

`--skip-setup` 写出把 Setup 标记为完成的种子副本，副本按种子哈希缓存。

## 报告

`userdata-root.json` 记录：

| 字段 | 含义 |
| --- | --- |
| `image_bytes` | 镜像总大小 |
| `image_sha256` | 镜像哈希 |
| `manifest_sha256` | manifest 哈希 |
| `windows` | 每个窗口的名称、偏移、大小和哈希 |
| `userdata_bytes` | 分区容量 |
| `flash_command` | `fastboot flash userdata <image>` |
| `source` | 本地或远程 IPSW |
| `hardware_tested` | 验证状态 |

## 下一步

- [userdata 镜像](/zh/guide/userdata)
- [设备配置](/zh/reference/configuration)
- [启动链](/zh/concepts/boot-chain)
