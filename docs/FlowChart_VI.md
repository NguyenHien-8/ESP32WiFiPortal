# Lưu đồ hoạt động ESP32WiFiPortal 2.1.2

Tài liệu này mô tả state machine của `ESP32WiFiPortal` 2.1.2 cho kết nối blocking, Config Portal blocking/non-blocking, Wi-Fi event, retry, Auto Reconnect và Wi-Fi scan bất đồng bộ.

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
    B -- Có --> D{IP và gateway là unicast host cùng subnet /24 đến /28?}
    D -- Không --> E[Trả false và ghi lastError]
    D -- Có --> F[Lưu cấu hình Portal IP trong object]
    C --> G[connectSaved hoặc autoConnect]
    F --> G
    G --> X{Có cred_erased?}
    X -- Có --> Y[Xóa mọi record credential và trả về không có credential]
    X -- Không --> H{cred_blob hợp lệ?}
    H -- Có --> J[Áp dụng DHCP hoặc Static STA IP và bắt đầu connection]
    H -- Thiếu hoặc hỏng --> HB{cred_backup hợp lệ?}
    HB -- Có --> HC[Dùng backup cũ và thử phục hồi primary]
    HC --> J
    HB -- Không --> R{Legacy ssid/pass hợp lệ?}
    R -- Có --> S[Ghi một blob, đọc lại và kiểm CRC]
    S --> T{Xác minh thành công?}
    T -- Có --> U[Xóa hai legacy key]
    U --> J
    T -- Không --> I{Đang dùng autoConnect?}
    R -- Không --> I
    J --> K{Kết nối trước timeout?}
    K -- Có --> L[State = Connected]
    K -- Không --> M[WiFi.disconnect, trả false và đặt lịch credential đã lưu]
    Y --> I
    I -- Có --> N[Mở Config Portal blocking]
    I -- Không --> O[Trả false cho ứng dụng]
    M --> I
```

`connectSaved()` đọc namespace `ewp_wifi`; lỗi kết nối không xóa credential đã
lưu. Record hỏng bị từ chối và không được Auto Reconnect sử dụng; lỗi mở NVS tạm
thời được thử lại sau cooldown. Nếu một Portal đang hoạt động, thư viện dừng và dọn Portal trước khi
chuyển sang `WIFI_STA`. Khi Auto Reconnect bật, một lần blocking thất bại vẫn
đặt lịch thử lại non-blocking; `autoConnect()` sẽ hủy lịch này khi chuyển ngay
sang Portal nên không có hai owner kết nối.

## Khởi tạo Config Portal

```mermaid
flowchart TD
    A[startConfigPortal hoặc startConfigPortalAsync] --> V{Revalidate Portal IP gateway subnet}
    V -- Không hợp lệ --> Y[Trả false, không khởi động tài nguyên]
    V -- Hợp lệ --> B[stopConfigPortal để dọn phiên cũ]
    B --> C[Chuyển sang WIFI_AP_STA]
    C --> D[Áp dụng local IP, gateway, subnet bằng softAPConfig]
    D --> E{Cấu hình IP thành công?}
    E -- Không --> X[Cleanup và phục hồi credential cũ nếu STA đang offline]
    E -- Có --> F[Khởi động SoftAP]
    F --> G{SoftAP thành công?}
    G -- Không --> X
    G -- Có --> H[Đọc lại SoftAP IP và subnet runtime]
    H --> I{Khớp cấu hình yêu cầu?}
    I -- Không --> X
    I -- Có --> J[Khởi động WebServer cổng 80]
    J --> K[Khởi động wildcard DNS cổng 53]
    K --> L{DNS thành công?}
    L -- Không --> X
    L -- Có --> M[State = Portal và gọi onPortalStarted]
