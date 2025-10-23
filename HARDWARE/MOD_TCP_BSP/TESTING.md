Testing Modbus-TCP Multi-Connection Support

1) 开启/关闭多连接
   - 编辑 `modtcpbsp.h`，取消注释或注释 `#define TCP_MULTI_CONNECTION_MODE`。

2) 调整最大并发连接数
   - 在 `modtcpbsp.h` 修改 `#define TCP_MAX_SOCK` 为所需值（典型 1..8）。修改后重新构建。

3) 调试输出控制
   - 使用 `MODTCP_DEBUG`（0/1）开启或关闭调试输出。
   - 使用 `MODTCP_DEBUG_LEVEL`（1 或 2）选择输出级别：1 = 基本信息，2 = 详细信息。
   - 在代码中使用 `MODTCP_DBG(level, "fmt", ...)` 或 `TCP_DBG("fmt", ...)`（等同于 level=1）。

4) 手动测试步骤（基本）
   - 将设备连接到局域网并设置合适 IP。
   - 在 PC 上运行 `modbus-tcp` 客户端或 `mbtget` 发起读写请求到端口 502。
   - 观察串口输出（调试信息）确认连接建立与帧收发。

5) 常见问题
   - 未开启多连接但端口被另一个程序占用：确保端口空闲或修改监听端口。
   - 内存不足/ACC overflow：把 `MB_TCP_ACC_SIZE` 减小或减少 `TCP_MAX_SOCK`，或者增加 per-socket 缓冲大小。

6) 提交
   - 修改并测试无误后，使用 git commit 提交本地修改到 `feature/multi-connection` 分支。