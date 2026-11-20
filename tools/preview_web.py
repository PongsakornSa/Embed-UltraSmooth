#!/usr/bin/env python3
"""Preview the gateway dashboard locally, with mock NETPIE data.

The page the ESP32 serves lives inside gateway/include/web_page.h as a C++ raw
string. This server extracts that string on every request, so you can edit
web_page.h and just refresh the browser - no flash needed until you are happy.

Endpoints mirror the firmware routes:

    GET /                  the page from web_page.h
    GET /api/status        mock shadow snapshot (measurements + ages)
    GET /api/history       mock paginated history (state=ready, raw)

Usage:
    python3 tools/preview_web.py                # http://127.0.0.1:8080
    python3 tools/preview_web.py --port 8090
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import time
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent.parent
PAGE_FILE = ROOT / "gateway" / "include" / "web_page.h"
PAGE_PATTERN = re.compile(r'R"WEB\((.*?)\)WEB"', re.S)

METRICS = {
    "mq135": {"field": "mq135_raw", "min": 0, "max": 4095},
    "temperature": {"field": "temperature_c", "min": -50, "max": 150},
    "humidity": {"field": "mq135_rh", "min": 0, "max": 100},
    "light": {"field": "light_lux", "min": 0, "max": 1000000},
    "sound": {"field": "sound_raw", "min": 0, "max": 4095},
    "flame": {"field": "flame_raw", "min": 0, "max": 4095},
}
# Coarser sampling for long windows keeps the preview fast; the real device
# returns raw Feed samples, the UI does not care about the spacing.
STEP_S = {5 / 60: 5, 0.25: 15, 1: 10, 72: 900, 168: 1800, 360: 4500, 720: 9000}

START = time.time()


def read_page() -> str:
    text = PAGE_FILE.read_text(encoding="utf-8")
    match = PAGE_PATTERN.search(text)
    if not match:
        raise RuntimeError(f'no R"WEB(...)WEB" block in {PAGE_FILE}')
    return match.group(1)


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def jitter(name: str, bucket: int) -> float:
    """Deterministic -1..1 noise, stable for the same bucket."""
    digest = hashlib.md5(f"{name}:{bucket}".encode()).digest()
    return int.from_bytes(digest[:4], "big") / 0xFFFFFFFF * 2 - 1


def value_at(metric: str, t_ms: float) -> float:
    t = t_ms / 1000.0
    local = datetime.fromtimestamp(t)
    hour = local.hour + local.minute / 60 + local.second / 3600
    j = jitter(metric, int(t // 300))

    if metric == "temperature":
        value = 24.3 + 1.5 * math.sin(2 * math.pi * (hour - 15) / 24)
        value += 0.25 * math.sin(t / 2400) + 0.15 * j
    elif metric == "humidity":
        value = 55 + 8 * math.sin(2 * math.pi * (hour + 3) / 24) + 2 * j
    elif metric == "light":
        if 7 <= hour < 18:
            value = 650 * math.sin(math.pi * (hour - 6.5) / 11.5) + 40 * j
        elif 18 <= hour < 22:  # exam room lights on
            value = 430 + 30 * j
        else:
            value = 25 + 10 * j
    elif metric == "sound":
        value = 380 + 120 * math.sin(t / 900) + 60 * j
        if 8 <= hour < 16:
            value += 260
    elif metric == "mq135":
        value = 1250 + 120 * math.sin(t / 3600) + 40 * j
        if local.minute in (23, 24):  # brief gas bump
            value += 420
    elif metric == "flame":
        value = 3180 + 30 * j
        if local.hour % 2 == 0 and local.minute == 17 and local.second < 20:
            value = 420  # demo flame event, shows up in the 1h window
    else:
        raise KeyError(metric)

    spec = METRICS[metric]
    return round(clamp(value, spec["min"], spec["max"]), 3)


def fan_state() -> dict:
    temp = value_at("temperature", time.time() * 1000)
    # 3-class air movement preference, loosely tied to temperature.
    more = clamp((temp - 23.5) / 4, 0.02, 0.95)
    less = clamp((26.5 - temp) / 8, 0.02, 0.5)
    same = max(0.01, 1 - more - less)
    return {
        "fan_on": temp >= 25.5,
        "fan_src": "ai",
        "ai_valid": True,
        "ai_probs": {
            "more": round(more, 3),
            "same": round(same, 3),
            "less": round(less, 3),
        },
        "ai_temp": round(temp, 2),
        "ai_suspended": False,
        "ai_threshold_on": 25.5,
    }


def alerts() -> list[dict]:
    now = int(time.time())
    return [
        {"kind": "flame", "at": now - 10 * 60, "message": "ตรวจพบเปลวไฟ 20 วินาที", "sent": True},
        {"kind": "gas", "at": now - 32 * 60, "message": "MQ-135 สูงกว่าค่าอ้างอิง", "sent": True},
        {"kind": "node", "at": now - 74 * 60, "message": "node ขาดการเชื่อมต่อ 45 วินาที", "sent": True},
        {"kind": "gas", "at": now - 3 * 3600, "message": "MQ-135 สูงกว่าค่าอ้างอิง", "sent": True},
    ]


def status_payload() -> dict:
    now = time.time() * 1000
    measurements = {metric: value_at(metric, now) for metric in METRICS}
    ages = {metric: 2 + index for index, metric in enumerate(METRICS)}
    uptime = int(7320 + (time.time() - START))
    payload = {
        "source": "netpie",
        "netpie_connected": True,
        "netpie_http": 200,
        "node_id": "out-01",
        "node_online": True,
        "mq135_r0_ready": True,
        "mq135_r0_adc": 1200,
        "mq135_r0_factor": 5 / (1200 * 3.3 / 4095 * 1.5) - 1,
        "shadow_age_s": 2,
        "sample_age_s": 2,
        "sequence": 1250,
        "sample_id": 1250,
        "warmed_up": True,
        "mq135_raw": measurements["mq135"],
        "last_mq135_raw": measurements["mq135"],
        "temperature": measurements["temperature"],
        "humidity": 55,
        "pm25": None,
        "pm10": None,
        "measurements": measurements,
        "measurement_age_s": ages,
        "received": int(1250 + (time.time() - START) / 5),
        "gaps": 3,
        "duplicates": 0,
        "nodes_online": 1,
        "mqtt_connected": True,
        "ip": "192.168.1.42",
        "rssi": -57,
        "uptime_s": uptime,
        "published": int(1240 + (time.time() - START) / 5),
        "publish_failed": 0,
        "buffered": 0,
        # Ahead of the firmware, so the redesigned UI can be prototyped:
        "fan": fan_state(),
        "alerts": alerts(),
        "discord_ok": True,
        "discord_cooldown_s": 0,
    }
    return payload


def history_payload(query: dict[str, list[str]]) -> tuple[int, dict]:
    try:
        hours = float(query.get("hours", ["1"])[0])
    except ValueError:
        hours = -1
    if hours not in STEP_S:
        return 400, {"error": "hours must be 0.08333333333333333, 0.25, 1, 72, 168, 360 or 720"}

    metric = query.get("metric", ["mq135"])[0]
    if metric not in METRICS:
        return 400, {"error": "unknown metric"}

    step_ms = STEP_S[hours] * 1000
    now_ms = int(time.time() * 1000)
    try:
        end_ms = int(query.get("until", [now_ms])[0])
        start_ms = int(query.get("after", [end_ms - hours * 3600 * 1000])[0])
    except ValueError:
        return 400, {"error": "invalid time window"}

    first = math.ceil(start_ms / step_ms) * step_ms
    points = []
    t = first
    while t <= end_ms and len(points) < 300:
        points.append([t, value_at(metric, t)])
        t += step_ms
    next_after = t if (len(points) == 300 and t < end_ms) else None

    return 200, {
        "metric": metric,
        "field": METRICS[metric]["field"],
        "source": "netpie-feed",
        "hours": hours,
        "aggregation": "raw",
        "start_ms": first,
        "end_ms": end_ms,
        "next_after": next_after,
        "state": "ready",
        "http": 200,
        "points": points,
    }


class Handler(BaseHTTPRequestHandler):
    server_version = "ExamBarierPreview/1.0"

    def reply(self, code: int, body: bytes, content_type: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        parsed = urlparse(self.path)
        if parsed.path == "/":
            try:
                page = read_page().encode("utf-8")
            except Exception as error:  # surface a useful message instead of a blank page
                self.reply(500, f"preview error: {error}".encode(), "text/plain; charset=utf-8")
                return
            self.reply(200, page, "text/html; charset=utf-8")
        elif parsed.path == "/api/status":
            body = json.dumps(status_payload(), ensure_ascii=False).encode()
            self.reply(200, body, "application/json; charset=utf-8")
        elif parsed.path == "/api/history":
            code, payload = history_payload(parse_qs(parsed.query))
            body = json.dumps(payload, ensure_ascii=False).encode()
            self.reply(code, body, "application/json; charset=utf-8")
        elif parsed.path == "/favicon.ico":
            self.reply(204, b"", "image/x-icon")
        else:
            self.reply(404, b"Not found", "text/plain; charset=utf-8")

    def log_message(self, fmt: str, *args) -> None:
        if args and str(args[1]).startswith(("4", "5")):
            print(f"[preview] {self.address_string()} {fmt % args}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    read_page()  # fail fast if web_page.h changed shape
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"[preview] serving {PAGE_FILE.name} at http://{args.host}:{args.port}/")
    print("[preview] mock data: /api/status, /api/history (15m raw/1h/6h/24h/7d/30d)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
