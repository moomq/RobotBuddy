# Python 编码标准 — RobotBuddy Local Bridge

> 适用范围：RobotBuddy Local Bridge (PC端桌面代理)、测试脚本、构建工具、HIL 测试脚本
> 版本：1.0
> 最后更新：2026-07-10

---

## 1. 环境与版本

| 项目 | 要求 |
|------|------|
| Python 版本 | ≥ 3.8 |
| 类型检查 | mypy --strict |
| 代码格式 | Black (line-length=100) |
| Import 排序 | isort |
| Lint | flake8 + pylint |
| 依赖管理 | requirements.txt + venv |

---

## 2. 命名规范

| 类型 | 风格 | 示例 |
|------|------|------|
| 模块 | snake_case | `mqtt_client.py` |
| 类 | PascalCase | `MQTTClient`, `RobotBuddy` |
| 函数 | snake_case | `connect_to_robot()` |
| 变量 | snake_case | `device_ip = "192.168.4.1"` |
| 常量 | UPPER_SNAKE_CASE | `MAX_RETRY_COUNT = 3` |
| 私有属性 | _leading_underscore | `self._connection_state` |
| 枚举 | PascalCase + UPPER_VALUES | `class ConnectionState: CONNECTED = "connected"` |

---

## 3. 类型标注

```python
# 所有公共函数必须添加类型标注
from typing import Optional, List, Dict, Any

def send_command(
    device_id: str,
    command: str,
    params: Optional[Dict[str, Any]] = None,
    timeout: float = 5.0,
) -> bool:
    """发送命令到 RobotBuddy 设备。

    Args:
        device_id: 设备 MAC 地址
        command: 命令名称
        params: 命令参数，默认为 None
        timeout: 超时时间（秒）

    Returns:
        bool: 命令是否成功发送并收到 ACK

    Raises:
        ConnectionError: 设备不可达
        TimeoutError: 命令超时
    """
    ...
```

### 类型标注规则

1. **所有公共函数必须有类型标注** — 参数和返回值
2. **使用 `Optional[X]` 而非 `X | None`** — Python 3.8 兼容
3. **使用 `Dict[str, Any]` 而非 `dict`** — 明确键值类型
4. **回调函数类型标注** — `Callable[[str, Dict], None]`

---

## 4. 异步编程

```python
# Local Bridge 使用 asyncio 处理并发 I/O
import asyncio

class RobotBuddyBridge:
    async def connect(self, host: str, port: int) -> None:
        """异步连接设备。"""
        self._reader, self._writer = await asyncio.open_connection(host, port)

    async def listen(self) -> None:
        """持续监听设备消息。"""
        while self._running:
            try:
                data = await asyncio.wait_for(
                    self._reader.read(4096),
                    timeout=5.0,
                )
                await self._process_message(data)
            except asyncio.TimeoutError:
                await self._send_heartbeat()
            except ConnectionError:
                await self._reconnect()
```

### 异步规则

1. **所有 I/O 操作使用 async/await** — 禁止同步阻塞调用
2. **使用 `asyncio.wait_for()` 设置超时** — 所有网络操作必须有超时
3. **异常处理不吞没** — `except Exception` 必须记录日志
4. **使用 `asyncio.Queue` 进行任务间通信** — 替代线程 Queue

---

## 5. 错误处理

```python
# 自定义异常层次
class RobotBuddyError(Exception):
    """RobotBuddy 基础异常。"""

class ConnectionError(RobotBuddyError):
    """连接异常。"""

class CommandError(RobotBuddyError):
    """命令执行异常。"""

class TimeoutError(RobotBuddyError):
    """超时异常。"""


# 使用模式
try:
    result = await bridge.send_command("build_status")
except ConnectionError:
    logger.error("设备连接失败，尝试重连")
    await bridge.reconnect()
except TimeoutError:
    logger.warning("命令超时，使用缓存数据")
    result = cache.get("build_status")
except RobotBuddyError as e:
    logger.error(f"RobotBuddy 错误: {e}")
    raise
```

---

## 6. 日志规范

```python
import logging

logger = logging.getLogger(__name__)

# 日志级别使用规范:
# CRITICAL: 程序无法继续运行
# ERROR: 功能失败，但程序可继续
# WARNING: 异常情况，需要关注
# INFO: 重要状态变化（连接/断开/命令执行）
# DEBUG: 详细调试信息

logger.info("连接到 RobotBuddy %s:%d", host, port)
logger.warning("命令超时: %s, 重试中...", command)
logger.error("发送失败: %s", e, exc_info=True)
logger.debug("收到消息: %s", message.hex())
```

---

## 7. 配置管理

```python
# 使用 dataclass 管理配置
from dataclasses import dataclass, field

@dataclass
class BridgeConfig:
    """Local Bridge 配置。"""
    host: str = "192.168.4.1"
    port: int = 8080
    mqtt_broker: str = "broker.robotbuddy.local"
    mqtt_port: int = 1883
    retry_count: int = 3
    retry_interval: float = 5.0
    log_level: str = "INFO"

    @classmethod
    def from_file(cls, path: str) -> "BridgeConfig":
        """从配置文件加载。"""
        ...

    def to_dict(self) -> Dict[str, Any]:
        """导出为字典。"""
        return self.__dict__.copy()
```

---

## 8. 测试规范

```python
# 使用 pytest
import pytest

class TestMQTTClient:
    """MQTT 客户端测试。"""

    @pytest.fixture
    def client(self):
        """创建测试客户端。"""
        return MQTTClient(config=BridgeConfig())

    def test_connect_success(self, client):
        """测试连接成功。"""
        assert client.connect() is True

    def test_connect_failure(self, client, mocker):
        """测试连接失败。"""
        mocker.patch("asyncio.open_connection", side_effect=ConnectionRefusedError)
        with pytest.raises(ConnectionError):
            client.connect()

    @pytest.mark.asyncio
    async def test_send_command(self, client):
        """测试异步命令发送。"""
        result = await client.send_command("ping")
        assert result is True
```

---

## 9. 禁止项

| # | 禁止 | 替代方案 |
|---|------|---------|
| 1 | `except:` 裸异常捕获 | `except Exception as e:` + 记录 |
| 2 | `import *` | 明确导入 |
| 3 | 全局可变状态 | 依赖注入或配置类 |
| 4 | 硬编码 IP/端口/密钥 | 配置文件或环境变量 |
| 5 | 同步阻塞 I/O | asyncio + aiohttp/websockets |
| 6 | `time.sleep()` | `await asyncio.sleep()` |
| 7 | 无类型标注的公共函数 | 添加类型标注 |
| 8 | `print()` 调试输出 | `logger.debug()` |
| 9 | 未处理的网络异常 | try/except + 重连逻辑 |
| 10 | 不加密的 WiFi 密码存储 | 加密存储或密钥管理 |

---

## 10. 文件组织

```
local_bridge/
├── __init__.py
├── config.py          # BridgeConfig dataclass
├── mqtt_client.py     # MQTT 通信
├── ws_client.py       # WebSocket 通信
├── device_manager.py  # 设备管理
├── build_watcher.py   # 构建/编译状态监控
├── git_monitor.py     # Git 仓库状态
├── notification.py    # 通知推送
└── utils.py           # 工具函数

tests/
├── test_mqtt_client.py
├── test_ws_client.py
├── test_device_manager.py
├── test_build_watcher.py
├── test_git_monitor.py
└── conftest.py         # pytest fixtures
```