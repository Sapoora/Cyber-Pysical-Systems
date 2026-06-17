import os
from perfetto.trace_processor import TraceProcessor

# 1. Initial setup and connection to trace file
# If using a local tool, enable the server address (addr)
TRACE_FILE_PATH = "traces/trace_file.perfetto-trace"

if not os.path.exists(TRACE_FILE_PATH):
    print(f"[ERROR] Trace file not found at {TRACE_FILE_PATH}!")
    print("Please make sure your teammates pulled the trace from the phone and put it here.")
    exit(1)

print(f"[PERFETTO] Loading trace file: {TRACE_FILE_PATH} ...")
# Connection to Trace Processor (compatible with offline mode port 9001 of project)
tp = TraceProcessor(trace=TRACE_FILE_PATH, addr="http://127.0.0.1:9001")


def run_query(title, sql_query, description=""):
    """Helper function to execute clean queries and print coherent output"""
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
# Question 3: Time between reading two consecutive sensor data (actual sampling rate)
# ==============================================================================
s3_title = "Question 3: Calculate the actual sampling period of sensors compared to code"
s3_desc = "Extract time interval (Timestamp) between consecutive sensor events to calculate actual frequency."
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
        print(f"  🔹 Total number of samples: {row.total_samples}")
        print(f"  🔹 Average system sampling period: {row.avg_period_ms:.4f} milliseconds")
        print(f"  🔹 Equivalent frequency: {1000.0/row.avg_period_ms:.2f} Hz")


# ==============================================================================
# Question 4: Conflict of system calls (System Calls) and graphics processing
# ==============================================================================
s4_title = "Question 4: Check for conflicts and blocking of sensor thread by graphics rendering or Syscalls"
s4_desc = "Observe thread status when in sleep or wait state (Runnable / Uninterruptible Sleep) due to conflicts."
s4_query = """
    SELECT 
        t.name as thread_name,
        s.state,
        COUNT(*) as occurrences,
        SUM(s.dur)/1e6 as total_blocked_time_ms,
        AVG(s.dur)/1e6 as avg_blocked_time_ms
    FROM thread_state s
    JOIN thread t USING (utid)
    WHERE (t.name LIKE '%Sensor%' OR t.name LIKE '%Main%' OR t.name LIKE '%Render%')
      AND s.state IN ('R', 'D') -- R: Runnable (waiting for processor), D: Uninterruptible Sleep (waiting for I/O or Syscall)
    GROUP BY thread_name, state
    ORDER BY total_blocked_time_ms DESC
    LIMIT 10;
"""
res4 = run_query(s4_title, s4_query, s4_desc)
if res4:
    for row in res4:
        status = "Waiting for CPU (Runnable)" if row.state == 'R' else "Locked on Syscall/IO"
        print(f"Thread: {row.thread_name} | Status: {status}")
        print(f"  🔹 Number of conflict occurrences: {row.occurrences} times")
        print(f"  🔹 Total blocking delay: {row.total_blocked_time_ms:.2f} milliseconds")


# ==============================================================================
# Question 6: Average execution time of filter function on CPU and most resource-intensive sensor
# ==============================================================================
s6_title = "Question 6: Average execution time of filter function and distribution of sensor processing load"
s6_desc = "Calculate exact net processing time (Duration) of fusion filter functions (such as Madgwick or Complementary code)."
s6_query = """
    SELECT 
        name as filter_function_name,
        COUNT(*) as execution_count,
        SUM(dur)/1e6 as total_cpu_time_ms,
        AVG(dur)/1e6 as avg_execution_time_ms,
        MAX(dur)/1e6 as max_execution_time_ms
    FROM slice
    WHERE name LIKE '%Filter%' OR name LIKE '%Madgwick%' OR name LIKE '%Complementary%'
       OR name LIKE '%Calc%'
    GROUP BY name
    ORDER BY total_cpu_time_ms DESC;
"""
res6 = run_query(s6_title, s6_query, s6_desc)
if res6:
    for row in res6:
        print(f"Filter function: {row.filter_function_name}")
        print(f"  🔹 Number of executions: {row.execution_count}")
        print(f"  🔹 Average processing time on CPU: {row.avg_execution_time_ms:.4f} milliseconds")
        print(f"  🔹 Maximum single execution time (Worst-case): {row.max_execution_time_ms:.4f} milliseconds")


