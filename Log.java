import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;

public class Log {
    public static void LogPrint(String IP, String Time) {
        long timestamp = System.currentTimeMillis();
         //将时间戳转换为字符串格式
         LocalDateTime dateTime = LocalDateTime.ofInstant(Instant.ofEpochMilli(timestamp), ZoneId.systemDefault());
         DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
         String formattedTime = dateTime.format(formatter);
        
        // 拼接要写入文件的内容
        String content = 
                "[" + formattedTime + "]\n" +
                "IP 地址为：" + IP + "\n" +
                "已经使用时间为：" + Time + "\n" +
                "---------------------------------------\n\n";

        // 定义输出文件路径
        String LogFile = "Log.txt";

        // 将 String 写入文件
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(LogFile, true))) {
            writer.write(content);
            System.out.println("内容已成功写入文件：" + LogFile);
        } catch (IOException e) {
            System.err.println("写入文件时发生错误：" + e.getMessage());
        }
    }
    public static void ExceptionLogPrint (String message) { //异常信息写入日志
         // 获取当前时间戳
         long timestamp = System.currentTimeMillis();
         //将时间戳转换为字符串格式
         LocalDateTime dateTime = LocalDateTime.ofInstant(Instant.ofEpochMilli(timestamp), ZoneId.systemDefault());
         DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
         String formattedTime = dateTime.format(formatter);

        // 拼接要写入文件的内容
        String content = 
                "[" + formattedTime + "]\n" +
                message + "\n\n" ;
        // 定义输出文件路径
        String LogFile = "Log.txt";

        // 将 String 写入文件
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(LogFile, true))) {
            writer.write(content);
            System.out.println("内容已成功写入文件：" + LogFile);
        } catch (IOException e) {
            System.err.println("写入文件时发生错误：" + e.getMessage());
        }
    

    }
}
