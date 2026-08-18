# core_module.py
import threading
import re
import queue
import platform
import subprocess
from urllib.parse import urlparse
import UserException
import time
import requests
import json
import os

config_path = os.path.join(os.path.dirname(__file__), "config.json") # 配置文件路径
cookie = "" # yudear cookie 值
csrf_token = "" # CSRF Token 值
auth_server = "" # 认证服务器地址
credentials = ("", "") # (用户名, 密码) 元组


def load_config_from_file():
    """读取配置文件并初始化核心配置变量。"""
    global cookie, csrf_token, auth_server, credentials

    # 读取配置
    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)

    # 读取需要的信息 - Cookie 和 CSRF Token
    key_config = config.get("key", {})
    cookie = key_config.get("cookie", "")
    csrf_token = key_config.get("csrf_token", "")
    last_update = key_config.get("LastUpdate", "")

    # 读取需要的信息 - 认证服务器地址 和 SSID
    auth_server = config.get("auth_server", "")
    ssid = config.get("target_ssid", "")

    if auth_server.startswith(('http://', 'https://')):
        parsed = urlparse(auth_server)
        auth_server = parsed.hostname or parsed.netloc # 只取主机名部分
        config["auth_server"] = auth_server
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f, ensure_ascii=False, indent=2)

    # 如果缺少 Cookie 或 CSRF Token，则获取并保存
    if not cookie or not csrf_token or LastUpdate_check(last_update):
        cookie, csrf_token = get_cookie_and_csrf()
        config["key"]["cookie"] = cookie
        config["key"]["csrf_token"] = csrf_token
        config["key"]["LastUpdate"] = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f, ensure_ascii=False, indent=2)

    # 读取需要的信息 - 账号密码
    UserCredentials = config.get("UserCredentials", {})
    username = UserCredentials.get("username", "")
    password = UserCredentials.get("password", "")
    credentials = (username, password)

    # 读取需要的信息 - 网络稳定性检测参数
    network_config = config.get("check_network_stability", {})
    network_config_ping = network_config.get("with_ping", {})
    network_config_http = network_config.get("with_http", {})
    data = {"auth_server": auth_server}
    type = ""
    if network_config_ping.get("enabled", False):
        target = network_config_ping.get("target", "202.89.233.100")
        count = network_config_ping.get("count", 1)
        loss_threshold = network_config_ping.get("loss_threshold", 0.0)
        data = {"target": target,"count": count,"loss_threshold": loss_threshold}
        type = "ping"
    else:
        network_config_ping = None
        if network_config_http.get("enabled", False):
            http_url = network_config_http.get("url", "http://connectivitycheck.platform.hicloud.com/generate_204")
            timeout = network_config_http.get("timeout", 10)
            data = {"http_url": http_url,"timeout": timeout}
            type = "http"
        else:
            network_config_http = None

    return ssid, type, data


def core_task(task_queue: "queue.Queue", stop_event: threading.Event): # 传参： task_queue 用于传递异常， stop_event 用于监听停止信号
    """核心线程必须监听 stop_event！"""
    global cookie, csrf_token, auth_server, credentials
    try:
        print("⚙️ 自动认证服务启动")
        ssid, type, data = load_config_from_file()
        print("✔️核心线程配置导入完成")
        flag = 0
        while not stop_event.is_set():  # 关键！检查停止信号
            try:
                if flag >= 10:
                    print("ℹ️ 长时间网络稳定，继续保持监测中...")
                    flag = 0
                flag += 1
                if check_network(type, **data) :
                    time.sleep(60)
                    continue
                
                print("🔄 网络不稳定，尝试重新认证...")
                if not is_connected_wlan(ssid):
                    raise UserException.CheckWlanException
                if not check_status(auth_server):
                    print("认证失效，正在重新登录...")
                    login(
                        auth_server=auth_server,
                        cookie_value=cookie,
                        csrf_token=csrf_token,
                        credentials=credentials
                    )
                else:
                    print("认证有效，重新认证")
                    logout(auth_server)
                    time.sleep(10)
                    login(
                        auth_server=auth_server,
                        cookie_value=cookie,
                        csrf_token=csrf_token,
                        credentials=credentials
                    )
                time.sleep(60)
            except UserException.CheckWlanException as e:
                while(1):
                    time.sleep(60)
                    if is_connected_wlan(ssid):
                        break
                continue
            except requests.exceptions.RequestException:
                print("⚠️ 网络出现不可达错误，30s后重试")
                time.sleep(30)
                continue
            except UserException.LoginException :
                print(f"❌ 登录失败: 认证服务器不可达，10s后重试")
                time.sleep(10)
                continue
        print("⏹️ 核心线程收到停止信号，正在退出")
    
    except Exception as e:
        task_queue.put(e)
        raise

