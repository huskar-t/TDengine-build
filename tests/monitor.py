import argparse
import csv
import os
import subprocess
import sys
import psutil
import time
from datetime import datetime, timedelta

def start_process(process_name, command):
    """
    启动指定进程

    Args:
        process_name: 进程名称(用于记录)
        command: 启动进程的命令列表
    """
    print(f"启动进程 {process_name}...")
    proc = subprocess.Popen(command)
    return proc


def stop_process(proc):
    """
    停止指定进程

    Args:
        proc: 进程对象
    """
    print(f"停止进程 {proc.pid}...")
    if proc.poll() is None:  # 进程仍在运行
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            print(f"进程 {proc.pid} 超时，强制终止...")
            proc.kill()

def monitor_memory_usage(pid_list):
    """
    监控指定进程的内存使用情况

    Args:
        pid_list: 进程 id 列表
    """
    data = [0] * len(pid_list)  # 初始化数据列表
    for i, pid in enumerate(pid_list):
        try:
            # 获取进程内存使用情况
            ps = psutil.Process(pid)
            mem_info = ps.memory_info()
            mem_usage = mem_info.rss / 1024 / 1024  # 转换为MB
            data[i] = mem_usage
        except Exception as e:
            pass
    return data

def run_monitoring(proc_list,output_file,monitor_interval,monitor_time):
    """
    监控进程内存使用情况并保存到文件

    Args:
        proc_list: 进程对象列表
        output_file: 输出文件路径
        monitor_interval: 监控间隔时间（秒）
        monitor_time: 监控持续时间（秒）
    """
    taosd_pid = None
    # 获取 taosd 进程 id
    for proc in psutil.process_iter(['pid', 'name']):
        if proc.info['name'] == 'taosd':
            taosd_pid = proc.pid
            break
    if taosd_pid is None:
        print("未找到 taosd 进程，无法监控。")
        return
    pid_list = [taosd_pid]+[proc.pid for proc in proc_list]
    start_time = datetime.now()
    end_time = start_time + timedelta(seconds=monitor_time)
    data = [0]*(len(pid_list)+1)
    while datetime.now() < end_time:
        timestamp = int(time.time() * 1000)
        data[0] = timestamp
        mem_data_list = monitor_memory_usage(pid_list)
        for i, mem_data in enumerate(mem_data_list):
            data[i + 1] = mem_data
        # 打印内存使用情况
        csv_out = csv.writer(sys.stdout)
        csv_out.writerow(data)
        # 将数据写入文件
        with open(output_file, 'a') as f:
            csv_writer = csv.writer(f)
            csv_writer.writerow(data)
        time.sleep(monitor_interval)

if __name__ == "__main__":
    # 通过命令行参数获取输出文件名 --result_file_name
    parser = argparse.ArgumentParser()
    parser.add_argument('--result_file_name', required=True, help='结果文件名')
    args = parser.parse_args()
    output_file_name = args.result_file_name
    if not output_file_name:
        print("未提供输出文件名")
        sys.exit(1)
    # 检查输出文件名是否以 .csv 结尾
    if not output_file_name.endswith('.csv'):
        print("输出文件名必须以 .csv 结尾")
        sys.exit(1)
    # 配置要监控的进程 (示例配置)
    processes_config = {
        "conn": ["../debug/build/bin/conn_test"],
        "insert": ["../debug/build/bin/insert_test"],
        "query": ["../debug/build/bin/query_test"],
        "stmt2_bind": ["../debug/build/bin/stmt2_bind_test"],
    }
    monitor_interval = 15  # 监控间隔时间（秒）
    monitor_time = 60   # 监控持续时间（秒）
    target_dir = '../dist/data'
    print(f"监控时间: {monitor_time}秒")
    print(f"监控间隔: {monitor_interval}秒")
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    print(f"输出目录: {target_dir}")
    
    # 启动进程
    proc_list = []
    for name, cmd in processes_config.items():
        proc = start_process(name, cmd)
        proc_list.append(proc)

    # 运行监控
    current_time = datetime.now()
    time_str = current_time.strftime("%Y-%m-%d %H:%M:%S")
    # 创建文件,写行首
    process_name_list = list(processes_config.keys())
    output_file = os.path.join(target_dir, output_file_name)
    print(f"输出文件: {output_file}")
    headers = ['ts','taosd'] + process_name_list
    csv_out = csv.writer(sys.stdout)
    csv_out.writerow(headers)
    with open(output_file, 'w') as f:
        csv_writer = csv.writer(f)
        csv_writer.writerow(headers)
    index_file = os.path.join(target_dir,"index.csv")
    try:
        run_monitoring(proc_list, output_file,monitor_interval,monitor_time)
    finally:
        # 停止进程
        for proc in proc_list:
            stop_process(proc)
        print("所有进程已停止。")