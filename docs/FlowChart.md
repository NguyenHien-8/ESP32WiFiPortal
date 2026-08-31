# Lưu đồ hoạt động ESP32WiFiPortal 1.1.1

Tài liệu này mô tả state machine chung cho kết nối blocking, Config Portal
non-blocking, Wi-Fi event, retry và Auto Reconnect.

## Cấu hình địa chỉ STA

```mermaid
flowchart TD
    A[Ứng dụng chọn cấu hình STA] --> B{Gọi setSTAStaticIP?}
    B -- Không --> C[DHCP mặc định]
    B -- Có --> D{IP và Gateway là host khác nhau cùng subnet?}
    D -- Không --> E[Trả false và giữ cấu hình cũ]
    D -- Có --> F{DNS hợp lệ?}
    F -- Không --> E
    F -- Có --> G[Lưu Static IP và DNS trong object]
    C --> H[Trước WiFi.begin gọi WiFi.config với địa chỉ zero]
    G --> I[Trước WiFi.begin gọi WiFi.config với Static IP và DNS]
    H --> J[Chỉ cấu hình STA]
    I --> J
```

`useSTADHCP()` chọn lại DHCP cho lần kết nối do thư viện quản lý tiếp theo. Cấu
hình STA không đọc hoặc thay đổi Portal IP của SoftAP.

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
    H -- Có --> J[Áp dụng DHCP hoặc Static STA IP và bắt đầu connection]
    J --> K{Kết nối trước timeout?}
    K -- Có --> L[State = Connected]
    K -- Không --> M[WiFi.disconnect, trả false và đặt lịch credential đã lưu]
    I -- Có --> N[Mở Config Portal blocking]
    I -- Không --> O[Trả false cho ứng dụng]
    M --> I
```

`connectSaved()` chỉ đọc namespace `ewp_wifi`; lỗi kết nối không xóa credential
đã lưu. Nếu một Portal đang hoạt động, thư viện dừng và dọn Portal trước khi
chuyển sang `WIFI_STA`. Khi Auto Reconnect bật, một lần blocking thất bại vẫn
đặt lịch thử lại non-blocking; `autoConnect()` sẽ hủy lịch này khi chuyển ngay
sang Portal nên không có hai owner kết nối.

## Khởi tạo Config Portal

```mermaid
flowchart TD
    A[startConfigPortal hoặc startConfigPortalAsync] --> B[stopConfigPortal để dọn phiên cũ]
    B --> C[Chuyển sang WIFI_AP_STA]
    C --> D[Áp dụng local IP, gateway, subnet bằng softAPConfig]
    D --> E{Cấu hình IP thành công?}
    E -- Không --> X[Cleanup và phục hồi credential cũ nếu STA đang offline]
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

## Wi-Fi scan non-blocking trong Portal

```mermaid
flowchart TD
    A[GET /scan] --> B{Scan state}
    B -- Idle --> C[Start scan async]
    C --> D[HTTP 202 scanning]
    B -- Scanning --> E{scanComplete}
    E -- Running --> D
    E -- Ready --> F[State = Ready]
    E -- Failed hoặc timeout --> G[HTTP 503 và cleanup]
    D --> H[Browser chờ 400 ms rồi poll lại]
    H --> A
    B -- Ready --> I[Build JSON từ kết quả]
    F --> I
    I --> J[scanDelete, HTTP 200]
```

Scan driver chạy asynchronous nên mỗi lần `process()` chỉ poll trạng thái rồi
tiếp tục phục vụ DNS, HTTP và state machine. Portal stop hoặc một yêu cầu kết nối
hợp lệ sẽ hủy scan đang chạy và giải phóng kết quả; không có hai scan đồng thời.

## Nhận và thử credential mới

```mermaid
flowchart TD
    A[POST /save] --> B{SSID và password có độ dài hợp lệ?}
    B -- Không --> C[HTTP 400]
    B -- Có --> D{Đã có attempt pending hoặc active?}
    D -- Có --> E[HTTP 409]
    D -- Không --> F[Giữ credential mới tạm thời trong RAM]
    F --> G[Trả trang Connecting]
    G --> H[Phase Disconnect STA nếu cần]
    H --> U[Phase Settling bằng millis]
    U --> V[Phase apply STA config]
    V --> W[Phase WiFi.begin]
    W --> I{STA đã connected?}
    I -- Chưa --> J{Đã quá connect timeout?}
    J -- Chưa --> I
    J -- Có --> K{Credential mới bị từ chối rõ ràng?}
    K -- Có --> L[Hủy attempt, không lưu NVS, giữ Portal]
    K -- Không --> R{Còn retry đã cấu hình?}
    R -- Có --> S[Đặt lịch retry bằng millis và backoff]
    S --> H
    R -- Không --> T[Xóa credential tạm, giữ Portal]
    I -- Có --> M[Ghi credential mới vào Preferences/NVS]
    M --> N{Ghi thành công?}
    N -- Không --> O[Ngắt candidate STA, xóa dữ liệu tạm, giữ Portal]
    N -- Có --> P[Gọi callback, dừng Portal, giữ STA connected]
    P --> Q[State = Connected]
```

