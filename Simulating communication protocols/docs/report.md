<div dir="rtl">

# گزارش تمرین کامپیوتری شماره ۱
## شبیه‌سازی پروتکل‌های ارتباطی

**درس:** سامانه‌های بی‌درنگ نهفته

---

## ۱. مقدمه و هدف پروژه

هدف این تمرین، افزودن لایه‌ی پروتکل به یک سیستم شبیه‌ساز سخت‌افزار جاسازی‌شده (Embedded Hardware Simulator) بود. شبیه‌ساز از قبل قابلیت اتصال فیزیکی ماژول‌ها را از طریق لینک‌ها و پایه‌های GPIO داشت. وظیفه‌ی ما این بود که قرارداد ارتباطی (پروتکل) را روی این بستر فیزیکی پیاده‌سازی کنیم تا دو ماژول بتوانند به شکل معنادار و قاعده‌مند با یکدیگر ارتباط برقرار کنند.

پروژه شامل سه بخش مستقل اما مرتبط به هم بود:
- **بخش اول:** پیاده‌سازی یک پروتکل آدرس‌محور ساده‌شده شبیه به I2C
- **بخش دوم:** پیاده‌سازی پروتکل Full Duplex USART برای ارتباط با یک هارد دیسک مجازی
- **بخش سوم:** طراحی یک I2C Multiplexer برای مدیریت چند سنسور هم‌آدرس

---

## ۲. معماری کلی فریم‌ورک

قبل از توضیح هر بخش، لازم است معماری فریم‌ورک ارائه‌شده را درک کنیم.

### ۲.۱ ساختار کلاس‌های پایه

```
Board<BaudRate, BitRate, Frequency, Voltage, Gpio>
├── Esp8266       ← میکروکنترلر اصلی
├── Sensors::Vl530x   ← سنسور فاصله
├── Sensors::Usb      ← هارد دیسک مجازی (بخش ۲)
└── Sensors::I2CMultiplexer  ← مالتی‌پلاکسر (بخش ۳)

AbstractProtocol<Gpio>
├── AbstractI2C<BoardType, Gpio>  ← پایه پروتکل I2C
└── AbstractUsart<BoardType, Gpio> ← پایه پروتکل USART

AbstractSketch<BoardType>
├── Sensor / RangeSensor   ← اسکچ سنسور
├── MicroController        ← اسکچ میکروکنترلر (I2C)
├── UsartMicroController   ← اسکچ میکروکنترلر (USART)
├── MuxMicroController     ← اسکچ میکروکنترلر (MUX)
├── HardDisk               ← اسکچ هارد دیسک
└── I2CMuxSketch           ← اسکچ مالتی‌پلاکسر
```

### ۲.۲ نحوه کار سیستم

هر بورد یک `Processor` دارد. پردازنده به ازای هر سیکل CPU:
1. تابع `run()` همه پروتکل‌های نصب‌شده را صدا می‌زند (لایه بیت‌های پروتکل)
2. اگر `setup()` هنوز اجرا نشده، آن را یک بار صدا می‌زند
3. تابع `loop()` اسکچ را صدا می‌زند (لایه اپلیکیشن)

لینک‌ها (`Link`) پایه‌ها (`Pin`) را به هم وصل می‌کنند. هر بیتی که روی یک پین نوشته شود، از طریق لینک در دسترس پین متصل در طرف دیگر قرار می‌گیرد.

---

## ۳. بخش اول: پروتکل I2C ساده‌شده

### ۳.۱ توضیح پروتکل

پروتکل I2C ساده‌شده در این پروژه بر اساس سه مرحله کار می‌کند:

**مرحله ۱ — آدرس‌دهی:**
میکروکنترلر (master) آدرس سنسور هدف (`0x29` برای VL530X) را روی خط `SDA` می‌نویسد. این آدرس به صورت ۸ بیت، یک بیت در هر سیکل، انتقال می‌یابد.

