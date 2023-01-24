<p align="center">
    <img alt="GitHub Workflow Status" src="https://img.shields.io/github/workflow/status/zouxianyu/SysY-compiler/Build SysY-compiler and upload to xiji gitlab server">
    <img alt="Codiga" src="https://api.codiga.io/project/34978/status/svg">
    <img alt="GitHub" src="https://img.shields.io/github/license/zouxianyu/SysY-compiler">
</p>

# SysY编译器

基于LLVM框架实现的SysY（极简版C语言）编译器。支持将.sy文件编译为arm汇编语言文件。

（SysY语言相关资料：[https://gitlab.eduxiji.net/nscscc/compiler2022](https://gitlab.eduxiji.net/nscscc/compiler2022)）

## 二进制文件下载

基于github actions在线编译，可直接下载二进制文件。

（有hf后缀的版本使用Hard Float ABI，没有hf后缀的版本使用Soft Float ABI）

目前支持以下版本系统：

- ubuntu 22.04 (amd64)
- ubuntu 20.04 (amd64)

## 使用方式

不开启优化：

```bash
./sysy_compiler -S -o 输出文件.s 输入文件.sy
```

开启优化：

```bash
./sysy_compiler -S -o 输出文件.s 输入文件.sy -O2
```
