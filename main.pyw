# main.py
import threading
import time
import sys
import os
import tkinter as tk
from tkinter import scrolledtext, filedialog, messagebox
import pystray
from PIL import Image, ImageDraw
from io import StringIO
import queue
import subprocess
import platform
import re
from guardian import start_guardian, request_stop
import json


# === 全局唯一 Tk 根窗口 ===
_root_tk = None
def get_root_tk():
    global _root_tk
    if _root_tk is None:
        _root_tk = tk.Tk()
        _root_tk.withdraw()
    return _root_tk

# === 内存日志缓冲区 ===
MAX_LOG_SIZE = 10 * 1024 * 1024

class CircularLogBuffer:
    def __init__(self, max_size=MAX_LOG_SIZE):
        self.buffer = StringIO()
        self.max_size = max_size
        self.lock = threading.Lock()

    def write(self, msg):
        with self.lock:
            self.buffer.write(msg)
            if self.buffer.tell() > self.max_size:
                content = self.buffer.getvalue()
                start = len(content) - int(self.max_size * 0.9)
                self.buffer = StringIO(content[start:])

    def get_content(self):
        with self.lock:
            return self.buffer.getvalue()

    def clear(self):
        with self.lock:
            self.buffer = StringIO()

log_buffer = CircularLogBuffer()

# === 捕获 print 输出 ===
class LogWriter:
    def __init__(self, mirror_stream_name="__stdout__"):
        # 在 pythonw 下 __stdout__/__stderr__ 可能为 None
        self.console = getattr(sys, mirror_stream_name, None)
        self.encoding = "utf-8"  # 兼容某些库访问 .encoding
        self._lock = threading.Lock()

    def write(self, msg):
        if not msg:
            return 0
        if msg.strip():
            timestamp = time.strftime("%H:%M:%S")
            # 添加时间戳 + 换行
            formatted_msg = f"[{timestamp}] {msg}"
            if not formatted_msg.endswith('\n'):
                formatted_msg += '\n'
            log_buffer.write(formatted_msg)
        # 若存在控制台则镜像输出；无控制台则静默
        if self.console:
            try:
                self.console.write(msg)
            except Exception:
                pass
        return len(msg)

    def flush(self):
        if self.console:
            try:
                self.console.flush()
            except Exception:
                pass

    # 兼容性：一些库会调用这些方法/属性
    def isatty(self): return False
    def writable(self): return True

sys.stdout = LogWriter("__stdout__")
sys.stderr = LogWriter("__stderr__")

