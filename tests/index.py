import argparse
from datetime import datetime
import os
import csv
if __name__ == "__main__":
    # 通过命令行参数获取输出文件名 --result_file_name
    parser = argparse.ArgumentParser()
    parser.add_argument('--result_file_name', required=True, help='结果文件名')
    args = parser.parse_args()
    output_file_name = args.result_file_name
    target_dir = '../dist/data'
    current_time = datetime.now()
    time_str = current_time.strftime("%Y-%m-%d %H:%M:%S")
    index_file = os.path.join(target_dir,"index.csv")
    # 更新索引文件
    with open(index_file, 'a') as f:
        csv_writer = csv.writer(f)
        if os.path.getsize(index_file) == 0:  # 如果文件为空，写入表头
            csv_writer.writerow(['ts', 'filename'])
        csv_writer.writerow([time_str,output_file_name])