```

Portal IP được giữ cố định trong suốt phiên đang chạy. `setPortalIP()` trả
`false` nếu được gọi khi Portal còn active, nhờ đó SoftAP, DNS và HTTP redirect
luôn dùng cùng một địa chỉ.

Validator chấp nhận unicast Class A/B/C hợp lệ nhưng dùng CIDR `/24` đến `/28`
cho DHCP SoftAP; không suy ra classful `/8` hoặc `/16`. RFC 1918 vẫn được khuyến
nghị để tránh xung đột route. `200.5.29.8/24` được hỗ trợ cho SoftAP cục bộ nhưng
không thay thế default an toàn `192.168.4.1/24`.

Validator cũng tính trước lease pool mặc định của Arduino-ESP32 3.3.11 và từ
chối IP/gateway nằm trong pool, tránh trường hợp setter thành công nhưng
`softAPConfig()` chắc chắn thất bại khi khởi động.

Các phép kiểm tra IPv4 thuần được đóng gói thành helper `private static inline`
ngay trong class `ESP32WiFiPortal`. Vì vậy thư viện chỉ có một implementation,
không mở rộng public API, không cấp phát heap khi validate và không phát sinh
multiple-definition khi nhiều translation unit cùng include header chính.

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
    A[POST /save] --> B{SSID đúng 1-32 byte và password rỗng, 8-63 byte hoặc 64 ký tự hex?}
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
    I -- Có --> M[Xác minh backup bản cũ, rồi ghi và xác minh cred_blob]
    M --> N{Đọc lại đủ byte, metadata và CRC hợp lệ?}
    N -- Không --> O[Ngắt candidate STA, xóa dữ liệu tạm, giữ Portal]
    N -- Có --> P[Gọi callback, dừng Portal, giữ STA connected]
    P --> Q[State = Connected]
```

Credential cũ trong NVS không bị thay đổi khi candidate không kết nối được hoặc
khi Portal hết thời gian. Việc ghi chỉ diễn ra sau khi `WL_CONNECTED`. Handshake
timeout không tự động chứng minh password sai vì cũng có thể do sóng yếu hoặc AP
đang restart.

SSID từ cả danh sách scan và **Advanced Wi-Fi Setting** được giữ nguyên byte,
không gọi `trim()`. Advanced view dùng cùng POST `/save`, cùng candidate RAM và
cùng state machine; password không nằm trong URL hoặc browser storage.

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
`setConnectTimeout(0)` và `connectSaved(0)` được chuẩn hóa thành 15000 ms, nên
blocking connect, Portal candidate và Auto Reconnect không thể giữ một attempt
vô hạn.

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
    A[eraseCredentials] --> B[Ghi và xác minh cred_erased]
    B --> C[Xóa primary backup và các legacy key]
    C --> D[Xóa cred_erased sau cùng]
    D --> E{Có yêu cầu disconnect?}
    E -- Không --> F[Giữ nguyên trạng thái Wi-Fi]
    E -- Có --> G{Instance này sở hữu global Wi-Fi?}
    G -- Không --> H[Trả false, không đổi global Wi-Fi]
    G -- Có --> I[Dừng Portal và yêu cầu ngắt STA]
```

`eraseCredentials()` là thao tác xóa duy nhất do API công khai yêu cầu. Connect
Nếu reset xảy ra sau khi marker được commit, boot tiếp tục xóa thay vì phục hồi
backup còn sót. Connect timeout, Portal timeout và `stopConfigPortal()` không
xóa credential đã lưu.

---

## Async Wi-Fi scan

```mermaid
flowchart TD
    A[Browser GET /scan] --> B[handleScan]
    B --> C{Pending/active connection?}
    C -- Có --> D[HTTP 409]
    C -- Không --> E[processScan]
    E --> F{ScanState}

    F -- Idle --> G[WiFi.scanNetworks true,true]
    G --> H{RUNNING?}
    H -- Có --> I[State = Scanning]
    H -- Không, result >= 0 --> J[State = Ready]
    H -- Lỗi --> K[State = Failed]

    F -- Scanning --> L[WiFi.scanComplete]
    L --> M{RUNNING và < 15 s?}
    M -- Có --> I
    M -- Không --> N{result >= 0?}
    N -- Có --> J
    N -- Không --> K

    I --> O[HTTP 202]
    O --> P[Browser đợi 400 ms]
    P --> A

    J --> Q[Build JSON]
    Q --> R[resetScan false / scanDelete]
    R --> S[HTTP 200]

    K --> T[HTTP 503]
    T --> U[resetScan false]