**مرحله ۲ — ACK:**
سنسور (slave) در هر سیکل چک می‌کند آیا یک بایت کامل رسیده یا نه. وقتی بایت رسید، با آدرس خودش مقایسه می‌کند. در صورت تطابق، یک بیت `1` به عنوان تأیید (ACK) روی `SDA` می‌نویسد.

**مرحله ۳ — انتقال داده:**
از این لحظه میکروکنترلر آماده خواندن است و سنسور می‌تواند داده بفرستد. هر بار که `loop()` سنسور اجرا می‌شود، یک عدد تصادفی بین ۰ تا ۴۰۰۰ تولید شده و به صورت سه بایت ارسال می‌شود: بایت بالا (High Byte)، بایت پایین (Low Byte)، و یک بایت Checksum.

### ۳.۲ نحوه ارسال و Checksum

فرض کنید فاصله اندازه‌گیری‌شده `2457` است:
- باینری: `0000 1001 1001 1001`
- High Byte: `0x09` (مقدار ۹)
- Low Byte: `0x99` (مقدار ۱۵۳)
- Checksum: `|153 - 9|` = `144` = `0x90`

میکروکنترلر بعد از دریافت سه بایت، Checksum را مستقل محاسبه می‌کند. اگر با بایت سوم مطابقت داشت، عدد معتبر است و چاپ می‌شود.

### ۳.۳ ساختار کد — سمت سنسور (VL530X)

```cpp
void run(Gpio& gpio) override
{
    if(m_started) return;
    if(!gpio.sda.hasByteToRead()) return;

    Byte incoming = gpio.sda.read();
    if(incoming != Vl530x::address) return;

    gpio.sda.write(Bit::One);  // ACK
    m_started = true;

    // flush any bytes the Sketch pre-queued
    while(!m_pending.empty()) { ... }
}
```

**نکته طراحی:** تابع `write()` سنسور قبل از آدرس‌دهی بایت‌ها را در صف (`m_pending`) نگه می‌دارد و بعد از ACK آن‌ها را flush می‌کند. این مانع تداخل داده با آدرس روی `SDA` می‌شود.

### ۳.۴ ساختار کد — سمت میکروکنترلر (Esp8266)

```cpp
void run(Gpio& gpio) override
{
    if(!m_addressing) return;

    if(!m_started)
    {
        if(gpio.sda.hasBitToWrite()) return;   // آدرس هنوز دارد می‌رود
        if(!gpio.sda.hasBitToRead()) return;   // ACK نرسیده
        if(gpio.sda.readBit() != Bit::One) return;  // ACK نامعتبر

        m_started = true;
        return;
    }

    if(gpio.sda.hasByteToRead())
        m_buffer.push(gpio.sda.read());
}
```

### ۳.۵ اسکچ سنسور

```cpp
std::int32_t loop(Sensors::Vl530x::Gpio& gpio) override
{
    if(!node()->i2c.isAddressed()) return 0;

    std::uint16_t distance = m_distribution(m_rng);
    UByte high = static_cast<UByte>(getByte<1>(distance));
    UByte low  = static_cast<UByte>(getByte<0>(distance));
    Byte  checksum = static_cast<Byte>(high > low ? high - low : low - high);

    node()->i2c.write(static_cast<Byte>(high));
    node()->i2c.write(static_cast<Byte>(low));
    node()->i2c.write(checksum);

    return 0;
}
```

### ۳.۶ اسکچ میکروکنترلر

