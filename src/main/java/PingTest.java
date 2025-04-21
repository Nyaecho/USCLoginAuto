import java.net.InetAddress;
import java.lang.Exception;

class PingTest {
    public static boolean JudgePing() {
        String TestIP = "www.baidu.com"; // 替换为需要检测的目标网站
        String SchoolIP = "210.43.112.9";
        int timeout = 2000; // 超时时间（毫秒）

        try {
            InetAddress SchoolAddress = InetAddress.getByName(SchoolIP);
            InetAddress TextAddress = InetAddress.getByName(TestIP); // 获取 IP 地址
            
            if (SchoolAddress.isReachable(timeout)) {//当前连接到校园网时
                if(!TextAddress.isReachable(timeout)) { //检测当前网络是否可用,若不可用
                    return true; //返回 true,即判断需要执行下列脚本
                }
            }
            return false; //返回 false,即判断不需要执行下列脚本
            
        
        }catch (Exception e) {
            Log.ExceptionLogPrint(e.getMessage());
            return false; // 返回 false，表示 ping 操作失败
            
        }
    }
}