def check_network_stability_ping(target, count, loss_threshold):
    """
    检测网络稳定性

    警告: 该函数可能抛出 Exception.PingException, 需要调用方处理.
    Args:
        target (str): 目标主机地址
        count (int): ping 测试次数
        loss_threshold (float): 丢包率阈值，超过则视为不稳定
    Returns:
        bool: 网络是否稳定
    """
    try:
        # 获取系统类型
        system = platform.system().lower()
        
        if system == "windows":
            # Windows ping 命令
            cmd = ["ping", "-n", str(count), target]
            # Windows 输出中的丢包行示例：
            # 丢包率 = 20% (2/10 个)
            loss_pattern = r"(\d+)%.*丢失"
            creation_flags = subprocess.CREATE_NO_WINDOW
        else:
            # Linux/Mac ping 命令
            cmd = ["ping", "-c", str(count), target]
            # Linux 输出中的丢包行示例：
            # 10 packets transmitted, 8 received, 20% packet loss
            loss_pattern = r"(\d+)%.*packet loss"
            # 阻止弹出控制台窗口
            creation_flags = 0
        # 执行 ping 命令
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=count * 3 + 5,  # 超时30秒
            creationflags=creation_flags
        )
        
        if result.returncode != 0: # 非零返回码表示命令失败
            raise UserException.PingException(f"⚠️ ping 命令执行失败: {result.stdout}\ncode={result.returncode}")
        
        # 从输出中提取丢包率
        output = result.stdout
        match = re.search(loss_pattern, output)
        
        if match:
            loss_rate = float(match.group(1))
            if loss_rate > 0.1:
                print(f"🌐 ping {target}: 丢包率 {loss_rate:.1f}%")
            
            if loss_rate > loss_threshold:
                return False
            else:
                return True
        else:
            # 如果没找到丢包信息，假设全部丢失
            print(f"❓ 无法解析 ping 输出，假设网络异常")
            return False
            
    except subprocess.TimeoutExpired:
        print(f"⏰ ping {target} 超时")
        return False
    except UserException.PingException as e:
        raise 
    except Exception as e:
        print(f"⚠️ 出现预料之外的错误: {e}")
        raise e

def get_cookie_and_csrf(auth_server: str = "210.43.112.9") -> tuple:
    """
    获取认证所需的 Cookie 和 CSRF Token
    警告： 该函数可能抛出 UserException.getTokenAndCookieException, 需要调用方处理.
    :param auth_server: 认证服务器地址
    :return: tuple (yudear_cookie, csrf_token) 或 (None, None)
    """
    try:
        # 发送 GET 请求获取 CSRF Token（不使用会话管理）
        url = f"http://{auth_server}/api/csrf-token"
        response = requests.get(
            url,
            headers={
                "X-Requested-With": "XMLHttpRequest",
                "Accept-Language": "zh-CN,zh;q=0.9",
                "Connection": "keep-alive"
            },
            timeout=10
        )
        
        # 检查响应
        if response.status_code != 200:
            raise UserException.getTokenAndCookieException(f"❌ 获取 CSRF Token 失败: HTTP {response.status_code}")
            
        # 解析 JSON
        token_data = response.json()
        csrf_token = token_data.get("csrf_token")
        if not csrf_token:
            raise UserException.getTokenAndCookieException("❌ 响应中缺少 CSRF Token")
        print(f"✅ 成功获取 CSRF Token: {csrf_token[:5]}...")
        
        # 获取 Cookie
        yudear = None
        cookies = response.headers.get('Set-Cookie', '')
        if "yudear=" in cookies:
            yudear = cookies.split('yudear=')[1].split(';')[0]
        if yudear:
            print(f"🍪 Cookie: yudear={yudear[:5]}...")
        else:
            print("🍪 Cookie: 未获取到 yudear")
        
        return yudear, csrf_token
        
    except requests.exceptions.Timeout as e:
        print("⏰ 请求超时")
        raise e
    except requests.exceptions.RequestException as e:
        print(f"❌ 网络错误: {e}")
        raise e
    except UserException.getTokenAndCookieException as e:
        raise e
    except Exception as e:
        print(f"❌ 未知错误: {e}")
        raise e
    