```cpp
std::int32_t setup(Boards::Esp8266::Gpio& gpio) override
{
    node()->i2c.init(Sensors::Vl530x::address);  // آدرس 0x29
    return 0;
}

std::int32_t loop(Boards::Esp8266::Gpio& gpio) override
{
    if(!node()->i2c.isDataAvailable()) return 0;
    Byte incoming = node()->i2c.read();

    if(!m_awaitingChecksum) {
        m_distanceStream << incoming;
        if(m_distanceStream.isReady()) {
            m_distance = m_distanceStream.take();
            m_awaitingChecksum = true;
        }
        return 0;
    }

    // بررسی checksum و چاپ نتیجه
    UByte high = static_cast<UByte>(getByte<1>(m_distance));
    UByte low  = static_cast<UByte>(getByte<0>(m_distance));
    UByte expected = high > low ? high - low : low - high;

    if(static_cast<UByte>(incoming) == expected)
        std::cout << "new distance: " << m_distance << std::endl;
    else
        std::cerr << "checksum mismatch" << std::endl;

    m_awaitingChecksum = false;
    return 0;
}
```

### ۳.۷ نتیجه اجرا

```
esp8266 setup completed.
vl530x setup completed.
new distance: 2457
new distance: 2727
new distance: 63
...
```
تمام مقادیر در بازه ۰ تا ۴۰۰۰ بودند و هیچ پیام `checksum mismatch` مشاهده نشد، که نشان‌دهنده صحت کامل پروتکل است.

---

## ۴. بخش دوم: پروتکل USART Full Duplex

### ۴.۱ توضیح پروتکل

USART یک پروتکل سریال ناهمگام (Asynchronous) است. برخلاف I2C که خط کلاک مشترک دارد، در USART هر دو طرف بر اساس یک Baud Rate مشترک از قبل توافق‌شده، داده می‌فرستند و دریافت می‌کنند.

**ساختار یک فریم USART:**
```
[0][D0][D1][D2][D3][D4][D5][D6][D7][1]
 ↑                                   ↑
Start Bit                          Stop Bit
```
- **Start Bit:** مقدار `0` — اعلام شروع فریم
- **8 بیت داده:** از کم‌ارزش‌ترین بیت (LSB) شروع می‌شود
- **Stop Bit:** مقدار `1` — اعلام پایان فریم

**Full Duplex:** هر دو طرف می‌توانند هم‌زمان بفرستند و دریافت کنند، چون خطوط TX و RX مستقل هستند.

### ۴.۲ معماری سیستم

```
Esp8266 (MCU)          Usb (HardDisk)
    TX ────────────────────── RX
    RX ────────────────────── TX
   VDD ────────────────────── VDD
   GND ────────────────────── GND
```

میکروکنترلر یک آدرس (۰ تا ۲۵۵) روی TX می‌فرستد. هارد دیسک آدرس را می‌خواند، داده متناظر را حساب می‌کند (`address × 2`) و روی TX خودش (که به RX میکروکنترلر وصل است) برمی‌گرداند.

### ۴.۳ پیاده‌سازی USART.h

کلاس `Protocols::USART::Receiver` یک state machine سه‌حالته است:

```
Idle ─(بیت 0 رسید)→ Data ─(8 بیت دریافت شد)→ Stop ─(بیت 1)→ بافر
                                                             └─(بیت 0)→ Idle (فریم خراب)
```

تابع `writeFrame` ارسال را انجام می‌دهد:
```cpp
template <typename TxPin>
void writeFrame(TxPin& tx, Byte byte)
{
    tx.write(Bit::Zero);  // Start Bit
    auto value = static_cast<UByte>(byte);
    for(std::uint8_t i = 0; i < bitWidth<Byte>(); ++i)
        tx.write(takeNthBit(value, i));  // 8 بیت داده، LSB اول
    tx.write(Bit::One);  // Stop Bit
}
```

### ۴.۴ بورد Usb

```cpp
class Usb : public Board<BaudRates::NotSpecified,
                         BitRates::same(BaudRates::NotSpecified),
                         Frequency::F320khz, UsbVoltage, UsbGpio>
{
    class USART : public Protocols::AbstractUsart<Usb, Gpio> {
        void write(Byte byte) override {
            Protocols::USART::writeFrame(m_board->gpio().tx, byte);
        }
        void run(Gpio& gpio) override {
            while(gpio.rx.hasBitToRead())
                m_receiver.push(gpio.rx.readBit());
        }
    } mutable usart {this};
};
```

