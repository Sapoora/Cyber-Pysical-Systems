import os
import sqlite3
from perfetto.trace_processor import TraceProcessor

TRACE_FILE_PATH = "traces/trace_file.perfetto-trace"

if not os.path.exists(TRACE_FILE_PATH):
    print(f"[ERROR] Trace file not found at {TRACE_FILE_PATH}!")
    exit(1)

print(f"[PERFETTO] Loading trace file natively into memory: {TRACE_FILE_PATH} ...")

tp = TraceProcessor(trace=TRACE_FILE_PATH, addr="http://127.0.0.1:9001")

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
    rows = list(res4)
    if not rows:
        print("No matching thread-state rows found for Sensor/Main/Render threads.")
    for row in rows:
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
    rows = list(res6)
    if not rows:
        print("No matching filter slices found. Check that app tracing is enabled and that the trace contains AirMouse_ComplementaryFilter slices.")
    for row in rows:
        print(f"Filter function: {row.filter_function_name}")
        print(f"  🔹 Execution count: {row.execution_count}")
        print(f"  🔹 Average CPU processing time: {row.avg_execution_time_ms:.4f} ms")

# ==============================================================================
# Question 9: End-to-end latency from sensor change to laptop cursor movement
# ==============================================================================
s9_title = "Question 9: End-to-end latency from sensor change to laptop cursor movement"
s9_desc = (
    "This Perfetto trace can measure the Android-side path from AirMouse_OnSensorChanged "
    "to AirMouse_UdpSendMove. Measuring the full laptop cursor movement requires a matching "
    "timestamp or trace marker on the Windows server side."
)
s9_query = """
    WITH sensor_events AS (
        SELECT
            id,
            ts AS sensor_ts
        FROM slice
        WHERE name = 'AirMouse_OnSensorChanged'
    ),
    move_send_events AS (
        SELECT
            ts AS send_ts
        FROM slice
        WHERE name = 'AirMouse_UdpSendMove'
           OR name = 'AirMouse_CursorMoveSent'
           OR name = 'AirMouse_MovePacketSent'
    ),
    paired_events AS (
        SELECT
            s.sensor_ts,
            (
                SELECT MIN(m.send_ts)
                FROM move_send_events m
                WHERE m.send_ts >= s.sensor_ts
                  AND m.send_ts - s.sensor_ts <= 100000000
            ) AS send_ts
        FROM sensor_events s
    )
    SELECT
        COUNT(*) AS matched_events,
        AVG(send_ts - sensor_ts) / 1e6 AS avg_latency_ms,
        MIN(send_ts - sensor_ts) / 1e6 AS min_latency_ms,
        MAX(send_ts - sensor_ts) / 1e6 AS max_latency_ms
    FROM paired_events
    WHERE send_ts IS NOT NULL;
"""
res9 = run_query(s9_title, s9_query, s9_desc)
if res9:
    rows = list(res9)
    if not rows or rows[0].matched_events == 0:
        print("No matched latency events found.")
        print("Expected trace markers: AirMouse_OnSensorChanged and AirMouse_UdpSendMove.")
        print("For true laptop cursor latency, also log/trace the server-side pyautogui.moveRel timestamp.")
    for row in rows:
        if row.matched_events == 0:
            continue
        print(f"Matched movement events: {row.matched_events}")
        print(f"  🔹 Average Android sensor-to-send latency: {row.avg_latency_ms:.4f} ms")
        print(f"  🔹 Minimum latency: {row.min_latency_ms:.4f} ms")
        print(f"  🔹 Maximum latency: {row.max_latency_ms:.4f} ms")

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
    rows = list(res10)
    if not rows:
        print("No thread activity slices found.")
    for row in rows:
        print(f"🧵 Thread name: {row.thread_name:<20} | Total active time: {row.total_active_time_ms:.2f} ms")

print("\n" + "="*80)
print("✅ [PERFETTO] Analysis complete smoothly!")
print("="*80)
