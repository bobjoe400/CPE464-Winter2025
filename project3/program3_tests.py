#!/usr/bin/env python3
import subprocess
import time
import os
import hashlib
import signal

# Configuration
SERVER_EXEC = "./server"
RCOPY_EXEC  = "./rcopy"
SERVER_PORT = "12345"

# ---------------------------
# Helper Functions
# ---------------------------
def create_dir(path):
    """Create the directory if it does not exist."""
    os.makedirs(path, exist_ok=True)

def compute_md5(filename):
    """Compute the MD5 hash of a file."""
    hash_md5 = hashlib.md5()
    try:
        with open(filename, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
    except Exception as e:
        print(f"Error reading file {filename}: {e}")
        return None
    return hash_md5.hexdigest()

def run_command(cmd, log_filename):
    """
    Runs a command with its stdout and stderr redirected to log_filename.
    Returns the subprocess.Popen object and its log file handle.
    """
    log_file = open(log_filename, "w")
    # Note: We pass the file handle for both stdout and stderr.
    proc = subprocess.Popen(cmd, stdout=log_file, stderr=log_file)
    return proc, log_file

def kill_process(proc):
    try:
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=5)
    except Exception as e:
        print(f"Error terminating process: {e}")

# ---------------------------
# Test Case Functions
# ---------------------------
def run_sequential_test(from_file, base_downloaded, window_size, buffer_size, error_rate,
                          base_server_log, base_client_log, test_name, test_dir):
    """
    Runs a single test case up to 3 attempts.
    For each attempt, unique log and downloaded file names are generated and stored
    in the specified test subdirectory.
    Returns the best (shortest) successful time in seconds or None if failed.
    """
    best_time = None
    print(f"\n=== Running {test_name} ===")
    
    for attempt in range(1, 4):
        print(f"Attempt {attempt} of 3...")
        
        # Build unique filenames inside the test directory.
        downloaded_file = os.path.join(test_dir, f"{base_downloaded}_attempt{attempt}")
        server_log_file = os.path.join(test_dir, f"{base_server_log}_attempt{attempt}.log")
        client_log_file = os.path.join(test_dir, f"{base_client_log}_attempt{attempt}.log")
        
        # Build server command: stdbuf -oL -eL ./server <error_rate> <SERVER_PORT>
        server_cmd = ["stdbuf", "-oL", "-eL", SERVER_EXEC, f"{error_rate:.2f}", SERVER_PORT]
        server_proc, server_log_handle = run_command(server_cmd, server_log_file)
        
        # Allow server time to start.
        time.sleep(2)
        
        # Build client command:
        # stdbuf -oL -eL ./rcopy <from_file> <downloaded_file> <window_size> <buffer_size> <error_rate> localhost <SERVER_PORT>
        client_cmd = ["stdbuf", "-oL", "-eL", RCOPY_EXEC, from_file, downloaded_file,
                      str(window_size), str(buffer_size), f"{error_rate:.2f}", "localhost", SERVER_PORT]
        start_time = time.monotonic()
        client_proc, client_log_handle = run_command(client_cmd, client_log_file)
        
        client_proc.wait()
        elapsed = time.monotonic() - start_time
        
        # Stop the server.
        kill_process(server_proc)
        server_log_handle.close()
        client_log_handle.close()
        
        # Give OS a moment to free the port.
        time.sleep(1)
        
        # Compare checksums.
        orig_hash = compute_md5(from_file)
        downloaded_hash = compute_md5(downloaded_file)
        if orig_hash is None or downloaded_hash is None:
            print("Error computing checksum(s).")
        elif orig_hash == downloaded_hash:
            print(f"SUCCESS: {test_name} completed in {elapsed:.3f} seconds (attempt {attempt}).")
            best_time = elapsed if best_time is None or elapsed < best_time else best_time
            break
        else:
            print(f"FAIL: Checksums differ (attempt {attempt}). Original: {orig_hash}, Downloaded: {downloaded_hash}.")
    
    if best_time is None:
        print(f"Test {test_name} FAILED all 3 attempts.")
    else:
        print(f"Test {test_name} PASSED with best run time: {best_time:.3f} seconds.")
    return best_time