**نکته مهم:** پین `TX` بورد Usb با `setCanRead(false)` محدود شده تا فقط برای ارسال استفاده شود، نه خواندن. این از نوشتن هارد دیسک روی پین اشتباه جلوگیری می‌کند.

### ۴.۵ اسکچ HardDisk

```cpp
std::int32_t loop(Sensors::Usb::Gpio& gpio) override
{
    if(!node()->usart.isDataAvailable()) return 0;
    Byte address = node()->usart.read();
    Byte data    = dataAt(address);
    node()->usart.write(data);
    return 0;
}

Byte dataAt(Byte address) const {
    return static_cast<Byte>(static_cast<UByte>(address) * 2U);
}
```

### ۴.۶ اسکچ میکروکنترلر (USART)

میکروکنترلر هر ۲۰۰ سیکل یک آدرس جدید می‌فرستد. اگر در بازه timeout پاسخ نرسید، دوباره ارسال می‌کند:

```
آدرس 0  → داده 0
آدرس 1  → داده 2
آدرس 2  → داده 4
...
آدرس 127 → داده 254
```

خروجی نمونه:
```
USART address 0 -> data 0
USART address 1 -> data 2
USART address 2 -> data 4
```

---

## ۵. بخش سوم: I2C Multiplexer

### ۵.۱ مشکل و راه‌حل

در I2C واقعی، نمی‌توان دو سنسور با آدرس یکسان را روی یک باس مشترک قرار داد چون باعث تداخل می‌شود. راه‌حل، استفاده از Multiplexer است: هر سنسور روی یک کانال I2C مجزا قرار می‌گیرد و MUX تنها یک کانال را در هر لحظه فعال می‌کند.

### ۵.۲ معماری کلی

```
ESP8266 ←─── USART ───→ I2CMultiplexer ←─── I2C Channel 0 ───→ VL530X (سنسور A)
                                        └─── I2C Channel 1 ───→ VL530X (سنسور B)
```

**جریان ارتباط:**
1. MCU از طریق USART یک فرمان به MUX می‌فرستد (one-hot: `0x01` برای کانال ۰، `0x02` برای کانال ۱)
2. MUX کانال انتخاب‌شده را فعال می‌کند
3. سنسور آن کانال داده می‌فرستد
4. MUX داده را از طریق USART به MCU فوروارد می‌کند

### ۵.۳ کلاس C2I عمومی (Generic)

یکی از مهم‌ترین جنبه‌های طراحی، کلاس `C2I<std::size_t Channel>` است که به صورت template پیاده‌سازی شده:

```cpp
template <std::size_t Channel>
class C2I : public Protocols::AbstractI2C<I2CMultiplexer, Gpio>
{
    void run(Gpio& gpio) override
    {
        // فقط اگر این کانال فعال است، کار کن
        if(m_board->activeChannel() != static_cast<std::int8_t>(Channel))
            return;
        // ... منطق I2C master
    }

    static auto& sda(Gpio& gpio) {
        if constexpr(Channel == 0) return gpio.sda0;
        else                        return gpio.sda1;
    }
};

C2I<0> mutable c2i0 {this};
C2I<1> mutable c2i1 {this};
```

**مزیت طراحی:** به جای نوشتن دو کلاس جداگانه برای دو کانال، یک کلاس template نوشتیم که با تغییر پارامتر، رفتار درست را دارد. این از copy-paste جلوگیری می‌کند.

### ۵.۴ منطق انتخاب کانال

```cpp
bool requestChannel(std::uint8_t channel) const
{
    if(!selectChannel(channel)) return false;
    enqueueChannelFrame(channel);  // داده سنسور را آماده می‌کند
    m_activeChannel = -1;
    return true;
}
```