# === 日志窗口 (使用 Toplevel) ===
class LogWindow:
    def __init__(self):
        self.window = None
        self.text_area = None
        self._auto_refresh_job = None
        self._auto_refresh_enabled = True  # 新增：开关状态
        self.toggle_btn = None  # 新增：按钮引用

    def show(self):
        root = get_root_tk()
        # 已存在窗口
        if self.window and self.window.winfo_exists():
            if self.window.state() == "withdrawn":
                self.window.deiconify()
            self.window.lift()
            self.window.focus_force()
            self._refresh_log()
            return

        # 重新创建窗口
        self.window = tk.Toplevel(root)
        self.window.title("校园网守护日志")
        self.window.geometry("700x500")
        self.window.protocol("WM_DELETE_WINDOW", self.hide)

        self.text_area = scrolledtext.ScrolledText(
            self.window,
            wrap=tk.CHAR,
            state=tk.DISABLED,
            font=("Consolas", 10)
        )
        self.text_area.pack(expand=True, fill=tk.BOTH, padx=5, pady=5)

        btn_frame = tk.Frame(self.window)
        btn_frame.pack(pady=5)

        # 保存按钮引用，初始文字根据状态设置
        self.toggle_btn = tk.Button(
            btn_frame,
            text=("⏸ 暂停自动刷新" if self._auto_refresh_enabled else "▶️ 继续自动刷新"),
            command=self.toggle_auto_refresh
        )
        self.toggle_btn.pack(side=tk.LEFT, padx=5)

        tk.Button(btn_frame, text="💾 保存日志为 TXT", command=self.save_log).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="🧹 清空日志", command=self.clear_log).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="🛑 退出程序", bg="#ff6b6b", fg="white", command=self.quit_app).pack(side=tk.RIGHT, padx=5)

        self._refresh_log()
        self._start_auto_refresh()

    def hide(self):
        if self.window and self.window.winfo_exists():
            self.window.withdraw()
            self._stop_auto_refresh()

    def _start_auto_refresh(self):
        # 窗口可见时每 500ms 刷新一次
        if not self.window or not self.window.winfo_exists():
            return
        if self._auto_refresh_job is None and self._auto_refresh_enabled:
            def tick():
                if self.window and self.window.winfo_exists() and self.window.state() != "withdrawn" and self._auto_refresh_enabled:
                    self._refresh_log()
                    self._auto_refresh_job = self.window.after(500, tick)
                else:
                    self._auto_refresh_job = None
            self._auto_refresh_job = self.window.after(500, tick)

    def _stop_auto_refresh(self):
        if self.window and self._auto_refresh_job is not None:
            try:
                self.window.after_cancel(self._auto_refresh_job)
            except Exception:
                pass
        self._auto_refresh_job = None

    def _refresh_log(self):
        if self.text_area:
            content = log_buffer.get_content()
            self.text_area.config(state=tk.NORMAL)
            self.text_area.delete(1.0, tk.END)
            self.text_area.insert(tk.END, content)
            self.text_area.config(state=tk.DISABLED)
            self.text_area.see(tk.END)

    def save_log(self):
        file_path = filedialog.asksaveasfilename(
            defaultextension=".txt", # 默认扩展名
             initialfile=time.strftime("%Y%m%d_%H%M%S") + ".txt", # 默认文件名为时间戳
            filetypes=[("文本文件", "*.txt")], # 过滤器
            title="保存日志" # 标题
        )
        if file_path:
            try:
                with open(file_path, "w", encoding="utf-8") as f:
                    f.write(log_buffer.get_content())
                messagebox.showinfo("成功", f"日志已保存至:\n{file_path}")
            except Exception as e:
                messagebox.showerror("错误", f"保存失败:\n{str(e)}")

    def clear_log(self):
        if messagebox.askyesno("确认清空", "确定要清空当前日志吗？"):
            log_buffer.clear()
            self._refresh_log()

    def quit_app(self):
        if messagebox.askokcancel("确认退出", "确定要退出校园网守护程序吗？"):
            shutdown_app()

    def toggle_auto_refresh(self):
        self._auto_refresh_enabled = not self._auto_refresh_enabled
        # 立即应用
        if self._auto_refresh_enabled:
            self._start_auto_refresh()
        else:
            self._stop_auto_refresh()
        # 同步更新按钮文字与图标
        if self.toggle_btn and self.toggle_btn.winfo_exists():
            self.toggle_btn.config(
                text=("⏸ 暂停自动刷新" if self._auto_refresh_enabled else "▶️ 继续自动刷新")
            )

# === 托盘图标 ===
def create_icon():
    #导入外部icon.png
    icon_path = "icon.png"
    if os.path.exists(icon_path):
        return Image.open(icon_path)

    width, height = 64, 64
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    dc = ImageDraw.Draw(image)
    dc.ellipse((8, 8, 56, 56), fill=(255, 182, 193))
    dc.ellipse((24, 20, 40, 36), fill=(255, 255, 255))
    dc.ellipse((18, 34, 30, 46), fill=(255, 255, 255))
    dc.ellipse((34, 34, 46, 46), fill=(255, 255, 255))
    return image

exit_queue = queue.Queue()
log_window = LogWindow()

def on_show_log(icon, item):
    # 在主线程调度 Tk 操作
    get_root_tk().after(0, log_window.show)

def on_exit(icon, item):
    exit_queue.put("exit_request")

def setup_tray():
    return pystray.Icon(
        "CampusGuardian",
        create_icon(),
        menu=pystray.Menu(
            pystray.MenuItem("显示日志", on_show_log, default=True),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("退出", on_exit)
        )
    )

