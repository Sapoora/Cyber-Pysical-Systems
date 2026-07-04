<div dir="rtl">

# شکست کار تیمی

## اطلاعات تیم

| نفر | نقش | بخش مسئولیت |
|-----|-----|------------|
| نفر اول | مسئول بخش ۱ | پروتکل I2C ساده‌شده |
| نفر دوم | مسئول بخش ۲ | پروتکل USART Full Duplex |
| نفر سوم | مسئول بخش ۳ | I2C Multiplexer |

---

## نفر اول — پیاده‌سازی پروتکل I2C (بخش ۱)

### فایل‌های پیاده‌سازی‌شده:
- `include/CPS4042/Hardwares/Boards/Esp8266.h` ← تکمیل کلاس `I2C` (سمت master)
- `include/CPS4042/Hardwares/Sensors/VL530X.h` ← تکمیل کلاس `I2C` (سمت slave)
- `include/CPS4042/Sketchs/Sensor.h` ← اسکچ سنسور (تولید داده تصادفی + Checksum)
- `include/CPS4042/Sketchs/Microcontroller.h` ← اسکچ میکروکنترلر (دریافت + اعتبارسنجی)
- `src/main.cpp` ← اتصال فیزیکی دو بورد و اجرا

### کارهای انجام‌شده:
1. **تحلیل فریم‌ورک:** بررسی کلاس‌های `AbstractProtocol`، `AbstractI2C`، `Pin`، `Link`، `ByteStream`
2. **طراحی ماشین حالت:** تعریف سه حالت Idle/Addressing/Receiving برای master و دو حالت Listening/Ready برای slave
3. **پیاده‌سازی سمت slave (VL530X):**
   - تابع `run()`: تشخیص آدرس از روی SDA، ارسال ACK، flush کردن بایت‌های از پیش صف‌شده
   - تابع `write()`: بافر کردن بایت‌ها قبل از آدرس‌دهی، ارسال مستقیم بعد از ACK
   - متد کمکی `isAddressed()` برای استفاده در اسکچ
4. **پیاده‌سازی سمت master (Esp8266):**
   - تابع `init()`: ثبت آدرس هدف و ارسال آن روی SDA
   - تابع `run()`: انتظار برای drain شدن آدرس، تشخیص ACK، بافر کردن بایت‌های دریافتی
   - تابع `read()`: واسط API برای اسکچ
5. **اسکچ سنسور:** تولید عدد تصادفی `[0, 4000]` با `boost::mt19937`، شکستن به High/Low Byte، محاسبه Checksum
6. **اسکچ میکروکنترلر:** ماشین حالت دو‌مرحله‌ای با `ByteStream` برای بازسازی عدد ۱۶بیتی و اعتبارسنجی Checksum
7. **تست:** اجرا و تأیید عدم وجود `checksum mismatch` در صدها خواندن پیاپی
8. **سوالات:**
   - `docs/report.md` ← گزارش کامل پروژه
   - `docs/part1_answers.md` ← پاسخ سوالات بخش I2C

---

## نفر دوم — پیاده‌سازی پروتکل USART (بخش ۲)

### فایل‌های پیاده‌سازی‌شده:
- `include/CPS4042/Protocols/USART.h` ← پروتکل USART (Receiver + writeFrame)
- `include/CPS4042/Hardwares/Comm/Usb.h` ← بورد جدید هارد دیسک مجازی
- `include/CPS4042/Sketchs/HardDisk.h` ← اسکچ هارد دیسک (جدول آدرس → داده)
- `include/CPS4042/Sketchs/UsartMicrocontroller.h` ← اسکچ MCU برای USART
- `include/CPS4042/Hardwares/Boards/Esp8266.h` ← اضافه کردن کلاس `USART` به Esp8266

### کارهای انجام‌شده:
1. **تحلیل ساختار فریم USART:** درک Start Bit / 8 Data Bits / Stop Bit
2. **پیاده‌سازی `USART::Receiver`:** ماشین حالت Idle/Data/Stop برای دریافت بیت‌به‌بیت و بازسازی بایت
3. **پیاده‌سازی `writeFrame`:** ارسال Start Bit، ۸ بیت داده به صورت LSB-first، Stop Bit
4. **طراحی و پیاده‌سازی بورد `Usb`:**
   - GPIO: TX، RX، VDD، GND
   - تنظیم `BaudRates::NotSpecified` و `Frequency::F320khz`
   - نصب پروتکل USART روی processor
   - محدود کردن TX با `setCanRead(false)` برای جلوگیری از اشتباه جهت
