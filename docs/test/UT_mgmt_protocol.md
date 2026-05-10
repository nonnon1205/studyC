# UT 項目表 — mgmt_protocol.c

## テスト対象ファイル

`Mgmt/src/mgmt_protocol.c`
テストコード: `tests/unit/unity/mgmt/test_mgmt_protocol.c`

---

## mgmt_request_validate

| No. | テスト関数名 | 入力条件 | 期待結果 | 種別 |
|---|---|---|---|---|
| 1 | `test_validate_null_returns_false` | `req = NULL` | 0 | 異常 |
| 2 | `test_validate_valid_request` | 全フィールド正常 | 1 | 正常 |
| 3 | `test_validate_invalid_cmd_type` | `cmd_type = MGMT_CMD_MAX` | 0 | 異常 |
| 4 | `test_validate_empty_module_returns_false` | `target_module = ""` | 0 | 異常 |
| 5 | `test_validate_payload_len_at_max` | `payload_len = MGMT_PAYLOAD_REQUEST_SIZE`（上限ピッタリ） | 1 | 境界 |
| 6 | `test_validate_payload_len_overflow_returns_false` | `payload_len = MGMT_PAYLOAD_REQUEST_SIZE + 1` | 0 | 境界 |

---

## mgmt_result_str

全 case を網羅（ホワイトボックス・分岐網羅）。

| No. | テスト関数名 | 入力条件 | 期待結果 | 種別 |
|---|---|---|---|---|
| 7 | `test_result_str_ok` | `MGMT_RESULT_OK` | `"OK"` | 正常 |
| 8 | `test_result_str_invalid_cmd` | `MGMT_RESULT_INVALID_CMD` | `"INVALID_CMD"` | 正常 |
| 9 | `test_result_str_module_not_found` | `MGMT_RESULT_MODULE_NOT_FOUND` | `"MODULE_NOT_FOUND"` | 正常 |
| 10 | `test_result_str_handler_failed` | `MGMT_RESULT_HANDLER_FAILED` | `"HANDLER_FAILED"` | 正常 |
| 11 | `test_result_str_timeout` | `MGMT_RESULT_TIMEOUT` | `"TIMEOUT"` | 正常 |
| 12 | `test_result_str_buffer_overflow` | `MGMT_RESULT_BUFFER_OVERFLOW` | `"BUFFER_OVERFLOW"` | 正常 |
| 13 | `test_result_str_unauthorized` | `MGMT_RESULT_UNAUTHORIZED` | `"UNAUTHORIZED"` | 正常 |
| 14 | `test_result_str_internal_error` | `MGMT_RESULT_INTERNAL_ERROR` | `"INTERNAL_ERROR"` | 正常 |
| 15 | `test_result_str_unknown` | `0xFF`（未定義コード） | `"UNKNOWN"` | 異常 |

---

## mgmt_command_str

全 case を網羅（ホワイトボックス・分岐網羅）。`MGMT_CMD_ENABLE_TRACING` は定義済み上限境界値。

| No. | テスト関数名 | 入力条件 | 期待結果 | 種別 |
|---|---|---|---|---|
| 16 | `test_command_str_ping` | `MGMT_CMD_PING` | `"PING"` | 正常 |
| 17 | `test_command_str_set_log_level` | `MGMT_CMD_SET_LOG_LEVEL` | `"SET_LOG_LEVEL"` | 正常 |
| 18 | `test_command_str_get_status` | `MGMT_CMD_GET_STATUS` | `"GET_STATUS"` | 正常 |
| 19 | `test_command_str_get_metrics` | `MGMT_CMD_GET_METRICS` | `"GET_METRICS"` | 正常 |
| 20 | `test_command_str_set_buffer_size` | `MGMT_CMD_SET_BUFFER_SIZE` | `"SET_BUFFER_SIZE"` | 正常 |
| 21 | `test_command_str_enable_profiling` | `MGMT_CMD_ENABLE_PROFILING` | `"ENABLE_PROFILING"` | 正常 |
| 22 | `test_command_str_reset_metrics` | `MGMT_CMD_RESET_METRICS` | `"RESET_METRICS"` | 正常 |
| 23 | `test_command_str_shutdown` | `MGMT_CMD_SHUTDOWN` | `"SHUTDOWN"` | 正常 |
| 24 | `test_command_str_get_config` | `MGMT_CMD_GET_CONFIG` | `"GET_CONFIG"` | 正常 |
| 25 | `test_command_str_enable_tracing` | `MGMT_CMD_ENABLE_TRACING` | `"ENABLE_TRACING"` | 境界 |
| 26 | `test_command_str_max_boundary` | `MGMT_CMD_MAX`（範囲外） | `"UNKNOWN"` | 境界 |
| 27 | `test_command_str_unknown` | `0xFF`（未定義コード） | `"UNKNOWN"` | 異常 |

