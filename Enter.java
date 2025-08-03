import java.io.File;

//先实例化UI界面，将UI界面置空，随后启动线程检测
public class Enter {
    public static void main(String[] args) {
        if (JosnAnalyze.USER.isEmpty() || JosnAnalyze.PW.isEmpty()) { // 检查账号和密码是否为空
            Log.ExceptionLogPrint(Log.ERROR, "账号或密码未设置，请在配置文件中设置。"); // 传入异常信息
            return; // 退出程序
        }
        if (!(JosnAnalyze.Value != null && new File(JosnAnalyze.Value).exists())) {
            Log.ExceptionLogPrint(Log.ERROR, "浏览器驱动文件不存在，请检查配置文件中的路径是否正确");
            return; // 退出程序
        }
        
        UI ui = new UI(); //创建UI类的实例
        startRunnable.ui = ui; //将UI实例传递给startRunnable类
        /*
        SwingUtilities.invokeLater(() -> {
            ui.setVisible(true);
        });
        */
        new Thread(new startRunnable()).start(); // 启动连接状态检测线程
        Log.ExceptionLogPrint(Log.INFO,"连接状态检测线程已启动"); // 传入日志
    }
}

/**
 * @brief 后台连接状态检测线程
 * 此线程无限循环执行，直到报错，报错后会自动退出。
 * 需要设置为后台线程，随着UI界面的退出而退出。
 */
class startRunnable implements Runnable {
    static UI ui;
    private ExecuteBody EB = new ExecuteBody(); // 创建ExecuteBody类的实例
    public boolean LoginTag = false; // 初始连接状态
    private int noConnectSchoolSum = 5; //未连接校园网报错计数器,累计报错到一定程度弹窗提示

    public void run(){
        try {
            try{
            Boolean LoginTag = EB.JudgePing(); // 初始连接状态
            ui.refreshStatus(LoginTag); // 刷新UI界面连接状态
            Thread.sleep(30000); // 每隔30秒检查一次连接状态
            }catch (noConnectSchool e) {Thread.sleep(30000);}
            
            do{
                try{
                    Boolean LoginTagg = EB.JudgePing();
                    if (LoginTag != LoginTagg){ //当状态发生改变时调用刷新方法
                        LoginTag = LoginTagg; // 更新连接状态
                        ui.refreshStatus(LoginTag);
                    }
                    Thread.sleep(30000); // 每隔30秒检查一次连接状态
                }catch (noConnectSchool e) {
                    if (noConnectSchoolSum-- != 0) {
                        Thread.sleep(30000); // 等待半分钟
                        Log.ExceptionLogPrint(Log.INFO,"未连接到校园网,尝试重连...\n剩余" + noConnectSchoolSum + "次"); // 传入异常信息
                    }else {
                        Log.ExceptionLogPrint(Log.ERROR,"未连接到校园网"); // 传入异常信息
                        return;
                    }
                }
            }while(true);
        }catch (Exception e) {
            Log.ExceptionLogPrint(Log.ERROR,"连接状态检测线程出错" + e.getMessage()); // 传入异常信息
        }
    }
}
        