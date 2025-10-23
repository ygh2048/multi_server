说明: 多连接 (TCP_MULTI_CONNECTION_MODE)

1) 启用/禁用多连接模式
   - 在 `modtcpbsp.h` 中使用 `#define TCP_MULTI_CONNECTION_MODE` 启用多连接，注释掉则回到单连接模式。

2) 最大并发连接数
   - 使用 `TCP_MAX_SOCK` 宏控制并发 socket 数，默认在 `modtcpbsp.h` 中定义为 4。
   - 修改该值后需重新构建工程。

3) 每-socket 缓冲
   - 当前实现为每 socket 使用 `MB_TCP_ACC_SIZE/2` 的累加缓冲（在 `modbus/port/porttcp.c` 中），可根据内存和负载调整。
   - 若需要更高并发或更大帧，建议把每 socket 缓冲改为 `MB_TCP_ACC_SIZE` 或实现环形缓冲。

4) 注意事项
   - 目前端口层使用全局的单帧输出缓冲 `ucTCPBuf`，同时保存来源 socket id (`s_frame_sock`)；假如上层处理响应为同步流程（立即调用发送），此设计是可行的。
   - 若上层处理耗时较长或需要高并发，请改造 FreeModbus 层以接受 socket id 或使用帧队列。

5) 调试
   - 打开 `TCP_MULTI_DEBUG` 可看到每个 socket 的连接/接收日志，便于诊断。

作者: 自动补丁由助理生成
时间: 2025-10-23
