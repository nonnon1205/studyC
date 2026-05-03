/**
 * @file main.c
 * @brief mgmtctl — studyC モジュール向けコマンドラインクライアント
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "mgmt_protocol.h"
#include "mgmt_paths.h"
#include "mgmt_send.h"

static const char* module_to_path(const char* module)
{
    if (strcmp(module, "collector") == 0) return MGMT_SOCKET_PATH_COLLECTOR;
    if (strcmp(module, "router")    == 0) return MGMT_SOCKET_PATH_ROUTER;
    if (strcmp(module, "viewer")    == 0) return MGMT_SOCKET_PATH_VIEWER;
    return NULL;
}

static int parse_command(const char* s, uint8_t* out)
{
    static const struct { const char* name; MgmtCommandType type; } table[] = {
        { "ping",      MGMT_CMD_PING            },
        { "status",    MGMT_CMD_GET_STATUS       },
        { "metrics",   MGMT_CMD_GET_METRICS      },
        { "reset",     MGMT_CMD_RESET_METRICS    },
        { "loglevel",  MGMT_CMD_SET_LOG_LEVEL    },
        { "bufsize",   MGMT_CMD_SET_BUFFER_SIZE  },
        { "profiling", MGMT_CMD_ENABLE_PROFILING },
        { "tracing",   MGMT_CMD_ENABLE_TRACING   },
        { "config",    MGMT_CMD_GET_CONFIG       },
        { "shutdown",  MGMT_CMD_SHUTDOWN         },
        { NULL,        0                         }
    };
    for (int i = 0; table[i].name; i++) {
        if (strcmp(s, table[i].name) == 0) {
            *out = (uint8_t)table[i].type;
            return 0;
        }
    }
    return -1;
}

static int parse_loglevel(const char* s, uint8_t* out)
{
    static const struct { const char* name; LogLevel level; } table[] = {
        { "trace", LOG_LEVEL_TRACE },
        { "debug", LOG_LEVEL_DEBUG },
        { "info",  LOG_LEVEL_INFO  },
        { "warn",  LOG_LEVEL_WARN  },
        { "error", LOG_LEVEL_ERROR },
        { "fatal", LOG_LEVEL_FATAL },
        { NULL,    0               }
    };
    for (int i = 0; table[i].name; i++) {
        if (strcmp(s, table[i].name) == 0) {
            *out = (uint8_t)table[i].level;
            return 0;
        }
    }
    return -1;
}

static void print_usage(const char* prog)
{
    fprintf(stderr,
        "Usage: %s <module> <command> [arg]\n"
        "\n"
        "Modules : collector, router, viewer\n"
        "\n"
        "Commands:\n"
        "  ping                  Liveness check\n"
        "  status                Get module status\n"
        "  metrics               Get performance metrics\n"
        "  reset                 Reset metric counters\n"
        "  loglevel <level>      Set log level  (trace|debug|info|warn|error|fatal)\n"
        "  bufsize  <size>       Resize buffer capacity\n"
        "  profiling             Enable performance profiling\n"
        "  tracing               Enable event tracing\n"
        "  config                Dump current configuration\n"
        "  shutdown              Request graceful shutdown\n",
        prog);
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char* module  = argv[1];
    const char* cmd_str = argv[2];

    const char* path = module_to_path(module);
    if (!path) {
        fprintf(stderr, "Unknown module: %s\n", module);
        return 1;
    }

    uint8_t cmd_type;
    if (parse_command(cmd_str, &cmd_type) < 0) {
        fprintf(stderr, "Unknown command: %s\n", cmd_str);
        return 1;
    }

    uint8_t payload[MGMT_PAYLOAD_REQUEST_SIZE] = {0};
    size_t  payload_len = 0;

    if (cmd_type == MGMT_CMD_SET_LOG_LEVEL) {
        if (argc < 4) {
            fprintf(stderr, "loglevel requires a level argument\n");
            return 1;
        }
        uint8_t level;
        if (parse_loglevel(argv[3], &level) < 0) {
            fprintf(stderr, "Unknown log level: %s\n", argv[3]);
            return 1;
        }
        payload[0]  = level;
        payload_len = 1;
    } else if (cmd_type == MGMT_CMD_SET_BUFFER_SIZE) {
        if (argc < 4) {
            fprintf(stderr, "bufsize requires a size argument\n");
            return 1;
        }
        char* endptr;
        long size_l = strtol(argv[3], &endptr, 10);
        if (*endptr != '\0' || size_l <= 0 || size_l > INT_MAX) {
            fprintf(stderr, "Invalid buffer size: %s\n", argv[3]);
            return 1;
        }
        int size = (int)size_l;
        memcpy(payload, &size, sizeof(int));
        payload_len = sizeof(int);
    }

    MgmtCommandRequest req;
    mgmt_request_init(&req, cmd_type, module,
                      payload_len ? payload : NULL, payload_len);

    MgmtCommandResponse resp;
    if (mgmt_send_command(path, &req, &resp, MGMT_SOCKET_TIMEOUT_MS) < 0) {
        fprintf(stderr, "No response from %s (is it running?)\n", module);
        return 1;
    }

    printf("[%s] %s -> %s\n",
           module,
           mgmt_command_str(cmd_type),
           mgmt_result_str(resp.result_code));

    if (resp.payload[0] != '\0')
        printf("%s\n", (const char*)resp.payload);

    return (resp.result_code == MGMT_RESULT_OK) ? 0 : 1;
}
