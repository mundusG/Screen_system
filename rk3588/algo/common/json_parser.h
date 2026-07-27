/**
 * json_parser.h — 迷你 JSON 解析器 (零依赖, 纯 C 实现)
 *
 * 从 person_efence_wrapper_v2.cpp 提取, 提供:
 *   json_get_string()  — 提取字符串值
 *   json_get_float()   — 提取浮点数 (兼容 int/float/bool)
 *   json_get_int()     — 提取整数
 *   json_get_str_array() — 提取字符串数组
 *
 * 所有函数均为 reentrant, 字符串返回值需调用方 free()。
 */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/** 提取 key 对应的字符串值, 返回 malloc 分配的 char*, 调用方 free */
char* json_get_string(const char* json, const char* key);

/** 提取 key 对应的浮点数, 失败返回 default_val */
float json_get_float(const char* json, const char* key, float default_val);

/** 提取 key 对应的整数, 失败返回 default_val */
int json_get_int(const char* json, const char* key, int default_val);

#ifdef __cplusplus
}
#endif

/** 提取 key 对应的字符串数组, 返回 std::vector<std::string> */
std::vector<std::string> json_get_str_array(const char* json, const char* key);

#endif // JSON_PARSER_H