Credential cũ trong NVS không bị thay đổi khi candidate không kết nối được hoặc
khi Portal hết thời gian. Việc ghi chỉ diễn ra sau khi `WL_CONNECTED`. Handshake
timeout không tự động chứng minh password sai vì cũng có thể do sóng yếu hoặc AP
đang restart.

## Wi-Fi event và Auto Reconnect

```mermaid
flowchart TD
    A[Arduino Wi-Fi event task] --> B{Event nào?}
    B -- STA Connected --> C[Set atomic Connected bit]
    B -- Got IP --> D[Set atomic Got-IP bit]
    B -- Disconnected --> E[Lưu reason và set atomic Disconnect bit]
    C --> F[Callback kết thúc ngay]
    D --> F
    E --> F
    G[Ứng dụng gọi process] --> H[Atomic exchange để lấy event]
    H --> I[Log ngắn và cập nhật state]
    I --> J{Disconnect khi hoạt động bình thường?}
    J -- Không --> K[Để owner hiện tại xử lý attempt]
    J -- Có --> N[Đặt lịch Reconnect sau retry interval]
    N --> S[Disconnect nếu cần, Settling, Config, WiFi.begin]
    S --> O[Thử hữu hạn với exponential backoff]
    O --> P{Kết nối được?}
    P -- Có --> Q[State = Connected, reset retry]
    P -- Không --> R[Cooldown bằng max retry interval]
    R --> N
```

Arduino-ESP32 Auto Reconnect được tắt khi event handler của thư viện được cài.
Nhờ đó chỉ có state machine này gọi `WiFi.begin()` và hủy attempt. Lỗi mất AP là
tạm thời nên sau cooldown thiết bị vẫn có thể phục hồi. Với credential đã lưu,
`AUTH_FAIL` và handshake timeout cũng tiếp tục theo cooldown vì các reason này
có thể xuất hiện do packet loss, AP quá tải hoặc router restart. Nếu password
thực sự đã đổi, cooldown giới hạn tần suất thử và tránh reconnect storm. Callback
không gọi Serial, DNS, WebServer, Preferences hoặc API Wi-Fi blocking.

## Timeout, stop và restart Portal

```mermaid
flowchart TD
    A[Portal timeout hoặc stop] --> B[Đánh dấu Portal inactive]
    B --> C{Candidate đã gọi WiFi.begin?}
    C -- Có --> D[WiFi.disconnect false, false]
    C -- Không --> E[Không ngắt STA độc lập]
    D --> F[Reset pending và active flags]
    E --> F
    F --> G[Xóa timestamp, SSID và password tạm]
    G --> H[Dừng WebServer và DNS]
    H --> I[Tắt SoftAP]
    I --> J{STA vẫn connected?}
    J -- Có --> K[State = Connected]
    J -- Không --> L{Auto Reconnect bật và có credential cũ?}
    L -- Có --> M[Đặt lịch credential cũ, State = Connecting]
    L -- Không --> N[Timeout: Failed; stop chủ động: Idle]
```

Trong blocking mode, vòng lặp kết thúc ngay khi `_portalActive` thành `false`.
Trong non-blocking mode, ứng dụng phải gọi `process()` thường xuyên. Vì cả hai
đều dùng cùng state machine, hành vi timeout và cleanup là nhất quán, không còn
STA candidate tiếp tục chạy nền. Nếu credential cũ hợp lệ còn trong cache/NVS,
`process()` phục hồi nó bằng Auto Reconnect non-blocking và không tự mở lại Portal.
Khi restart Portal, lịch phục hồi tạm thời được hủy trước khi phiên Portal mới bắt
đầu nên không có hai owner gọi `WiFi.begin()`.

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

## Luồng hoạt động thư viện ESP32WiFiPortal

```
KHỞI ĐỘNG
    ↓
Đọc credential từ NVS → cache RAM
    ↓
Thử STA connection
    │
    ├──────── Thành công
    │             ↓
    │         Connected
    │             ↓
    │      Theo dõi WiFi Event
    │             ↓
    │        WiFi Disconnect
    │             ↓
    │      Schedule Auto Reconnect
    │             ↓
    │          WIFI_STA
    │             ↓
    │       Reconnect WiFi cũ
    │          │
    │          ├── Success
    │          │      ↓
    │          │  Connected
    │          │
    │          └── Fail
    │                 ↓
    │            Retry/Backoff
    │                 ↓
    │            Hết retry burst
    │                 ↓
    │              Cooldown
    │                 ↓
    │          Reconnect WiFi cũ
    │                 ↓
    │                ...
    │
    └──────── Thất bại lúc startup
                  ↓
             Config Portal
                  ↓
              WIFI_AP_STA
                  ↓
          DNS + WebServer + SoftAP
                  ↓
              Scan WiFi
                  ↓
          User nhập credential mới
                  ↓
             Thử kết nối
              │
              ├── Fail
              │     ↓
              │  Giữ Portal
              │
              │  Nếu Portal timeout/stop
              │        ↓
              │  Credential cũ còn?
              │        │
              │    ┌───┴───┐
              │   Yes      No
              │    ↓        ↓
              │ Tắt AP    Idle/Failed
              │    ↓
              │ WIFI_STA
              │    ↓
              │ Auto Reconnect
              │ WiFi cũ
              │
              └── Success
                    ↓
              Lưu NVS
                    ↓
              cập nhật cache
                    ↓
              Tắt Portal
                    ↓
                Connected
```
