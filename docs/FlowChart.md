# Lưu đồ hoạt động ESP32WiFiPortal 1.1.1

Tài liệu này mô tả luồng xử lý hiện tại của thư viện cho cả chế độ blocking và
non-blocking. Hai chế độ dùng chung hàm `process()` và cùng một đường dọn dẹp.

## Khởi động và kết nối credential đã lưu

```mermaid
flowchart TD
    A[Ứng dụng khởi động] --> B{Có gọi setPortalIP?}
    B -- Không --> C[Dùng mặc định 192.168.4.1/24]
    B -- Có --> D{IP private, gateway và subnet hợp lệ?}
    D -- Không --> E[Trả false và ghi lastError]
    D -- Có --> F[Lưu cấu hình Portal IP trong object]
    C --> G[connectSaved hoặc autoConnect]
    F --> G
    G --> H{Có credential trong NVS?}
    H -- Không --> I{Đang dùng autoConnect?}
    H -- Có --> J[Bắt đầu STA connection]
    J --> K{Kết nối trước timeout?}
    K -- Có --> L[State = Connected]
    K -- Không --> M[WiFi.disconnect và State = Failed]
    I -- Có --> N[Mở Config Portal blocking]
    I -- Không --> O[Trả false cho ứng dụng]
    M --> I
```

`connectSaved()` chỉ đọc namespace `ewp_wifi`; lỗi kết nối không xóa credential
đã lưu. Nếu một Portal đang hoạt động, thư viện dừng và dọn Portal trước khi
chuyển sang `WIFI_STA`.

## Khởi tạo Config Portal

```mermaid
flowchart TD
    A[startConfigPortal hoặc startConfigPortalAsync] --> B[stopConfigPortal để dọn phiên cũ]
    B --> C[Chuyển sang WIFI_AP_STA]
    C --> D[Áp dụng local IP, gateway, subnet bằng softAPConfig]
    D --> E{Cấu hình IP thành công?}
    E -- Không --> X[Cleanup và State = Failed]
    E -- Có --> F[Khởi động SoftAP]
    F --> G{SoftAP thành công?}
    G -- Không --> X
    G -- Có --> H[Khởi động WebServer cổng 80]
    H --> I[Khởi động wildcard DNS cổng 53]
    I --> J{DNS thành công?}
    J -- Không --> X
    J -- Có --> K[State = Portal và gọi onPortalStarted]
```

Portal IP được giữ cố định trong suốt phiên đang chạy. `setPortalIP()` trả
`false` nếu được gọi khi Portal còn active, nhờ đó SoftAP, DNS và HTTP redirect
luôn dùng cùng một địa chỉ.

## Nhận và thử credential mới

```mermaid
flowchart TD
    A[POST /save] --> B{SSID và password có độ dài hợp lệ?}
    B -- Không --> C[HTTP 400]
    B -- Có --> D{Đã có attempt pending hoặc active?}
    D -- Có --> E[HTTP 409]
    D -- Không --> F[Giữ credential mới tạm thời trong RAM]
    F --> G[Trả trang Connecting]
    G --> H[process gọi WiFi.begin sau khoảng trễ ngắn]
    H --> I{STA đã connected?}
    I -- Chưa --> J{Đã quá connect timeout?}
    J -- Chưa --> I
    J -- Có --> K[Hủy STA attempt và xóa credential tạm]
    K --> L[State = Portal để người dùng thử lại]
    I -- Có --> M[Ghi credential mới vào Preferences/NVS]
    M --> N{Ghi thành công?}
    N -- Không --> O[Ngắt candidate STA, xóa dữ liệu tạm, giữ Portal]
    N -- Có --> P[Gọi callback, dừng Portal, giữ STA connected]
    P --> Q[State = Connected]
```

Credential cũ trong NVS không bị thay đổi khi candidate không kết nối được hoặc
khi Portal hết thời gian. Việc ghi chỉ diễn ra sau khi `WL_CONNECTED`.

## Timeout, stop và restart Portal

```mermaid
flowchart TD
    A[Portal timeout, stop, restart hoặc destructor] --> B[Đánh dấu Portal inactive]
    B --> C{Candidate đã gọi WiFi.begin?}
    C -- Có --> D[WiFi.disconnect false, false]
    C -- Không --> E[Không ngắt STA độc lập]
    D --> F[Reset pending và active flags]
    E --> F
    F --> G[Xóa timestamp, SSID và password tạm]
    G --> H[Dừng WebServer và DNS]
    H --> I[Tắt SoftAP]
    I --> J{Nguyên nhân là Portal timeout?}
    J -- Có --> K{Có candidate active lúc timeout?}
    K -- Có --> M[State = Failed]
    K -- Không --> L[State = Connected nếu STA cũ còn kết nối, ngược lại Failed]
    J -- Không --> N[State = Connected hoặc Idle theo trạng thái STA]
```

Trong blocking mode, vòng lặp kết thúc ngay khi `_portalActive` thành `false`.
Trong non-blocking mode, ứng dụng phải gọi `process()` thường xuyên. Vì cả hai
đều dùng cùng state machine, hành vi timeout và cleanup là nhất quán, không còn
STA attempt tiếp tục chạy nền sau khi state đã chuyển sang thất bại.

## Quản lý credential chủ động

```mermaid
flowchart LR
    A[eraseCredentials false] --> B[Xóa namespace ewp_wifi]
    B --> C[Không chủ động ngắt Wi-Fi]
    D[eraseCredentials true] --> F[Xóa namespace ewp_wifi]
    F --> E[Dừng và cleanup Portal nếu đang chạy]
    E --> G[Ngắt Wi-Fi và xóa cấu hình Wi-Fi của core]
    G --> H[State = Idle]
```

`eraseCredentials()` là thao tác xóa duy nhất do API công khai yêu cầu. Connect
timeout, Portal timeout và `stopConfigPortal()` không xóa credential đã lưu.
