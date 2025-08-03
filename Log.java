import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;

/**
 * @brief 日志记录类
 * @apiNote 提供日志记录功能，包括创建日志文件、写入日志内容和异常信息。
 * @note 日志文件存储在 "Log" 目录下，文件名为当前日期的格式（如 "yyyy-MM-dd.txt"）。
 * @wrrning 需要单独的线程保证日志记录的线程安全性。
 */
public  class Log {
    static String LogFile; // 定义日志文件路径
    
    static{ // 静态代码块在类加载时执行一次，用于初始化日志文件路径
        // 创建日志目录
        File logDir = new File("Log");
        if (!logDir.exists()) {
            logDir.mkdirs();  // 创建多级目录
        }
        //创建日志文件
        LogFile = "Log/" + getTime("yyyy-MM-dd") + ".txt"; // 定义日志文件路径
    }

    /**
     * @brief 将 IP 地址 和 已经使用时间 写入日志文件
     * @param IP
     * @param Time
     */
    public static void LogPrint(String IP, String Time) {
         String formattedTime = getTime("HH:mm:ss"); // 获取当前时间并格式化为 "HH:mm:ss" 格式
        
        // 拼接要写入文件的内容
        String content = 
                "[" + formattedTime + "]\n" +
                "IP 地址为：" + IP + "\n" +
                "已经使用时间为：" + Time + "\n" +
                "---------------------------------------\n";

        // 将 String 写入文件
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(LogFile, true))) {
            writer.write(content);
            System.out.println("本次数据已经记录");
        } catch (IOException e) {
            System.err.println("数据记录时发生错误：\n" + e.getMessage());
            UI.ErrorDialog(
                "日志写入失败\n" + 
                e.getMessage()
                );
        }
    }
    
    /**
     * @brief 打印异常信息到日志文件
     * @param level 日志级别，支持 "Log.INFO" 和 "Log.ERROR"
     * @note INFO级别报错，记录但不中断程序；ERROR级别报错，记录并中断程序
     */
    static int ERROR = 1; // 定义错误级别常量
    static int INFO = 0; // 定义信息级别常量 
    public static void ExceptionLogPrint(int level, String message) {
        switch (level) {
            case 0:
                ExceptionLogWrite("[INFO]\n" + message); // 调用异常信息写入方法
                break;
            case 1:
                ExceptionLogWrite("[ERROR]\n" + message); // 调用异常信息写入方法
                UI.ErrorDialog(message); // 弹出错误对话框
                // 待添加中断程序逻辑的代码
                break;
            default:
                break;
        }
    }
    private static void ExceptionLogWrite (String message) { //异常信息写入日志
        String formattedTime = getTime("HH:mm:ss"); // 获取当前时间并格式化为 "HH:mm:ss" 格式

        // 拼接要写入文件的内容
        String content = 
                "\n[" + formattedTime + "]\n" +
                ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n" + 
                message + "\n" + 
                "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";

        // 将 String 写入文件
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(LogFile, true))) {
            writer.write(content);
            System.out.println("写入日志");
        } catch (IOException e) {
            System.err.println("写入日志时发生错误：\n" + e.getMessage());
        }
    

    }

    /**
     * @brief 获取当前时间戳并格式化为指定模式的字符串
     * @param pattern 时间格式模式，例如 "yyyy-MM-dd HH:mm:ss"
     * @return 格式化后的时间字符串
     */
    private static String getTime(String pattern) { // 获取当前时间戳并格式化为指定模式的字符串
        long timestamp = System.currentTimeMillis();
        LocalDateTime dateTime = LocalDateTime.ofInstant(Instant.ofEpochMilli(timestamp), ZoneId.systemDefault());
        DateTimeFormatter formatter = DateTimeFormatter.ofPattern(pattern);
        return dateTime.format(formatter);
    }
}
