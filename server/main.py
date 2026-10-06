"""Entry point backend kiosk check-in / nhắc lịch hẹn.

Chạy song song:
  - MQTT Service: thread nền của paho-mqtt (lắng nghe kiosk ESP32, tự kết nối lại)
  - REST API (FastAPI + uvicorn): luồng chính, phục vụ Web UI tại http://127.0.0.1:8000

    python main.py              # tạo DB (nếu chưa có), chạy MQTT + API
    python main.py --seed       # nạp lại dữ liệu mẫu cho hôm nay rồi chạy
    python main.py --reset      # xoá bảng, tạo lại schema rồi chạy
    python main.py --no-api     # chỉ chạy MQTT (như phiên bản trước)
    python main.py --no-mqtt    # chỉ chạy API (phát triển giao diện khi không có broker)
"""
import argparse
import logging
import sys

import config
import models
from mqtt_service import MqttService


def main() -> int:
    parser = argparse.ArgumentParser(description="Backend MQTT + REST API cho kiosk check-in ESP32")
    parser.add_argument("--seed", action="store_true", help="nạp lại dữ liệu mẫu trước khi chạy")
    parser.add_argument("--reset", action="store_true", help="xoá và tạo lại các bảng trước khi chạy")
    parser.add_argument("--no-api", action="store_true", help="không chạy REST API")
    parser.add_argument("--no-mqtt", action="store_true", help="không kết nối MQTT broker")
    parser.add_argument("--host", default=config.API_HOST, help=f"địa chỉ API (mặc định {config.API_HOST})")
    parser.add_argument("--port", type=int, default=config.API_PORT, help=f"cổng API (mặc định {config.API_PORT})")
    parser.add_argument("--verbose", "-v", action="store_true", help="log chi tiết (DEBUG)")
    args = parser.parse_args()
    if args.no_api and args.no_mqtt:
        parser.error("--no-api và --no-mqtt không thể dùng cùng lúc")

    # Console Windows có thể không phải UTF-8 -> tránh lỗi khi log tên tiếng Việt
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)-7s %(name)s: %(message)s",
        stream=sys.stdout,
    )
    log = logging.getLogger("kiosk")

    models.init_db(reset=args.reset)
    if args.seed:
        models.seed_sample_data()
        log.info("Da nap du lieu mau cho ngay %s vao %s", models.today_str(), config.DB_PATH)

    service = None if args.no_mqtt else MqttService()

    if args.no_api:
        try:
            service.run_forever()
        except KeyboardInterrupt:
            log.info("Dang dung...")
            service.stop()
        return 0

    # --- [MỚI] Chạy song song: MQTT ở thread nền, FastAPI/uvicorn ở luồng chính ---
    import uvicorn
    from api import app

    app.state.mqtt_service = service
    if service is not None:
        service.start()  # paho loop_start(): thread riêng, tự kết nối lại khi mất broker

    log.info("REST API: http://%s:%s/api/appointments  (Swagger: /docs)", args.host, args.port)
    if config.WEB_UI_DIST.is_dir():
        log.info("Web UI: http://%s:%s/", args.host, args.port)
    try:
        # uvicorn tự xử lý Ctrl+C và trả về khi dừng
        uvicorn.run(app, host=args.host, port=args.port, log_level="info")
    finally:
        if service is not None:
            log.info("Dang dung MQTT...")
            service.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
