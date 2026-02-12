# masinaclient (OpenIPC / SigmaStar) (custom)

Клієнт для OpenIPC камер на SigmaStar, який:
- прокидає **CRSF (UART /dev/ttyS2) ↔ UDP** (RC/commands)
- прокидає **MAVLink (UART /dev/ttyS3) ↔ UDP**
- шле **телеметрію 1Hz** (CPU temp, RX/TX KB/s, RSSI/SNR)
- має **failsafe** з базовими PWM та **stage1/stage2** (перехід через таймаути)

> Типове застосування: LTE/USB модем → WireGuard → ground station.

---

## Example config (`/root/config.txt`)

```ini
-host=192.172.89.253
-CONTROL_PORT=2223
-CAM_INFO_PORT=2224
-MAVLINK_BAUD=115200

-FS_ENABLED=0
-LINK_LOST_MS=5000
-FAILSAFE_PWM_US=0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
-FS_STAGE1_AFTER_MS=5000
-FS_STAGE1_SET=3:1400
-FS_STAGE2_AFTER_MS=10000
-FS_STAGE2_SET=5:900

-LOCAL_TIMEOUT=0
-ELRS_SWITCH_PIN=0
-Failsafe parameters
-FS_ENABLED — 1 увімкнути stage-failsafe, 0 вимкнути

-LINK_LOST_MS — скільки мс без RC (26B) → вхід у failsafe

-FAILSAFE_PWM_US — 16 PWM значень, 0 = hold_last

-FS_STAGE1_AFTER_MS — мс після входу в failsafe → активувати stage1

-FS_STAGE1_SET — overrides ch:pwm,ch:pwm (pwm=0 → скинути override)

-FS_STAGE2_AFTER_MS — мс після входу в failsafe → активувати stage2

-FS_STAGE2_SET — overrides stage2 (перекриває stage1)

-Long loss (optional)
-LOCAL_TIMEOUT — мс дуже довгої втрати RC → виконати gpio clear

-0 = вимкнено (рекомендовано, якщо не треба)

-ELRS_SWITCH_PIN — GPIO пін для gpio clear (на камері)

-Failsafe logic (how it behaves)
-Поки RC пакети (26B) йдуть з ground — працює normal режим.

-Якщо RC немає довше ніж LINK_LOST_MS:

-активується базовий failsafe (FAILSAFE_PWM_US, де 0=hold_last)

-через FS_STAGE1_AFTER_MS застосовуються overrides stage1

-через FS_STAGE2_AFTER_MS застосовуються overrides stage2 (може переписати stage1)

-Коли зв’язок відновився — overrides скидаються і система повертається в normal.

-Tested
Тест було проведено на камері SSC30kq.