def run_concurrent_clients_test(results_dir):
    """
    Runs a concurrent test where one server handles up to three concurrent clients.
    Each client downloads a file concurrently.
    Unique downloaded file names and log files are generated and stored in a dedicated
    subdirectory under results_dir.
    """
    test_dir = os.path.join(results_dir, "concurrent")
    create_dir(test_dir)
    print("\n=== Running Concurrent Clients Test ===")
    
    # Define the client test parameters as a list of dictionaries.
    client_tests = [
        { "from_file": "random_small_text.txt",
          "downloaded": "downloaded_small_concurrent",
          "window": 10, "buffer": 1000, "error": 0.2,
          "base_client_log": "client_concurrent1", "test_name": "Concurrent Test: Small File" },
        { "from_file": "random_medium_text.txt",
          "downloaded": "downloaded_medium_concurrent",
          "window": 10, "buffer": 1000, "error": 0.2,
          "base_client_log": "client_concurrent2", "test_name": "Concurrent Test: Medium File" },
        { "from_file": "random_big_text.txt",
          "downloaded": "downloaded_big_concurrent",
          "window": 50, "buffer": 1000, "error": 0.1,
          "base_client_log": "client_concurrent3", "test_name": "Concurrent Test: Big File" },
    ]
    
    # Start one server for all concurrent clients.
    server_log_file = os.path.join(test_dir, "server_concurrent_attempt1.log")
    server_cmd = ["stdbuf", "-oL", "-eL", SERVER_EXEC, "0.20", SERVER_PORT]
    server_proc, server_log_handle = run_command(server_cmd, server_log_file)
    time.sleep(2)  # Allow server to initialize.
    
    client_procs = []
    start_times = {}
    for idx, test in enumerate(client_tests):
        downloaded_file = os.path.join(test_dir, f"{test['downloaded']}_attempt1")
        client_log_file = os.path.join(test_dir, f"{test['base_client_log']}_attempt1.log")
        client_cmd = ["stdbuf", "-oL", "-eL", RCOPY_EXEC, test["from_file"], downloaded_file,
                      str(test["window"]), str(test["buffer"]), f"{test['error']:.2f}", "localhost", SERVER_PORT]
        proc, log_handle = run_command(client_cmd, client_log_file)
        client_procs.append((proc, test, downloaded_file, log_handle))
        start_times[proc.pid] = time.monotonic()
    
    for proc, test, downloaded_file, log_handle in client_procs:
        proc.wait()
        elapsed = time.monotonic() - start_times[proc.pid]
        log_handle.close()
        exit_code = proc.returncode
        if exit_code == 0:
            print(f"SUCCESS: {test['test_name']} completed in {elapsed:.3f} seconds.")
        else:
            print(f"FAIL: {test['test_name']} failed with exit code {exit_code}.")
        orig_hash = compute_md5(test["from_file"])
        downloaded_hash = compute_md5(downloaded_file)
        if orig_hash == downloaded_hash:
            print(f"SUCCESS: Checksums match for {test['test_name']}.")
        else:
            print(f"FAIL: Checksums differ for {test['test_name']}. Original: {orig_hash}, Downloaded: {downloaded_hash}.")
    
    kill_process(server_proc)
    server_log_handle.close()
    time.sleep(1)
    print("Concurrent Clients Test completed.")

# ---------------------------
# Main
# ---------------------------
def main():
    results_dir = "test-results"
    create_dir(results_dir)
    
    # Sequential test cases: Create a subdirectory for each test.
    test1_dir = os.path.join(results_dir, "test1")
    create_dir(test1_dir)
    run_sequential_test(
        from_file="random_small_text.txt",
        base_downloaded="downloaded_small_w10",
        window_size=10,
        buffer_size=1000,
        error_rate=0.2,
        base_server_log="server_small_w10",
        base_client_log="client_small_w10",
        test_name="Test #1 (small, window=10, buffer=1000, error=0.2)",
        test_dir=test1_dir
    )
    
    test2_dir = os.path.join(results_dir, "test2")
    create_dir(test2_dir)
    run_sequential_test(
        from_file="random_medium_text.txt",
        base_downloaded="downloaded_medium_w10",
        window_size=10,
        buffer_size=1000,
        error_rate=0.2,
        base_server_log="server_medium_w10",
        base_client_log="client_medium_w10",
        test_name="Test #2 (medium, window=10, buffer=1000, error=0.2)",
        test_dir=test2_dir
    )
    
    test3_dir = os.path.join(results_dir, "test3")
    create_dir(test3_dir)
    run_sequential_test(
        from_file="random_big_text.txt",
        base_downloaded="downloaded_big_w50",
        window_size=50,
        buffer_size=1000,
        error_rate=0.1,
        base_server_log="server_big_w50",
        base_client_log="client_big_w50",
        test_name="Test #3 (big, window=50, buffer=1000, error=0.1)",
        test_dir=test3_dir
    )
    
    test4_dir = os.path.join(results_dir, "test4")
    create_dir(test4_dir)
    run_sequential_test(
        from_file="random_big_text.txt",
        base_downloaded="downloaded_big_w5",
        window_size=5,
        buffer_size=1000,
        error_rate=0.1,
        base_server_log="server_big_w5",
        base_client_log="client_big_w5",
        test_name="Test #4 (big, window=5, buffer=1000, error=0.1)",
        test_dir=test4_dir
    )
    
    # Concurrent test.
    run_concurrent_clients_test(results_dir)
    
    print("\nAll tests completed.")
    print("Check the 'test-results' directory for detailed output files and logs.")

if __name__ == "__main__":
    main()
