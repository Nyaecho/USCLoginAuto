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



def core_task(task_queue: "queue.Queue", stop_event: threading.Event):
    """核心线程必须监听 stop_event！"""
    try:
        print("⚙️ 自动认证服务启动")
        config_path = os.path.join(os.path.dirname(__file__), "config.json")
        # 读取配置
        with open(config_path, "r", encoding="utf-8") as f:
            config = json.load(f)

        # 读取需要的信息 - Cookie 和 CSRF Token
        cookie = config.get("cookie", "")
        csrf_token = config.get("csrf_token", "")

        # 读取需要的信息 - 认证服务器地址
        auth_server = config.get("auth_server", "")
        
        if auth_server.startswith(('http://', 'https://')):
            parsed = urlparse(auth_server)
            auth_server = parsed.hostname or parsed.netloc # 只取主机名部分
            config["auth_server"] = auth_server
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(config, f, ensure_ascii=False, indent=2) 

        
        if not cookie or not csrf_token:
            cookie, csrf_token = get_cookie_and_csrf()
            config["cookie"] = cookie
            config["csrf_token"] = csrf_token
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(config, f, ensure_ascii=False, indent=2) 

        # 读取需要的信息 - 账号密码
        UserCredentials = config.get("UserCredentials", {})
        username = UserCredentials.get("username", "")
        password = UserCredentials.get("password", "")
        credentials = (username, password)

        # 读取需要的信息 - 网络稳定性检测参数
        stability_config = config.get("check_network_stability", {})
        target = stability_config.get("target", "www.bing.com")
        count = stability_config.get("count", 1)
        loss_threshold = stability_config.get("loss_threshold", 0.0)

        if not check_status(auth_server): # 初始状态检查
            print("初始化认证状态：认证失效，正在重新登录...")
            login(
                auth_server=auth_server,
                cookie_value=cookie,
                csrf_token=csrf_token,
                credentials=credentials
            )
        else:
            print("初始化认证状态：认证有效")
        while not stop_event.is_set():  # 关键！检查停止信号
            try:
                if check_network_stability(target, count, loss_threshold) : 
                    continue
                else:
                    print("🔄 网络不稳定，尝试重新认证...")
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
            except UserException.PingException as e:
                if not is_connected_wlan():
                    print("❌ 未连接到无线局域网,等待连接")
                    time.sleep(60)
                    continue   
        print("⏹️ 核心线程收到停止信号，正在退出")

    except Exception as e:
        task_queue.put(e)
        raise

def check_network_stability(target, count, loss_threshold):
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
        raise requests.exceptions.Timeout("⏰ 请求超时")
    except requests.exceptions.RequestException as e:
        raise requests.exceptions.RequestException(f"❌ 网络错误: {e}")
    except UserException.getStatusException as e:
        raise e
    except Exception as e:
        raise Exception(f"❌ 未知错误: {e}")

def logout(auth_server="210.43.112.9"):
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
    
def is_connected_wlan() -> bool:
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
            
            # 只检查是否有"已连接"状态
            if result.returncode == 0 and "已连接" in result.stdout:
                print("✅ 已重新连接到无线局域网")
                return True
            
        return False
        
    except Exception as e:
        print(f"⚠️ WiFi 检测异常: {e}")
        return False