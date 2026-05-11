
BeShell 对 FastAccelStepper 库的包装组件。

## 快速开始

#### 1. 命令行安装

```
idf.py add-dependency beshell-FastAccelStepper
```

#### 2. 配置文件

也可以使用配置文件声明依赖。

在项目的 `main` 目录（ idf_component_register所在目录下 ）下建立文件 `idf_component.yml`:

```yml
name: "YourProjectName"
dependencies:
  become-cool/beshell-FastAccelStepper:
    version: ">=1.0.0"
```


然后重新编译项目，idf 构建工具会自动下载 beshell 和 beshell-FastAccelStepper 存放到 `managed_components` 
