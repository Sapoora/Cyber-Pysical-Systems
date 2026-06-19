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
# Question 3: Actual sensor sampling rate compared to the code
# ==============================================================================
s3_title = "Question 3: Calculate the actual sensor sampling period compared to the code"
s3_desc = "Extract the time interval (timestamp) between consecutive sensor events to calculate the actual frequency."
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
        print(f"Event name: {row.name}")
        print(f"  🔹 Total samples: {row.total_samples}")
        print(f"  🔹 Average system sampling period: {row.avg_period_ms:.4f} ms")
        print(f"  🔹 Equivalent frequency: {1000.0/row.avg_period_ms:.2f} Hz")

# ==============================================================================
# Question 4: Conflicts between system calls and graphics processing
# ==============================================================================
s4_title = "Question 4: Check sensor thread contention and blocking by graphics rendering or syscalls"
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
        status = "Waiting for CPU (Runnable)" if row.state == 'R' else "Blocked on syscall/IO"
        print(f"Thread: {row.thread_name} | State: {status} | Total delay: {row.total_blocked_time_ms:.2f} ms")

# ==============================================================================
# Question 6: Average filter function execution time on CPU
# ==============================================================================
s6_title = "Question 6: Average filter function execution time and sensor processing load breakdown"
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
        print(f"Filter function: {row.filter_function_name}")
        print(f"  🔹 Execution count: {row.execution_count}")
        print(f"  🔹 Average CPU processing time: {row.avg_execution_time_ms:.4f} ms")

# ==============================================================================
# Question 10: Break down and track thread activity
# ==============================================================================
s10_title = "Question 10: Break down thread responsibilities and load imposed on the main thread"
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
        print(f"🧵 Thread name: {row.thread_name:<20} | Total active time: {row.total_active_time_ms:.2f} ms")

print("\n" + "="*80)
print("✅ [PERFETTO] Analysis complete smoothly!")
print("="*80)