def check_status(auth_server):
    """
    检测校园网连接状态（无需任何认证参数！）
    
    :param auth_server: 认证服务器地址
    :return: 
        True = 已连接（在线）
        False = 未连接（不在线）
    """
    try:
        # 极简请求头
        
        url = f"http://{auth_server}/api/account/status"
        response = requests.get(
            url,
            headers= {},
            timeout=10
        )
        
        # 只要返回 200，就解析状态
        if response.status_code == 200:
            try:
                data = response.json()
                # 根据你的测试结果判断
                if data.get("code") == 0 and data.get("msg") == "在线":
                    return True   # 已连接
                elif data.get("code") == 1 and data.get("msg") == "不在线":
                    return False  # 未连接
                else:
                    # 未知状态，保守返回 False
                    raise UserException.getStatusException("❌ 未知的状态响应")
            except (ValueError, KeyError):
                raise UserException.getStatusException("❌ 无法解析状态响应")
        else:
            # 非200状态码视为未连接
            raise UserException.getStatusException(f"⚠️ 状态检测返回 HTTP {response.status_code}")
            
    except requests.exceptions.Timeout:
        print("⏰ 状态检测请求超时")
        return False
    except requests.exceptions.RequestException as e:
        raise requests.exceptions.RequestException(f"❌ 网络错误: {e}")
    except UserException.getStatusException as e:
        raise e
    except Exception as e:
        raise Exception(f"❌ 未知错误: {e}")

def logout(auth_server):
    """
    执行校园网登出操作
    警告: 该函数可能抛出 UserException.LoginOutException, 需要调用方处理.
    :param auth_server: 认证服务器地址
    :return: 
        True = 登出成功
        False = 登出失败
    """
    try:
        url = f"http://{auth_server}/api/account/logout"
        
        response = requests.get(
            url,
            timeout=10
        )
        
        if response.status_code == 200:
            try:
                data = response.json()
                code = data.get("code")
                msg = data.get("msg", "")
                
                if code == 0 :
                    print("✅ 登出成功")
                    return True
                elif code == 1 :
                    print("ℹ️ 已离线，无需重复登出")
                    return True
                else:
                    raise UserException.LogoutException(f"❌ 登出失败: {msg} (code={code})")
            except (ValueError, KeyError):
                raise UserException.ErrorException("❌ 无法解析登出响应")
        else:
            print(f"⚠️ 登出请求返回 HTTP {response.status_code}")
            return False
            
    except requests.exceptions.Timeout:
        print("⏰ 登出请求超时")
        return False
    except requests.exceptions.RequestException as e:
        raise UserException.LogoutException(f"❌ 网络错误: {e}")
    except UserException.LogoutException as e:
        raise e
    except Exception as e:
        raise 

def login(auth_server, 
        cookie_value=None, 
        csrf_token=None, 
        credentials=None,
        nas_id=1,
        isp="local"):
    """
    执行校园网登录操作
    
    :param auth_server: 认证服务器地址
    :param cookie_value: yudear cookie 值（必需）
    :param csrf_token: CSRF Token 值（必需）
    :param credentials: (username, password) 元组（必需）
    :param nas_id: NAS ID（默认 1）
    :param isp: ISP 类型（默认 "local"）
    :return: 
        0 = 登录成功
        1 = 账号密码错误
        2 = Cookie/Token 错误
        -1 = 其他错误
    """
    # === 参数校验 ===
    if not cookie_value:
        print("❌ Cookie 值不能为空")
        return -1
        
    if not csrf_token:
        print("❌ CSRF Token 不能为空")
        return -1
        
    if not credentials or len(credentials) != 2:
        print("❌ 凭据必须是 (username, password) 元组")
        return -1
        
    username, password = credentials
    if not username or not password:
        print("❌ 用户名或密码不能为空")
        return -1

    try:
        # === 构造请求 ===
        url = f"http://{auth_server}/api/account/login"
        
        # Cookie
        cookies = {"yudear": cookie_value}
        
        # Headers
        headers = {
            "X-CSRF-Token": csrf_token,
            "X-Requested-With": "XMLHttpRequest",
            "Accept-Language": "zh-CN,zh;q=0.9",
            "Accept": "*/*",
            "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
            "User-Agent": "Mozilla/5. .0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.6723.70 Safari/537.36",
            "Origin": f"http://{auth_server}",
            "Referer": f"http://{auth_server}/tpl/default/login_account.html?ip=10.14.75.198&nasId={nas_id}",
            "Connection": "keep-alive"
        }
        
        # 请求体
        data = {
            "username": username,
            "password": password,
            "nasId": str(nas_id),
            "isp": isp,
            "timeLimit": ""
        }
        
        # === 发送请求 ===
        response = requests.post(
            url,
            cookies=cookies,
            headers=headers,
            data=data,
            timeout=15
        )
        
        # === 处理响应 ===
        if response.status_code == 400:
            # CSRF token 错误
            try:
                error_data = response.json()
                if error_data.get("error") == "CSRF token mismatch":
                    print("❌ CSRF Token 或 Cookie 无效")
                    return 2
            except:
                pass
            print(f"❌ 登录请求失败: HTTP 400")
            return 2
            
        elif response.status_code == 200:
            try:
                result = response.json()
                code = result.get("code")
                msg = result.get("msg", "")
                
                if code == 0 and "认证成功" in msg:
                    print("✅ 登录成功")
                    return 0
                elif code == 1 and ("账号或密码错误" in msg or "E20002" in str(result.get("authCode", ""))):
                    print("❌ 账号或密码错误")
                    return 1
                else:
                    print(f"❓ 未知登录响应: code={code}, msg={msg}")
                    return -1
            except ValueError:
                print("❌ 登录响应不是有效的 JSON")
                return -1
        else:
            print(f"⚠️ 登录返回 HTTP {response.status_code}")
            return -1
            
    except requests.exceptions.Timeout:
        raise UserException.LoginException("⏰ 登录请求超时")
    except requests.exceptions.RequestException as e:
        raise UserException.LoginException(f"❌ 网络错误: {e}")
    except Exception as e:
        raise UserException.LoginException(f"❌ 未知错误: {e}")
    