**تولید داده‌های قابل‌تفکیک:**
- **کانال ۰:** اعداد ۰ تا ۲۰ (اعداد کوچک)
- **کانال ۱:** اعداد ۵۰ تا ۱۰۰ (اعداد متوسط)

این بازه‌های مختلف در خروجی به وضوح نشان می‌دهند که از کدام سنسور خوانده‌ایم.

### ۵.۵ اسکچ MUX

```cpp
std::int32_t loop(Sensors::I2CMultiplexer::Gpio& gpio) override
{
    if(node()->hasResponseToForward()) {
        node()->forwardQueuedResponse();  // پاسخ آماده را به MCU بفرست
        return 0;
    }

    if(!node()->usart.isDataAvailable()) return 0;

    auto command = static_cast<UByte>(node()->usart.read());
    // one-hot decode: 0x01 → کانال ۰، 0x02 → کانال ۱
    auto channel = static_cast<std::uint8_t>(
        std::countr_zero(command) % I2CMultiplexer::channelCount);

    node()->requestChannel(channel);
    return 0;
}
```

### ۵.۶ اسکچ MuxMicrocontroller

```cpp
std::int32_t loop(Boards::Esp8266::Gpio& gpio) override
{
    readPendingUsartData();  // اگر پاسخی رسیده، پردازش کن

    if(m_waitingForResponse) {
        if(m_responseCycles++ < m_responseTimeout) return 0;
        // timeout — دوباره بفرست
        sendRequest(m_pendingChannel);
        return 0;
    }

    if(m_waitCycles++ < m_period) return 0;
    m_waitCycles = 0;

    static std::uint8_t nextChannel {0};
    sendRequest(nextChannel);
    nextChannel = (nextChannel + 1U) % I2CMultiplexer::channelCount;
    return 0;
}
```

خروجی نمونه:
```
MUX channel 0 -> value 0
MUX channel 1 -> value 50
MUX channel 0 -> value 1
MUX channel 1 -> value 57
MUX channel 0 -> value 2
...
```

به وضوح می‌توان دید که MCU به تناوب از هر دو کانال می‌خواند و مقادیر کانال ۰ (۰–۲۰) از مقادیر کانال ۱ (۵۰–۱۰۰) قابل تشخیص هستند.

---

## ۶. ابزارهای مشترک (Utilities)

### ByteVector و getByte
برای شکستن یک عدد چندبایتی به بایت‌های جداگانه:
```cpp
std::uint16_t distance = 2457;
UByte high = getByte<1>(distance);  // بایت بالا
UByte low  = getByte<0>(distance);  // بایت پایین
```

### ByteStream
برای ساختن عدد چندبایتی از بایت‌های یک‌یک رسیده:
```cpp
ByteStream<std::uint16_t> stream;
stream << high_byte;
stream << low_byte;
if(stream.isReady())
    uint16_t value = stream.take();
```

---

## ۷. جمع‌بندی

در این تمرین سه پروتکل ارتباطی با موفقیت پیاده‌سازی شد:

| بخش | پروتکل | بردها | نتیجه |
|-----|--------|-------|-------|
| ۱ | I2C ساده‌شده | ESP8266 ↔ VL530X | انتقال مداوم داده فاصله با Checksum |
| ۲ | USART Full Duplex | ESP8266 ↔ USB | خواندن آدرس‌محور از هارد دیسک مجازی |
| ۳ | I2C MUX | ESP8266 ↔ MUX ↔ 2×VL530X | polling چرخشی از دو کانال مستقل |

مهم‌ترین نکات یاد گرفته‌شده:
- پروتکل‌های ارتباطی در واقع **ماشین حالت** (State Machine) هستند که در هر سیکل CPU اجرا می‌شوند
- `ByteStream` و `getByte` انتزاعی تمیز برای کار با داده‌های چندبایتی ایجاد می‌کنند
- طراحی generic با template از تکرار کد جلوگیری می‌کند
- Multiplexer راه‌حل عملی برای مشکل آدرس تکراری در I2C است

</div>
