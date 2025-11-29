from core_module import logout as e
from core_module import get_cookie_and_csrf as a
from core_module import login as b
from time import sleep
# 使用示例

if __name__ == "__main__":
    e()
    cookie, csrf = a()
    sleep(10)
    credentials={"20244330424","330424"}
    print(credentials)
    b(cookie_value=cookie, csrf_token=csrf, credentials=credentials)
        
    