```

Trong khoảng thời gian browser đang đợi 400 ms, `process()` vẫn có thể gọi `processScan()` và chuyển `Scanning -> Ready/Failed`. Vì vậy không nên mô tả rằng chỉ `/scan` mới làm scan tiến triển.

## Function `process()` cooperative non-blocking — luồng rút gọn chính xác

`process()` **không chờ** Wi-Fi connection, settle delay, retry delay hoặc network scan. Tuy nhiên một lần gọi `process()` có thể phục vụ nhiều tác vụ đã sẵn sàng (event, scan, DNS, HTTP) trước khi trả về; không phải mỗi lần gọi chỉ thực hiện đúng một thao tác.

```mermaid
flowchart TD
    A[loop] --> B[process]
    B --> C[processWiFiEvents]
    C --> D{Portal active?}

    D -- Có --> E[processScan]
    E --> F[DNS processNextRequest]
    F --> G[WebServer handleClient]
    G --> H{Portal timeout?}
    H -- Có --> I[stopConfigPortal, cập nhật state, return]
    H -- Không --> J{Pending connection đã tới thời điểm?}
    J -- Có --> K[beginPendingConnection, return]
    J -- Không --> L{Portal connection attempt active?}
    L -- Có --> M[advanceSTAConnection / kiểm tra connected, terminal, timeout]
    M --> N{Cần chờ thêm?}
    N -- Có --> O[return]
    N -- Không --> P[Xử lý success/failure/retry rồi return hoặc tiếp tục]
    L -- Không --> Q[processAutoReconnect sẽ no-op vì Portal active]

    D -- Không --> R[processAutoReconnect]
    Q --> S[return]
    R --> S
    P --> S
    O --> S
    S --> T[Application tiếp tục chạy]
    T --> A
```

Ví dụ phase kết nối Portal candidate:

```text
process #1
  -> pending delay 350 ms đã hết
  -> beginSTAConnection()
  -> nếu STA chưa ở trạng thái clean: WiFi.disconnect()
  -> Phase = Settling, settle delay = 20 ms
  -> return

process #2 ... #N
  -> advanceSTAConnection()
  -> nếu settle delay chưa hết: return nhanh

process tiếp theo khi settle đã hết
  -> applySTAConfig()
  -> WiFi.begin()
  -> Phase = Connecting
  -> return sau các kiểm tra tức thời

các process sau
  -> đọc event / WiFi.status()
  -> kiểm tra terminal failure hoặc connect timeout bằng millis()
  -> không busy-wait