def run_with_exit_check(tray_icon):
    root = get_root_tk()
    def check_exit():
        try:
            if exit_queue.get_nowait() == "exit_request":
                if messagebox.askokcancel("退出确认", "确定要退出校园网守护程序吗？"):
                    tray_icon.stop()
                    shutdown_app()
        except queue.Empty:
            pass
        root.after(500, check_exit)
    check_exit()
    tray_icon.run_detached()
    root.mainloop()

# === 程序控制 ===
def shutdown_app():
    print("🛑 正在关闭程序...")
    request_stop()
    time.sleep(1.5)
    os._exit(0)

# === SSID 检测函数 ===
def SSIDChecker(SSID: str) -> bool :
    target_ssid = SSID

    system_platform = platform.system()

    if system_platform == "Windows":
        try:
            # 获取当前控制台代码页并据此解码，避免 UnicodeDecodeError
            cp_bytes = subprocess.check_output("chcp", shell=True)
            m = re.search(rb"(\d+)", cp_bytes)
            codepage = m.group(1).decode("ascii") if m else "65001"
            encoding = f"cp{codepage}"

            raw = subprocess.check_output("netsh wlan show interfaces", shell=True)
            output = raw.decode(encoding, errors="ignore")

            # 只解析以 'SSID' 开头的字段，避免误匹配 'BSSID'
            for line in output.splitlines():
                if re.match(r"^\s*SSID\s*:", line, flags=re.IGNORECASE):
                    ssid = line.split(":", 1)[1].strip()
                    return ssid == target_ssid
            return False
        except subprocess.CalledProcessError:
            return False
    
    #在这里用elif添加对其他操作系统的支持，或者修改上面的部分以支持自己的系统
    else:
        raise NotImplementedError("Unsupported platform")

# === 主程序 ===
if __name__ == "__main__":
    print("校园网守护程序启动ing...")
    #检查有无config.json,没有就创建一个默认,并弹窗报错
    config_path = os.path.join(os.path.dirname(__file__), "config.json")
    if not os.path.exists(config_path):
        default_config = {
            "target_ssid": "Your_SSID_Here",
            "auth_server": "",
            "UserCredentials": {
                "username": "Your_Username_Here",
                "password": "Your_Password_Here"
            },
            "check_network_stability": {
                "tips": "这里存放检测网络连通性需要的参数，依次为 ping目标，ping次数，可以接受的丢包率百分比",
                "target": "www.bing.com",
                "count": 1, 
                "loss_threshold": 0.0
            },
            "cookie": "",
            "csrf_token": ""
            
        }
        try:
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(default_config, f, ensure_ascii=False, indent=2)
        except Exception as e:
            get_root_tk()
            messagebox.showerror("配置错误", f"无法创建默认配置文件:\n{e}")
            shutdown_app()
        get_root_tk() 
        messagebox.showerror("缺少配置", f"已生成默认配置文件:\n{config_path}\n请编辑后重启程序。")
        shutdown_app() 
    else:
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                config = json.load(f)
            target_ssid = config.get("target_ssid", "")
            UserCredentials = config.get("UserCredentials", {})
            auth_server = config.get("auth_server", "")
            if not UserCredentials.get("username") or not UserCredentials.get("password"):
                raise ValueError("用户名或密码不能为空")
            if not target_ssid:
                raise ValueError("目标SSID不能为空")
            if not auth_server:
                raise ValueError("认证服务器地址不能为空")
            if SSIDChecker(target_ssid):
                print("✅ 已连接到目标SSID，启动守护线程")
                start_guardian()  # 真实启动核心线程！
            else:
                print("⚠️ 未连接到目标SSID，守护线程未启动")
                exit(0)
            tray = setup_tray()
            try:
                run_with_exit_check(tray) 
            except KeyboardInterrupt:
                print("👋 手动中断")
                shutdown_app()
        except Exception as e:
            get_root_tk()
            messagebox.showerror("配置错误", f"无法读取配置文件:\n{e}")
            shutdown_app()


