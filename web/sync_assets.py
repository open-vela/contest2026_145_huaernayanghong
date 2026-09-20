#!/usr/bin/env python3
"""
Phoenix HoloDesk-S1 Web Assets Sync Script
Transforms standalone HTML files in web/ into C constants in core/web_assets.c
"""

import os
import re

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
APP_DIR = os.path.join(REPO_ROOT, "app", "phoenix_agent_app")
CORE_DIR = os.path.join(APP_DIR, "core")

SETUP_HTML_PATH = os.path.join(SCRIPT_DIR, "setup.html")
BLE_SETUP_HTML_PATH = os.path.join(SCRIPT_DIR, "ble_setup.html")
DASHBOARD_HTML_PATH = os.path.join(SCRIPT_DIR, "dashboard.html")

HEADER_OUT = os.path.join(CORE_DIR, "web_assets.h")
SOURCE_OUT = os.path.join(CORE_DIR, "web_assets.c")

def minify_html(html_str):
    # Remove HTML comments (except conditional comments if any)
    html_str = re.sub(r'<!--(?!\s*\[if).*?-->', '', html_str, flags=re.S)
    # Safely collapse lines while preserving newline delimiters to protect JS statements and comments
    processed = []
    for line in html_str.splitlines():
        s = line.strip()
        if not s or s.startswith('//'):
            continue
        processed.append(s)
    return '\n'.join(processed)

def to_c_string_literal(minified):
    lines = []
    chunk = []
    current_len = 0
    for ch in minified:
        if ch == '\\':
            esc = '\\\\'
        elif ch == '"':
            esc = '\\"'
        elif ch == '\n':
            esc = '\\n'
        elif ch == '\r':
            esc = '\\r'
        elif ch == '\t':
            esc = '\\t'
        else:
            esc = ch
        
        chunk.append(esc)
        current_len += len(esc)
        if current_len >= 100:
            lines.append('    "' + ''.join(chunk) + '"')
            chunk = []
            current_len = 0
    if chunk:
        lines.append('    "' + ''.join(chunk) + '"')
    return '\n'.join(lines)

def main():
    print(f"[WebAssets] Reading HTML files from: {SCRIPT_DIR}")
    with open(SETUP_HTML_PATH, 'r', encoding='utf-8') as f:
        setup_raw = f.read()
    with open(BLE_SETUP_HTML_PATH, 'r', encoding='utf-8') as f:
        ble_setup_raw = f.read()
    with open(DASHBOARD_HTML_PATH, 'r', encoding='utf-8') as f:
        dashboard_raw = f.read()

    setup_min = minify_html(setup_raw)
    ble_setup_min = minify_html(ble_setup_raw)
    dashboard_min = minify_html(dashboard_raw)

    print(f"  - setup.html: {len(setup_raw)} bytes -> minified: {len(setup_min)} bytes")
    print(f"  - ble_setup.html: {len(ble_setup_raw)} bytes -> minified: {len(ble_setup_min)} bytes")
    print(f"  - dashboard.html: {len(dashboard_raw)} bytes -> minified: {len(dashboard_min)} bytes")

    header_content = """/**
 * @file web_assets.h
 * @brief Embedded Web Page Assets (Auto-generated from web/setup.html, ble_setup.html & dashboard.html)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_WEB_ASSETS_H
#define PHOENIX_WEB_ASSETS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *phoenix_web_asset_get_setup_html(void);
size_t      phoenix_web_asset_get_setup_html_len(void);

const char *phoenix_web_asset_get_ble_setup_html(void);
size_t      phoenix_web_asset_get_ble_setup_html_len(void);

const char *phoenix_web_asset_get_dashboard_html(void);
size_t      phoenix_web_asset_get_dashboard_html_len(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_WEB_ASSETS_H */
"""

    source_content = f"""/**
 * @file web_assets.c
 * @brief Embedded Web Page Assets Implementation (Auto-generated)
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_assets.h"
#include <string.h>

static const char s_setup_html[] =
{to_c_string_literal(setup_min)};

static const char s_ble_setup_html[] =
{to_c_string_literal(ble_setup_min)};

static const char s_dashboard_html[] =
{to_c_string_literal(dashboard_min)};

const char *phoenix_web_asset_get_setup_html(void)
{{
    return s_setup_html;
}}

size_t phoenix_web_asset_get_setup_html_len(void)
{{
    return sizeof(s_setup_html) - 1;
}}

const char *phoenix_web_asset_get_ble_setup_html(void)
{{
    return s_ble_setup_html;
}}

size_t phoenix_web_asset_get_ble_setup_html_len(void)
{{
    return sizeof(s_ble_setup_html) - 1;
}}

const char *phoenix_web_asset_get_dashboard_html(void)
{{
    return s_dashboard_html;
}}

size_t phoenix_web_asset_get_dashboard_html_len(void)
{{
    return sizeof(s_dashboard_html) - 1;
}}
"""

    with open(HEADER_OUT, 'w', encoding='utf-8') as f:
        f.write(header_content)
    with open(SOURCE_OUT, 'w', encoding='utf-8') as f:
        f.write(source_content)

    print(f"✅ Generated {HEADER_OUT}")
    print(f"✅ Generated {SOURCE_OUT}")

if __name__ == '__main__':
    main()