---

## mgmt_response_latency_us

| No. | テスト関数名 | 入力条件 | 期待結果 | 種別 |
|---|---|---|---|---|
| 28 | `test_latency_null_returns_zero` | `resp = NULL` | 0 | 異常 |
| 29 | `test_latency_normal` | req=1000s / resp=1001s | 1000000μs | 正常 |
| 30 | `test_latency_sub_second` | req=1000s 0ns / resp=1000s 500ms | 500000μs | 正常 |
| 31 | `test_latency_same_timestamp` | req と resp が同一時刻 | 0 | 境界 |
| 32 | `test_latency_response_before_request_returns_zero` | resp < req（時刻逆転） | 0 | 境界 |

---

## mgmt_request_init

| No. | テスト関数名 | 入力条件 | 期待結果 | 種別 |
|---|---|---|---|---|
| 33 | `test_init_sets_cmd_type` | `cmd_type = MGMT_CMD_GET_STATUS` | `req.cmd_type == MGMT_CMD_GET_STATUS` | 正常 |
| 34 | `test_init_copies_module_name` | `module = "collector"` | `req.target_module == "collector"` | 正常 |
| 35 | `test_init_null_module_leaves_empty` | `module = NULL` | `req.target_module[0] == '\0'` | 異常 |
| 36 | `test_init_payload_copied` | `payload = "hello"` | `req.payload == "hello"`, `payload_len` 一致 | 正常 |
| 37 | `test_init_payload_truncated_at_max` | `payload` サイズ = `MGMT_PAYLOAD_REQUEST_SIZE + 64` | `payload_len == MGMT_PAYLOAD_REQUEST_SIZE` | 境界 |
| 38 | `test_init_null_payload_len_zero` | `payload = NULL, len = 5` | `payload_len == 0` | 異常 |
| 39 | `test_init_module_name_truncated_null_terminated` | `module` の長さ = `MGMT_MODULE_NAME_SIZE + 9` | `target_module[MGMT_MODULE_NAME_SIZE - 1] == '\0'` | 境界 |
| 40 | `test_init_null_req_does_not_crash` | `req = NULL` | クラッシュしない | 異常 |
| 51 | `test_init_nonnull_payload_zero_len` | `payload = "x", len = 0` | `payload_len == 0` | 境界 |

---

## mgmt_response_init

| No. | テスト関数名 | 入力条件 | 期待結果 | 種別 |
|---|---|---|---|---|
| 41 | `test_resp_init_null_does_not_crash` | `resp = NULL` | クラッシュしない | 異常 |
| 42 | `test_resp_init_sets_request_id` | `req_id = 42` | `resp.request_id == 42` | 正常 |
| 43 | `test_resp_init_sets_result_code` | `result = MGMT_RESULT_TIMEOUT` | `resp.result_code == MGMT_RESULT_TIMEOUT` | 正常 |
| 44 | `test_resp_init_copies_module_name` | `module = "router"` | `resp.source_module == "router"` | 正常 |
| 45 | `test_resp_init_null_module_leaves_empty` | `module = NULL` | `resp.source_module[0] == '\0'` | 異常 |
| 46 | `test_resp_init_payload_copied` | `payload = "hello"` | `resp.payload == "hello"` | 正常 |
| 47 | `test_resp_init_null_payload_no_crash` | `payload = NULL, len = 5` | `resp.payload[0] == '\0'` | 異常 |
| 48 | `test_resp_init_payload_len_at_max` | `len = MGMT_PAYLOAD_RESPONSE_SIZE`（上限ピッタリ） | 末尾バイトが正常コピー | 境界 |
| 49 | `test_resp_init_payload_truncated_at_max` | `len = MGMT_PAYLOAD_RESPONSE_SIZE + 64` | 先頭・末尾バイトが正常コピー | 境界 |
| 50 | `test_resp_init_module_truncated_null_terminated` | `module` の長さ = `MGMT_MODULE_NAME_SIZE + 9` | `source_module[MGMT_MODULE_NAME_SIZE - 1] == '\0'` | 境界 |
| 52 | `test_resp_init_nonnull_payload_zero_len` | `payload = "x", len = 0` | `resp.payload[0] == '\0'` | 境界 |

---

## 実行結果

```
52 Tests  0 Failures  0 Ignored  OK
```
