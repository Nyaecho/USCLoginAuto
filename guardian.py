# guardian.py
import threading
import time
import queue
import importlib

# 全局状态（避免重复启动）
_guardian_thread = None
_guardian_running = False
# 新增：全局停止信号（用于优雅退出所有线程）
_stop_all = threading.Event()
restartNum = 0

def _guardian_worker(core_task_func):
    """保活监控器的工作线程"""
    
    task_queue = queue.Queue() # 用于核心线程与守护线程通信
    core_thread = None # 核心线程引用

    def start_core():
        nonlocal core_thread 
        # 使用全局停止事件，不再使用局部 stop_event
        core_thread = threading.Thread(
            target=core_task_func, # 核心任务函数
            args=(task_queue, _stop_all), # 传入队列和全局停止事件
            daemon=True, # 核心线程作为守护线程，随主程序退出
            name="CoreWorker" # 线程名称
        )
        core_thread.start()
        print("🟢 核心线程已启动（守护模式）")

    # 先启动一次核心线程
    start_core()

    # 监控循环
    while _guardian_running and not _stop_all.is_set():
        # 检查核心线程是否存活
        if core_thread is None or not core_thread.is_alive():
            global restartNum
            restartNum += 1
            print(f"⚠️ 核心线程已停止，5s后重启... (重启次数: {restartNum})")
            time.sleep(5)
            start_core()

        # 可选：检查状态队列（非阻塞）
        try:
            msg = task_queue.get_nowait() 
            if isinstance(msg, Exception):
                print(f"💥 核心线程抛出异常: {msg}")
                # 可在这里记录日志或告警
        except queue.Empty: # 队列为空，忽略
            pass

        time.sleep(3)  # 每3秒检查一次

    # 清理：触发全局停止并等待核心线程优雅退出
    _stop_all.set()
    if core_thread and core_thread.is_alive():
        core_thread.join(timeout=2)
    print("🛑 保活线程已退出")


def start_guardian(core_module_path="core_module", core_func_name="core_task"):
    """
    启动保活线程（供外部调用）
    
    :param core_module_path: 核心模块路径（如 "my_core"）
    :param core_func_name: 核心函数名（默认 "core_task"）
    """
    global _guardian_thread, _guardian_running

    if _guardian_running:
        print("ℹ️ 保活线程已在运行")
        return

    # 动态导入核心函数
    try:
        core_module = importlib.import_module(core_module_path)
        core_func = getattr(core_module, core_func_name)
    except (ImportError, AttributeError) as e:
        print(f"❌ 无法加载核心函数: {e}")
        return

    # 启动前重置全局停止信号
    _stop_all.clear()

    # 标记运行中 & 启动线程
    _guardian_running = True
    _guardian_thread = threading.Thread(
        target=_guardian_worker,
        args=(core_func,),
        daemon=True,  # 保活线程自身是守护线程（随主程序退出）
        name="Guardian"
    )
    _guardian_thread.start()
    print("🛡️ 保活线程启动OK")


def stop_guardian():
    """可选：提供停止接口"""
    global _guardian_running
    # 设置运行标志为 False，并触发全局停止事件
    _guardian_running = False
    _stop_all.set()

# 新增：通知所有线程退出（供主线程在退出前调用）
def request_stop():
    """建议在主程序退出前调用，确保优雅关闭"""
    global _guardian_running
    _guardian_running = False
    _stop_all.set()