def is_connected_wlan(SSID: str) -> bool:
    """
    当ping出现异常时，调用此函数检测是否连接到无线局域网
    :return: bool: True = 已连接无线局域网, False = 未连接无线局域网
    """
    try:
        if platform.system().lower() == "windows":
            result = subprocess.run(
                ["netsh", "wlan", "show", "interfaces"],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='ignore',
                timeout=3,
                creationflags= subprocess.CREATE_NO_WINDOW
            )
            
            # 检查连接状态（兼容中英文系统语言）
            if result.returncode == 0 and ("已连接" in result.stdout or re.search(r'\bconnected\b', result.stdout, re.IGNORECASE)):
                for line in result.stdout.splitlines():
                    if re.match(r"^\s*SSID\s*:", line, flags=re.IGNORECASE):
                        ssid = line.split(":", 1)[1].strip()
                        print(f"✔️ 已连接无线局域网: {ssid}")
                        if SSID == "":
                            return True  # 没有指定SSID，默认通过ssid一致性校验
                        return ssid == SSID
            else : 
                print("❌ 未连接到无线局域网,等待连接")
                return False
            
        return False
        
    except Exception as e:
        print(f"⚠️ WiFi 检测异常: {e}")
        return False
    
# 传入时间戳，并对比时间戳与当前的时间差，判断是否需要更新
def LastUpdate_check(LastUpdate: str) -> bool:
    if not LastUpdate:
        return True
    try:
        last_time = time.mktime(time.strptime(LastUpdate, "%Y-%m-%d %H:%M:%S"))
        current_time = time.time()
        # 如果时间差超过24*7小时，返回True
        if current_time - last_time > 24 * 7 * 3600:
            return True
        else:
            return False
    except Exception as e:
        print(f"⚠️ 时间戳解析异常: {e}")
        return True
    
def check_network_stability_http(http_url, timeout=5):
    """
    通过 HTTP HEAD 请求检测网络连通性
    比 ping 更准确，因为直接测试实际使用的协议
    """
    try:
        # HEAD 请求比 GET 更轻量（只返回头部）
        response = requests.head(
            http_url,
            timeout=timeout,
            allow_redirects=False # 不允许重定向
        )
        # 只要能建立连接并收到响应，就认为网络正常
        if response.status_code in [200, 400, 401, 403, 404, 204]:
            return True
        else:
            print(f"🌐 HTTP 检测异常状态码: {response.status_code}")
            return False
        
    except requests.exceptions.RequestException: # 任何请求异常都视为网络不稳定
        raise UserException.HTTPCheckException("❌ HTTP 网络检测出错啦")
    
def check_network(type: str = "", **data) -> bool:
    """
    简单的网络连通性检测
    """
    try:
        if type == "ping":  
            return check_network_stability_ping(data.get("target"), data.get("count"), data.get("loss_threshold"))
        elif type == "http":
            return check_network_stability_http(data.get("http_url"), data.get("timeout", 10))
        else:
            return check_status(data.get("auth_server"))
    except UserException.HTTPCheckException :
        print("❌ HTTP 网络检测出错啦!可能是外网不可达！")
        return False
    except Exception as e:
        raise e

def re_auth():
    """
    触发重新认证请求
    """
    logout(auth_server)
    time.sleep(2)
    login(
        auth_server=auth_server,
        cookie_value=cookie,
        csrf_token=csrf_token,
        credentials=credentials
    )
    time.sleep(10)