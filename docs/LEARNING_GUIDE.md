# 学习路线

按数据在项目中的流向阅读：先看 F407 上的一次采样，再看 UART 帧如何到达 ESP32，最后追到 MQTT 发布和断网恢复。每一步都可以结合主机测试和串口输出验证理解。

## 1. 跟踪 F407 的一帧数据

从 `stage2/F407_SensorTerminal/Core/Src/main.c` 开始：

1. 找到各任务的创建位置，记录任务优先级、栈大小和运行周期。
2. 跟踪 `SensorTask`：BMP280/AHT20 读取后如何组成 `SensorFrame`。
3. 找到数据分别进入显示队列和遥测队列的位置。
4. 分别跟进 `DisplayTask` 和 `TelemetryTask`。思考某个消费者慢于 1 Hz 生产者时会发生什么。
5. 对比默认文本格式和 `F407_TELEMETRY_BINARY_TX=1` 的二进制格式。

可以先回答这些 FreeRTOS 问题：

- 周期任务为什么用 `vTaskDelayUntil()`，而不是每次处理结束后再延时？
- 队列复制了什么内容？接收任务拿到的数据可以使用多久？
- 队列满时会怎样？代码在哪里统计这个结果？
- 哪些工作放在中断中，哪些工作延后到任务执行？

## 2. 跟踪 UART 帧

阅读 `stage2/common/telemetry_bridge.c`、`stage4/protocol/telemetry_protocol.c`，再对照 `stage3/esp32s3_mqtt_node/components/` 中的 ESP32 解析组件。

把同一帧从 F407 序号一路跟到 JSON。检查帧长度、版本/类型字段、CRC，以及输入不完整或格式错误时的行为。

## 3. 跟踪网络中断时的投递

阅读 ESP32 主应用，再看缓存和投递模块。分别记录下面几个事件发生的时间和计数：

1. UART 帧解析成功。
2. MQTT 客户端接受发布请求。
3. MQTT 客户端报告发布完成。
4. MQTT 订阅端实际看到消息。

这些状态不能互相替代，尤其是 QoS 0。ESP32 缓存是有限的 RAM，长时间断网可能溢出，复位会清空缓存。

## 4. 把主机测试当作可执行例子

从 `host_tests/test_telemetry_protocol.c`、`test_telemetry_cache.c` 和 `test_telemetry_delivery.c` 开始。每个测试都展示输入、预期状态变化和失败边界。主机测试不会模拟 MCU 调度器或无线电硬件。

## 小实验

- 修改协议测试中的一个 CRC 字节，先预测计数器结果，再运行测试。
- 缩小缓存测试容量，推算哪个序号会被丢弃。
- 修改本地 F407 构建中的采样周期，根据 `configTICK_RATE_HZ` 算出预期 tick 间隔。
- 实机运行时断开 Wi-Fi，比较恢复前后的缓存、重连和发布计数。

每次先写下预测，再做实验。这样能检验对代码行为的理解，而不只是确认工程能编译。