# ==============================================================================
# Question 9: End-to-End Latency (End-to-End Latency) from sensor to network transmission
# ==============================================================================
s9_title = "Question 9: Average time consumed from data preparation to network port exit"
s9_desc = "Calculate time interval between recording raw sensor event and completion of network UDP send function (requires time accumulation in Android code)."
s9_query = """
    SELECT 
        t.name as thread_name,
        s1.name as start_event,
        s2.name as end_event,
        AVG(s2.ts - s1.ts)/1e6 as avg_latency_ms
    FROM slice s1
    JOIN slice s2 ON s1.track_id = s2.track_id AND s2.ts > s1.ts
    JOIN thread_track tt ON s1.track_id = tt.id
    JOIN thread t USING (utid)
    WHERE s1.name LIKE '%SensorChanged%' AND (s2.name LIKE '%Network%' OR s2.name LIKE '%Send%')
    GROUP BY thread_name, start_event, end_event
    LIMIT 5;
"""
res9 = run_query(s9_title, s9_query, s9_desc)
if res9:
    for row in res9:
        print(f"Processing thread: {row.thread_name}")
        print(f"  🔹 Start event: {row.start_event} ➡️ End event: {row.end_event}")
        print(f"  🔹 Average final processing and network latency: {row.avg_latency_ms:.3f} milliseconds")
else:
    print("⚠️ Note on Question 9: If output is empty, tell teammates to name network send start and end flags precisely.")


# ==============================================================================
# Question 10: Breaking down and tracking thread activities (Threads)
# ==============================================================================
s10_title = "Question 10: Breaking down thread responsibilities (Main, Sensor, Network, UI) and load on main thread"
s10_desc = "Examine the processing share of each thread from the total 10-second system time to find processing bottlenecks."
s10_query = """
    SELECT 
        t.name as thread_name,
        COUNT(s.id) as total_slices,
        SUM(s.dur)/1e6 as total_active_time_ms,
        AVG(s.dur)/1e6 as avg_slice_dur_ms
    FROM slice s
    JOIN thread_track tt ON s.track_id = tt.id
    JOIN thread t USING (utid)
    GROUP BY thread_name
    ORDER BY total_active_time_ms DESC
    LIMIT 8;
"""
res10 = run_query(s10_title, s10_query, s10_desc)
if res10:
    for row in res10:
        print(f"🧵 System thread name: {row.thread_name:<20} | Total net activity time: {row.total_active_time_ms:.2f} ms (including {row.total_slices} events)")


# ==============================================================================
# Question 11: Difference between slow and sudden phone movement on processing and latency
# ==============================================================================
s11_title = "Question 11: Compare system behavior in slow movement versus sudden movement (Burst Movements)"
s11_desc = "Examine changes in processing rate and Context Switches in different seconds of the trace."
s11_query = """
    SELECT 
        (ts - (SELECT MIN(ts) FROM slice))/1e9 as second_bucket,
        COUNT(*) as events_per_second
    FROM slice
    WHERE name LIKE '%Sensor%' OR name LIKE '%Filter%'
    GROUP BY second_bucket
    ORDER BY second_bucket ASC;
"""
res11 = run_query(s11_title, s11_query, s11_desc)
if res11:
    print("📊 Distribution of processing volume over 10-second interval (broken down by second):")
    for row in res11:
        print(f"  Second {int(row.second_bucket)}: Number of processes and executed functions = {row.events_per_second}")

print("\n" + "="*80)
print("✅ [PERFETTO] Analysis complete. Use these scientific metrics to populate your report answers!")
print("="*80)