import java.io.File;

import org.openqa.selenium.WebDriver;

public class Enter {
    public static void main(String[] args) {
        String user = "";
        String pw = "";
        
        try {
            File driverFile = new File("msedgedriver.exe");
            if (!driverFile.exists()) {
                throw new RuntimeException("浏览器驱动文件不存在，请在根目录下放置相应驱动" );
            }else {
                Log.ExceptionLogPrint("浏览器驱动文件存在，程序开始运行" ); //调用ExceptionLogPrint方法
            }
        } catch (RuntimeException e) {
            Log.ExceptionLogPrint(e.getMessage()); //调用ExceptionLogPrint方法，传入异常信息
            return; // 终止程序
        }

        do { //死循环，保持程序一直运行
        try {
            
            boolean PingResult = PingTest.JudgePing(); //调用PingTest类中的JudgePing方法，返回值为boolean类型
            if (PingResult) { //如果返回值为true,即需要执行下列脚本
                WebDriver options = Execute.getDriver(); //调用Execute类中的getDriver方法，创建浏览器实例
                Execute.excprtion(user, pw, options); //调用Exception类中的excprtion方法，传入账号和密码以及实例
                options.quit(); //关闭浏览器实例
                System.gc(); //调用垃圾回收器，释放内存
            }

            Thread.sleep(600000); //每隔分钟执行一次
        } catch (InterruptedException e) {
            Log.ExceptionLogPrint(e.getMessage()); //调用ExceptionLogPrint方法，传入异常信息
        } catch (Exception e) {
            // 捕获未知错误
            Log.ExceptionLogPrint(e.getMessage()); //调用ExceptionLogPrint方法，传入异常信息
            break;
        }
      }while (true); //死循环，保持程序一直运行
   
    }      
}