#! /usr/bin/python3

# 使用方式：
# ./test_wrapper.py <编译器路径> <项目根目录> <测试点相对路径> <-O2>

import sys
import subprocess
import os

if __name__ == '__main__':
    # 获得命令行参数
    compiler = sys.argv[1]
    home = sys.argv[2]
    test_path = home + '/' + sys.argv[3]

    test_name = test_path.split('/')[-1]

    # 编译到汇编文件
    cmd = [compiler, '-S', '-o', '/tmp/' + test_name + '.s', test_path + '.sy']
    if sys.argv.count('-O2') != 0:
        cmd.append('-O2')

    ret = subprocess.run(cmd)

    if ret.returncode != 0:
        exit(ret.returncode)

    # 编译 & 链接
    cmd = [
        'arm-linux-gnueabi-gcc',
        '/tmp/' + test_name + '.s',
        '-o',
        '/tmp/' + test_name + '.bin',
        '-L',
        home + '/runtime_lib',
        '-lsysy',
    ]

    ret = subprocess.run(cmd)

    if ret.returncode != 0:
        print("link failed!")
        exit(ret.returncode)

    # 使用QEMU运行
    cmd = [
        'qemu-arm',
        '-L',
        '/usr/arm-linux-gnueabi',
        '/tmp/' + test_name + '.bin',
    ]

    # 判断是否有输入文件
    if os.path.exists(test_path + '.in'):
        cmd.append('<')
        cmd.append(test_path + '.in')

    ret = subprocess.run(cmd, capture_output=True)

    # 删除.s文件和.bin文件
    os.remove('/tmp/' + test_name + '.s')
    os.remove('/tmp/' + test_name + '.bin')

    # 拼接返回值，并与正确输出比较
    output = ret.stdout.decode('utf-8')
    if not (len(output) == 0 or output.endswith('\n')):
        output += '\n'
    output += str(ret.returncode) + '\n'

    with open(test_path + '.out', 'r') as f:
        expected_output = f.read()

    if output == expected_output:
        exit(0)
    else:
        print("output:")
        print(output)
        print("expected output:")
        print(expected_output)
        exit(1)
