# XiaoKangOS 小白入门指南 - 什么是xiaojs API？

## 简单来说

想象一下，你在使用一个手机APP（比如微信），APP需要访问你的通讯录、相册、摄像头等手机功能。APP是怎么做到的呢？它使用手机系统提供的"接口"（API）来调用这些功能。

在XiaoKangOS中，网页（Web应用）就像手机APP一样，需要访问系统的各种功能。**xiaojs API 就是给网页用的"接口"，让网页可以操作电脑**。

---

## xiaojs API 能做什么？

### 1. 文件操作 (xiao.fs) 📁

就像你可以在文件管理器里查看、创建、修改文件一样，网页也可以！

```javascript
// 读取文件内容
const content = await xiao.fs.read('/home/xiao/我的文档.txt');

// 写入内容到文件
await xiao.fs.write('/home/xiao/日记.txt', '今天天气真好！');

// 查看文件夹里有什么
const 文件列表 = await xiao.fs.list('/home/xiao/');

// 检查文件是否存在
const 存在吗 = await xiao.fs.exists('/home/xiao/秘密.txt');
```

### 2. 进程管理 (xiao.process) 📋

想象任务管理器，网页也能查看正在运行的程序！

```javascript
// 查看所有正在运行的程序
const 程序列表 = await xiao.process.list();
// 会返回类似：[微信.exe, 浏览器.exe, 音乐.exe, ...]

// 结束某个程序（需要管理员权限）
await xiao.process.kill(1234);  // 1234是程序ID

// 查看某个程序的详细信息
const 信息 = await xiao.process.info(1234);
```

### 3. 系统信息 (xiao.system) 💻

网页可以查看电脑的各种信息！

```javascript
// 获取电脑基本信息
const 电脑信息 = await xiao.system.info();
// 返回：主机名、内核版本、运行时间、内存大小、CPU信息等

// 重启电脑（需要管理员权限）
await xiao.system.reboot();

// 关机（需要管理员权限）
await xiao.system.shutdown();

// 修改系统设置
await xiao.system.settings({
    主题: '深色',
    语言: '中文'
});
```

### 4. 网络管理 (xiao.network) 🌐

查看和设置网络连接！

```javascript
// 查看网络状态
const 网络列表 = await xiao.network.status();
// 返回：网卡名称、IP地址、MAC地址、接收/发送流量等

// 配置网络（需要管理员权限）
await xiao.network.configure({
    接口: 'eth0',
    IP: '192.168.1.100',
    子网掩码: '255.255.255.0',
    网关: '192.168.1.1'
});
```

### 5. 硬件信息 (xiao.hardware) 🔧

查看电脑硬件配置！

```javascript
// 查看硬件信息
const 硬件信息 = await xiao.hardware.info();
// 返回：CPU型号/核心数、内存总量、硬盘信息等

// 查看显卡信息（能不能玩游戏就靠它！）
const 显卡信息 = await xiao.gpu.status();
```

---

## 安全机制 🔒

看到上面有些功能需要"管理员权限"，这是为了保护你的电脑！

### 权限系统

就像手机APP第一次访问通讯录时会问你"允许吗"，XiaoKangOS也会问：

```
网页说：我想读取你的文件
系统说：可以吗？
用户说：允许！
系统说：好的，给你权限
```

### 沙箱保护

网页不能直接操作电脑，只能通过xiaojs API。而且API会检查：
- ✅ 你要访问的文件是否在允许的目录？
- ✅ 你有没有权限做这件事？
- ✅ 这操作安全吗？

**受保护的目录**（网页不能访问）：
- `/proc/` - 系统进程
- `/sys/` - 系统内核
- `/dev/` - 硬件设备
- `/root/` - 管理员文件夹

**可以访问的目录**：
- `/home/xiao/` - 用户文件夹
- `/mnt/persist/` - 持久数据
- `/tmp/` - 临时文件

---

## 错误处理

如果操作失败了怎么办？

```javascript
try {
    const 内容 = await xiao.fs.read('/不存在的文件.txt');
} catch (错误) {
    console.log('错误代码：' + 错误.code);
    console.log('错误信息：' + 错误.message);
}
```

常见错误：
- `EACCES` - 权限不足，就像你不能打开别人的加密文件
- `ENOENT` - 文件不存在，就像你找错了文件
- `EISDIR` - 把文件夹当文件用了
- `EPERM` - 操作被禁止，就像你不能删除系统文件

---

## 实际例子

### 例1：读取系统日志

```javascript
// 读取系统日志
const 日志 = await xiao.fs.read('/var/log/syslog');
console.log(日志);
```

### 例2：清理内存

```javascript
// 先看看有哪些进程占用内存多
const 进程列表 = await xiao.process.list();

// 按内存使用量排序
进程列表.sort((a, b) => b.memory - a.memory);

// 找出占用内存最多的进程
const 最占内存的 = 进程列表[0];
console.log(`最占内存的是：${最占内存的.name}，使用了${最占内存的.memory}%`);
```

### 例3：显示系统状态

```javascript
// 获取完整系统信息
const 信息 = await xiao.system.info();

console.log('=== 我的电脑状态 ===');
console.log(`主机名：${信息.hostname}`);
console.log(`运行时间：${Math.floor(信息.uptime/3600)}小时`);
console.log(`CPU：${信息.cpuModel}`);
console.log(`总内存：${Math.floor(信息.totalMemory/1024/1024)}MB`);
console.log(`可用内存：${Math.floor(信息.freeMemory/1024/1024)}MB`);
```

---

## 总结

**xiaojs API 就是让网页可以操作电脑的一套工具！**

- `xiao.fs` - 读写文件
- `xiao.process` - 管理程序
- `xiao.system` - 系统控制
- `xiao.network` - 网络设置
- `xiao.hardware` - 查看硬件

有了它，网页不只是显示内容，还能像真正的软件一样操作电脑！