5. **اسکچ HardDisk:** خواندن آدرس از USART و برگرداندن `address × 2` به عنوان داده
6. **اسکچ UsartMicrocontroller:** مکانیزم poll با timeout، ارسال آدرس‌های متوالی و اعتبارسنجی پاسخ
7. **تست Full Duplex:** تأیید ارسال و دریافت همزمان بدون تداخل
8. **سوالات:**
   - `docs/part2_answers.md` ← پاسخ سوالات بخش USART
---

## نفر سوم — پیاده‌سازی I2C Multiplexer (بخش ۳)

### فایل‌های پیاده‌سازی‌شده:
- `include/CPS4042/Hardwares/Comm/I2CMultiplexer.h` ← بورد MUX (اصلی‌ترین فایل بخش ۳)
- `include/CPS4042/Sketchs/I2CMuxSketch.h` ← اسکچ MUX
- `include/CPS4042/Sketchs/MuxMicrocontroller.h` ← اسکچ MCU برای MUX
- `include/CPS4042/Sketchs/RangeSensor.h` ← اسکچ سنسور عمومی با بازه قابل تنظیم

### کارهای انجام‌شده:
1. **طراحی معماری MUX:** USART uplink به MCU + دو کانال I2C مستقل به سنسورها
2. **کلاس template `C2I<Channel>`:**
   - یک کلاس برای هر دو کانال به جای copy-paste
   - انتخاب پین SDA مناسب با `if constexpr` در زمان کامپایل
   - بررسی `activeChannel()` در `run()` برای فعال/غیرفعال بودن کانال
3. **طراحی GPIO مالتی‌پلاکسر:** پایه‌های USART (tx/rx/vdd/gnd) + دو ست پایه I2C (sda0/scl0 و sda1/scl1)
4. **مکانیزم انتخاب کانال:** تفسیر فرمان one-hot از MCU، فعال کردن کانال، آدرس‌دهی سنسور
5. **تولید داده قابل‌تفکیک:** کانال ۰ اعداد ۰–۲۰، کانال ۱ اعداد ۵۰–۱۰۰
6. **اعتبارسنجی داده:** `isValidWindow()` برای تأیید Checksum و بازه مجاز قبل از فوروارد کردن
7. **اسکچ `RangeSensor`:** کلاس عمومی با `minValue`/`maxValue` قابل تنظیم که هم برای بخش ۱ و هم بخش ۳ استفاده می‌شود
8. **اسکچ `MuxMicrocontroller`:** polling چرخشی با شمارنده static، timeout و retry
9. **سوالات:**
   - `docs/part3_answers.md` ← پاسخ سوالات بخش MUX
---

### کارهای انجام‌شده:
1. **بررسی و درک کد** هر سه بخش برای نوشتن توضیحات دقیق
2. **نوشتن گزارش** شامل معماری، جریان داده، نمونه خروجی هر بخش
3. **پاسخ به ۹ سوال تئوری** (۳ سوال برای هر بخش) با استناد به کد واقعی پروژه
4. **مستندسازی تصمیمات طراحی**: چرا template به جای copy-paste، چرا pending queue در slave، چرا one-hot encoding برای کانال MUX
5. **بررسی یکپارچگی** سه بخش و اطمینان از سازگاری مستندات با کد نهایی

---

## نکات همکاری تیمی

- نفر اول و سوم تجربه مشترک روی پروتکل I2C داشتند: کلاس `RangeSensor` (بخش ۳) مستقیماً از تجربه `Sensor.h` (بخش ۱) بهره گرفت
- نفر دوم کلاس `Protocols::USART` را به گونه‌ای طراحی کرد که هم در بخش ۲ (هارد دیسک) و هم در بخش ۳ (MUX uplink) بدون تغییر قابل استفاده باشد
- نفر چهارم سوالات تئوری را با ارجاع مستقیم به کد هر بخش پاسخ داد تا توضیحات انتزاعی نباشند

</div>
