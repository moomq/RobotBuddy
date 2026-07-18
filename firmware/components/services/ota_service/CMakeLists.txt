# RobotBuddy OTA Service Component
#
# Complete OTA upgrade service with:
#   - HTTPS firmware download with resume support
#   - RSA-2048 signature verification
#   - SHA256 integrity check
#   - Partition management
#   - Automatic rollback
#   - Security features (cert pinning, signed URLs)
#

# 源文件列表
set(OTA_SOURCES
    "src/ota_service.c"
    "src/ota_manager.c"
    "src/ota_download.c"
    "src/ota_verify.c"
    "src/ota_partition.c"
    "src/ota_rollback.c"
    "src/ota_security.c"
)

# 头文件目录
set(OTA_INCLUDE_DIRS
    "include"
)

# 注册组件
idf_component_register(
    SRCS ${OTA_SOURCES}
    INCLUDE_DIRS ${OTA_INCLUDE_DIRS}
    REQUIRES
        framework
        esp_http_client
        esp_ota
        mbedtls
        nvs_flash
        esp_log
        json
)

# 编译选项
component_compile_options(
    -Wall
    -Wextra
    -Werror=unused-variable
    -Wno-unused-parameter
)

# 如果启用了安全启动，添加安全相关编译选项
if(CONFIG_SECURE_BOOT)
    message(STATUS "OTA: Secure Boot enabled - additional security checks active")
endif()

# 如果启用了 Flash 加密
if(CONFIG_SECURE_FLASH_ENC_ENABLED)
    message(STATUS "OTA: Flash encryption enabled")
endif()
