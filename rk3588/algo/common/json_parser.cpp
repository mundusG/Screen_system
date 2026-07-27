/**
 * json_parser.cpp — 迷你 JSON 解析器实现
 * 从 person_efence_wrapper_v2.cpp 提取
 */

#include "json_parser.h"

char* json_get_string(const char* json, const char* key) {
    if (!json || !key) return nullptr;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* p = strstr(json, search);
    if (!p) return nullptr;
    p = strchr(p + strlen(search), ':');
    if (!p) return nullptr;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return nullptr;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return nullptr;
    size_t len = (size_t)(end - p);
    char* value = (char*)malloc(len + 1);
    if (!value) return nullptr;
    memcpy(value, p, len);
    value[len] = '\0';
    return value;
}

float json_get_float(const char* json, const char* key, float default_val) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* p = strstr(json, search);
    if (!p) return default_val;
    p = strchr(p + strlen(search), ':');
    if (!p) return default_val;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (strncmp(p, "true", 4) == 0) return 1.0f;
    if (strncmp(p, "false", 5) == 0) return 0.0f;
    if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.') {
        return strtof(p, nullptr);
    }
    // 尝试作为字符串值解析
    char* s = json_get_string(json, key);
    if (!s) return default_val;
    float v = strtof(s, nullptr);
    free(s);
    return v;
}

int json_get_int(const char* json, const char* key, int default_val) {
    return (int)json_get_float(json, key, (float)default_val);
}

std::vector<std::string> json_get_str_array(const char* json, const char* key) {
    std::vector<std::string> result;
    if (!json || !key) return result;

    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* p = strstr(json, search);
    if (!p) return result;
    p = strchr(p + strlen(search), ':');
    if (!p) return result;
    // 跳过空白到 [
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':') p++;
    if (*p != '[') return result;
    p++;

    while (p && *p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) p++;
        if (*p == '\0' || *p == ']') break;
        if (*p != '"') { p++; continue; }
        p++;
        const char* end = strchr(p, '"');
        if (!end) break;
        result.push_back(std::string(p, (size_t)(end - p)));
        p = end + 1;
    }
    return result;
}
