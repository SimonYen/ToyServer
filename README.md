# ToyServer

C++编写的Webserver

## 目录结构

- src 源代码存放的目录

- include 头文件存放的目录

## 运行前的数据库准备

```sql
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```



## 编译方法

首先在项目的根目录上新建一个临时目录，用于保存cmake生成的中间文件，保持目录简洁（这里假设临时目录为build)。

```bash
mkdir build
```

进入build目录，并运行cmake ..

```bash
cd build && cmake ..
```

现在你应该能在build目录中发现由cmake自动生成的makefile文件了，开始编译！

```bash
make
```

编译后的二进制文件在根目录下。

```bash
cd .. && ./server
```

ENJOY~
