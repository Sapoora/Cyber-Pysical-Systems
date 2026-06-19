import os
import sqlite3
from perfetto.trace_processor import TraceProcessor

TRACE_FILE_PATH = "traces/trace_file.perfetto-trace"

if not os.path.exists(TRACE_FILE_PATH):
    print(f"[ERROR] Trace file not found at {TRACE_FILE_PATH}!")
    exit(1)

print(f"[PERFETTO] Loading trace file natively into memory: {TRACE_FILE_PATH} ...")

tp = TraceProcessor(trace=TRACE_FILE_PATH)

def run_query(title, sql_query, description=""):
    print("\n" + "="*80)
    print(f"📌 {title}")
    if description:
        print(f"ℹ️ {description}")
    print("-"*80)
    
    try:
        qr_it = tp.query(sql_query)
        return qr_it
    except Exception as e:
        print(f"❌ Error executing query: {e}")
        return None

# ==============================================================================
# سوال ۳: نرخ نمونه‌برداری واقعی سنسورها در مقایسه با کد
# ==============================================================================
s3_title = "سوال ۳: محاسبه دوره نمونه‌برداری واقعی سنسورها در مقایسه با کد"
s3_desc = "استخراج فاصله زمانی (Timestamp) بین رویدادهای متوالی سنسور برای محاسبه فرکانس واقعی."
s3_query = """
    SELECT 
        name,
        COUNT(*) as total_samples,
        MIN(ts)/1e6 as first_sample_ms,
        MAX(ts)/1e6 as last_sample_ms,
        (MAX(ts) - MIN(ts)) / (COUNT(*) - 1) / 1e6 as avg_period_ms
    FROM slice
    WHERE name LIKE '%Sensor%' OR name LIKE '%onSensorChanged%'
    GROUP BY name;
"""
res3 = run_query(s3_title, s3_query, s3_desc)
if res3:
    for row in res3:
        print(f"اسم رویداد: {row.name}")
        print(f"  🔹 تعداد کل نمونه‌ها: {row.total_samples}")
        print(f"  🔹 دوره نمونه‌برداری متوسط سیستمی: {row.avg_period_ms:.4f} میلی‌ثانیه")
        print(f"  🔹 فرکانس معادل: {1000.0/row.avg_period_ms:.2f} هرتز")

# ==============================================================================
# سوال ۴: تعارض فرخوان‌های سیستمی (System Calls) و پردازش گرافیکی
# ==============================================================================
s4_title = "سوال ۴: بررسی تعارض و بلاک شدن ترد سنسور توسط رندر گرافیکی یا Syscalls"
s4_query = """
    SELECT 
        t.name as thread_name,
        s.state,
        COUNT(*) as occurrences,
        SUM(s.dur)/1e6 as total_blocked_time_ms
    FROM thread_state s
    JOIN thread t USING (utid)
    WHERE (t.name LIKE '%Sensor%' OR t.name LIKE '%Main%' OR t.name LIKE '%Render%')
      AND s.state IN ('R', 'D')
    GROUP BY thread_name, state
    ORDER BY total_blocked_time_ms DESC
    LIMIT 5;
"""
res4 = run_query(s4_title, s4_query)
if res4:
    for row in res4:
        status = "منتظر CPU (Runnable)" if row.state == 'R' else "قفل شده روی Syscall/IO"
        print(f"ریسمان: {row.thread_name} | وضعیت: {status} | مجموع تاخیر: {row.total_blocked_time_ms:.2f} ms")

# ==============================================================================
# سوال ۶: میانگین زمان اجرای تابع فیلتر روی CPU
# ==============================================================================
s6_title = "سوال ۶: میانگین زمان اجرای تابع فیلتر و تفکیک بار پردازشی سنسورها"
s6_query = """
    SELECT 
        name as filter_function_name,
        COUNT(*) as execution_count,
        AVG(dur)/1e6 as avg_execution_time_ms,
        MAX(dur)/1e6 as max_execution_time_ms
    FROM slice
    WHERE name LIKE '%Filter%' OR name LIKE '%Madgwick%' OR name LIKE '%Complementary%'
    GROUP BY name;
"""
res6 = run_query(s6_title, s6_query)
if res6:
    for row in res6:
        print(f"تابع فیلتر: {row.filter_function_name}")
        print(f"  🔹 تعداد دفعات اجرا: {row.execution_count}")
        print(f"  🔹 میانگین زمان پردازش روی سی‌پیو: {row.avg_execution_time_ms:.4f} میلی‌ثانیه")

# ==============================================================================
# سوال ۱۰: تفکیک و پیگیری فعالیت ریسمان‌ها (Threads)
# ==============================================================================
s10_title = "سوال ۱۰: تفکیک وظایف تردها و بار تحمیلی روی ترد اصلی"
s10_query = """
    SELECT 
        t.name as thread_name,
        COUNT(s.id) as total_slices,
        SUM(s.dur)/1e6 as total_active_time_ms
    FROM slice s
    JOIN thread_track tt ON s.track_id = tt.id
    JOIN thread t USING (utid)
    GROUP BY thread_name
    ORDER BY total_active_time_ms DESC
    LIMIT 5;
"""
res10 = run_query(s10_title, s10_query)
if res10:
    for row in res10:
        print(f"🧵 نام ترد: {row.thread_name:<20} | مجموع زمان فعالیت: {row.total_active_time_ms:.2f} ms")

print("\n" + "="*80)
print("✅ [PERFETTO] Analysis complete smoothly!")
print("="*80)