```

Nếu STA đã ở trạng thái disconnected/clean, `_connectionSettleDelayMs` có thể bằng `0`; do đó **không phải mọi attempt đều bắt buộc chờ 20 ms**.

Lưu ý: `process()` là cooperative non-blocking, nhưng các API `connectSaved()` và `startConfigPortal()` vẫn là API blocking theo thiết kế. `startConfigPortal()` đạt hành vi blocking bằng cách tự lặp `process()` bên trong.

## Luồng hoạt động tổng thể

```mermaid
flowchart TD
    A[ESP32 khởi động / tạo object] --> B[Ứng dụng chọn API]

    B -->|connectSaved| C[ensureCredentialCache]
    B -->|autoConnect| D[connectSaved]
    B -->|startConfigPortal / Async / On-Demand trigger| P[openPortal]

    C --> E{Credential hợp lệ?}
    E -- Không --> F[Trả false; có thể schedule recovery nếu lỗi NVS tạm thời]
    E -- Có --> G[Blocking STA connect với credential đã lưu]
    G --> H{Success?}
    H -- Có --> I[State = Connected]
    H -- Không --> J{Đã schedule saved recovery?}
    J -- Có --> AH
    J -- Không --> JO[Trả false; offline/Failed]

    D --> K{connectSaved success?}
    K -- Có --> I
    K -- Không --> P

    P --> Q[WIFI_AP_STA + SoftAP + DNS + WebServer]
    Q --> R[State = Portal]
    R --> RA{Portal timeout hoặc stop?}
    RA -- Có --> AE
    RA -- Không --> S[Async scan hoặc nhập SSID thủ công]
    S --> T[POST /save]
    T --> U[Giữ candidate trong RAM, chờ 350 ms]
    U --> V[Non-blocking STA candidate: Settling -> Config -> WiFi.begin]
    V --> W{Kết nối?}

    W -- Thành công --> X[Xác minh backup cũ, rồi ghi/read-back cred_blob + CRC]
    X --> Y{Save thành công?}
    Y -- Có --> Z[Cập nhật cache, callbacks, stop Portal]
    Z --> I
    Y -- Không --> AA[Disconnect candidate, giữ Portal]

    W -- Terminal auth fail --> AA
    W -- Timeout --> AB{Còn retry Portal?}
    AB -- Có --> AC[Backoff bằng millis rồi thử lại]
    AC --> V
    AB -- Không --> AA

    AA --> R
    AE[Cleanup HTTP/DNS/scan/SoftAP]
    AE --> AF{STA còn connected?}
    AF -- Có --> I
    AF -- Không --> AG{Auto Reconnect bật và credential cũ hợp lệ/có thể đọc lại?}
    AG -- Có --> AH[Schedule saved recovery, State = Connecting]
    AG -- Không --> AI[Timeout -> Failed; stop chủ động -> Idle]

    I --> AJ[Wi-Fi event callback chỉ set atomic bits]
    AJ --> AK[processWiFiEvents]
    AK --> AL{Mất STA khi không có owner?}
    AL -- Không --> I
    AL -- Có --> AM{Auto Reconnect bật và Portal inactive?}
    AM -- Không --> AN[Không reconnect tự động]
    AM -- Có --> AH

    AH --> AO[processAutoReconnect]
    AO --> AP[WIFI_STA, beginSTAConnection owner Reconnect]
    AP --> AQ{Success trước timeout?}
    AQ -- Có --> I
    AQ -- Không --> AR{Còn retry trong burst?}
    AR -- Có --> AS[Exponential backoff]
    AS --> AO
    AR -- Không --> AT[Cooldown = max retry interval]
    AT --> AO
```

### Các điểm cần hiểu đúng

- Constructor **không tự động** đọc NVS, kết nối STA hoặc mở Portal; mọi luồng bắt đầu từ API mà ứng dụng gọi.
- Chỉ `autoConnect()` tự chuyển từ `connectSaved()` thất bại sang **blocking Config Portal**. `connectSaved()` đơn lẻ không tự mở Portal; ví dụ On-Demand có thể giữ thiết bị offline cho tới khi người dùng kích hoạt Portal.
- Config Portal không bắt buộc phải chọn SSID từ kết quả scan: Advanced Wi-Fi Setting có thể gửi hidden/unlisted SSID qua cùng `POST /save`.
- Candidate credential chỉ được ghi vào NVS sau khi STA đã kết nối. Nếu lưu/read-back/CRC thất bại, candidate bị ngắt và Portal vẫn được giữ.
- Khi Portal timeout/stop trong lúc STA offline, thư viện chỉ phục hồi Wi-Fi cũ khi Auto Reconnect bật và credential cũ còn hợp lệ hoặc có thể đọc lại. Nếu không, timeout kết thúc ở `Failed`, còn stop chủ động kết thúc ở `Idle`.
- Auto Reconnect chạy trong `process()` và dùng retry hữu hạn + exponential backoff + cooldown; Arduino core Auto Reconnect được tắt để tránh hai luồng reconnect cạnh tranh.
