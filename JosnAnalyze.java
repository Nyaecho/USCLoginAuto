import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.File;
import java.io.IOException;
import java.util.Map;

public class JosnAnalyze {
    public static String USER;
    public static String PW;
    public static String url_School;
    public static String url_Test;
    public static String Key;
    public static String Value;

    static {
        File configFile = new File("config.json");
        // 检查配置文件是否存在
        if (!configFile.exists()) {
            // 创建默认配置文件
            try {
                configFile.createNewFile();
                // 写入默认配置内容
                String defaultConfig = """
                {
                    "setProperty":{
                        "Note": "Key is the name of the property, value is the path to the driver executable",
                        "key": "",
                        "value": ""
                    },
                    "user": "",
                    "password": "",
                    "url_School":"210.43.112.9",
                    "url_Test":"www.baidu.com"
                }
                """;
                java.nio.file.Files.write(configFile.toPath(), defaultConfig.getBytes()); // 写入默认配置内容
            } catch (IOException e) {
                // 处理异常，打印错误信息
                throw new RuntimeException("创建配置文件失败：" + e.getMessage()); //抛出错误强制停止进程
            }
        }
        
        try {
            // 创建ObjectMapper实例
            ObjectMapper objectMapper = new ObjectMapper();
            // 读取JSON文件并转换为Map
            Map<String, Object> config = objectMapper.readValue(new File("config.json"), new TypeReference<Map<String, Object>>() {});
            // 从Map中获取配置项
            USER = (String) config.get("user");
            PW = (String) config.get("password");
            url_School = (String) config.get("url_School");
            url_Test = (String) config.get("url_Test");
            Map<String, Object> Property = (Map<String, Object>) config.get("setProperty");
            Key = (String)Property.get("Key");
            Value = (String)Property.get("Value");
        } catch (IOException e) {
            // 处理异常，打印错误信息
            System.err.println("读取配置文件时发生错误：" + e.getMessage());
            Log.ExceptionLogPrint(Log.ERROR, "读取配置文件错误\n" + e.getMessage());
        }
    }
}
