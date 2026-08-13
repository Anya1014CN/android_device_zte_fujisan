# LineageOS 23.2 kernel rebase：相机修复清单

- [x] 确认设备已连接，`adb root` 可用，CameraService 枚举后置/前置两颗相机。
- [x] 恢复被小米基线快照覆盖的 fujisan OEM CSI 时钟率（CSIPHY 266/200 MHz、CSID 320 MHz），后置拍照预览已从花屏恢复正常。
- [x] 恢复被 vendor 重新提取时遗漏的原厂 JPEG 编码库 `libmmqjpeg_codec.so`、`libmmqjpegdma.so`；它们是现有 JPEG OMX 组件的直接依赖。
- [x] 恢复被当前 vendor 镜像遗漏的原厂 C2D 后端库 `libc2d30-a5xx.so`、`libc2d30_bltlib.so` 的设备侧打包规则。
- [x] 对比 rebase 前内核的 VIDC 驱动；未发现导致本问题的逻辑差异，保持内核不改。
- [x] 通过 `adb root` 验证：预览、拍照保存、扫码及闪光灯正常。
- [x] 为 CAF V4L2 OMX 补齐设备侧构建所需的公开 MSM8996 gralloc ABI 头文件与 `eventfd` seccomp 规则；未修改 AOSP/上游源码。
- [ ] 修复录像：已确认并非 AVC 编码器；Aperture 在开始录像时创建麦克风输入流失败（所有采样率的输入缓冲查询失败），待音频输入恢复后复测。
- [x] 清理临时 C2 编码器 XML 测试；相机相关的新增文件均纳入版本控制。
