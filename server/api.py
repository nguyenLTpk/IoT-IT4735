"""REST API (FastAPI) cho Web UI quản lý lịch hẹn.

Endpoints:
  GET    /api/appointments?date=YYYY-MM-DD   Danh sách lịch hẹn trong ngày (mặc định hôm nay)
  POST   /api/appointments                   Thêm lịch hẹn (STT để trống -> tự cấp)
  DELETE /api/appointments/{id}              Xoá lịch hẹn
  GET    /api/health                         Trạng thái API + kết nối MQTT

Tài liệu tương tác (Swagger): http://127.0.0.1:8000/docs
"""
import re
from contextlib import asynccontextmanager, closing
from datetime import date
from typing import List, Optional

from fastapi import FastAPI, HTTPException, Query, Response, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, ConfigDict, Field, field_validator

import config
import models


@asynccontextmanager
async def lifespan(_app):
    # Tạo / nâng cấp schema khi API khởi động - để `uvicorn api:app --reload` cũng chạy được
    # trên DB mới (idempotent, an toàn khi main.py đã gọi trước đó)
    models.init_db()
    yield


app = FastAPI(title="Kiosk Check-in API", version="1.0.0", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=config.API_CORS_ORIGINS,
    allow_methods=["GET", "POST", "DELETE"],
    allow_headers=["Content-Type"],
)

# main.py gán MqttService vào đây để /api/health báo trạng thái kết nối broker
app.state.mqtt_service = None

_CONTROL_CHARS = re.compile(r"[\x00-\x1f\x7f]")


class AppointmentIn(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)

    # MSSV: 6-10 chữ số ASCII (giống quy tắc QR MSSV trong firmware). Dùng [0-9] chứ không
    # dùng \d: \d của pydantic nhận cả chữ số Unicode như '２０２１', thứ máy quét không bao giờ gửi
    student_id: str = Field(pattern=r"^[0-9]{6,10}$", examples=["20210001"])
    student_name: str = Field(min_length=1, max_length=60, examples=["Nguyễn Văn An"])
    # HH:MM 24h có số 0 đứng đầu -> sắp xếp chuỗi đúng thứ tự thời gian
    appointment_time: str = Field(pattern=r"^([01][0-9]|2[0-3]):[0-5][0-9]$", examples=["10:30"])
    # Giới hạn để gói verify_result gửi xuống ESP32 luôn vừa buffer MQTT 1024 byte
    message: str = Field(default="", max_length=120, examples=["Vao quay so 3"])
    queue_number: Optional[int] = Field(default=None, ge=1, le=9999)
    appointment_date: Optional[date] = None  # Mặc định: hôm nay

    @field_validator("student_name", "message")
    @classmethod
    def no_control_chars(cls, v: str) -> str:
        if _CONTROL_CHARS.search(v):
            raise ValueError("không được chứa ký tự điều khiển (xuống dòng, tab...)")
        return v


class AppointmentOut(BaseModel):
    id: int
    student_id: str
    student_name: str
    appointment_date: str
    appointment_time: str
    queue_number: int
    status: str
    message: str
    checked_in_at: Optional[str] = None


def _db():
    return closing(models.connect())


@app.get("/api/health")
def health():
    service = app.state.mqtt_service
    return {
        "status": "ok",
        "mqtt_enabled": service is not None,
        "mqtt_connected": bool(service and service.is_connected),
        "broker": f"{config.MQTT_HOST}:{config.MQTT_PORT}",
        "today": models.today_str(),
    }


@app.get("/api/appointments", response_model=List[AppointmentOut])
def list_appointments(day: Optional[date] = Query(default=None, alias="date",
                                                  description="YYYY-MM-DD, mặc định hôm nay")):
    with _db() as conn:
        return [a.to_dict() for a in models.list_appointments(conn, day.isoformat() if day else None)]


@app.post("/api/appointments", response_model=AppointmentOut, status_code=status.HTTP_201_CREATED)
def create_appointment(body: AppointmentIn):
    with _db() as conn:
        try:
            appt = models.create_appointment(
                conn,
                student_id=body.student_id,
                student_name=body.student_name,
                appointment_time=body.appointment_time,
                message=body.message,
                queue_number=body.queue_number,
                day=body.appointment_date.isoformat() if body.appointment_date else None,
            )
        except models.QueueNumberTaken:
            raise HTTPException(status.HTTP_409_CONFLICT,
                                detail=f"STT {body.queue_number} đã được dùng trong ngày này")
    return appt.to_dict()


@app.delete("/api/appointments/{appointment_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_appointment(appointment_id: int):
    with _db() as conn:
        if not models.delete_appointment(conn, appointment_id):
            raise HTTPException(status.HTTP_404_NOT_FOUND, detail="Không tìm thấy lịch hẹn")
    return Response(status_code=status.HTTP_204_NO_CONTENT)


# Phục vụ luôn Web UI đã build (web_ui/dist) tại "/" -> chỉ cần chạy 1 tiến trình khi triển khai.
# Mount SAU các route /api để không che mất API.
if config.WEB_UI_DIST.is_dir():
    app.mount("/", StaticFiles(directory=str(config.WEB_UI_DIST), html=True), name="web_ui")
