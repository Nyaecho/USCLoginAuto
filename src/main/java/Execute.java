import org.openqa.selenium.By;
import org.openqa.selenium.WebDriver;
import org.openqa.selenium.WebElement;
import org.openqa.selenium.edge.EdgeDriver;
import org.openqa.selenium.edge.EdgeOptions;
import org.openqa.selenium.support.ui.WebDriverWait;
import org.openqa.selenium.support.ui.ExpectedConditions;
import java.time.Duration;


class Execute {
    public static void excprtion(String user, String pw,WebDriver options){
        

        // 创建 Edge 浏览器实例
        WebDriver driver = options;

        try {
            // 访问网页
            driver.get("http://210.43.112.9/srun_portal_pc?ac_id=5&theme=basic");

            WebElement username = driver.findElement(By.id("username")); // 账号输入框的 ID
                username.clear();
                username.sendKeys(user); // 键入账号
            WebElement password = driver.findElement(By.id("password")); // 密码输入框的 ID
                password.clear();
                password.sendKeys(pw); // 键入密码


            WebElement button = driver.findElement(By.id("login")); // 替换为实际按钮的 ID
                button.click();

            WebDriverWait wait = new WebDriverWait(driver, Duration.ofSeconds(10));//设置等待元素加载时间
            WebElement IpResult = wait.until(ExpectedConditions.visibilityOfElementLocated(By.xpath("//*[@id='ip']"))); // IP元素的 ID
            WebElement TimeResult = wait.until(ExpectedConditions.visibilityOfElementLocated(By.xpath("//*[@id=\'used_time\']"))); // 使用时长元素的 ID
            String IP = IpResult.getText();// 获取 IP 的文本内容
            String Time = TimeResult.getText(); // 获取使用时间的文本内容
            
            Log.LogPrint(IP, Time); // 调用 LogPrint 方法，传入 IP 和使用时间

            System.out.println("执行完成");//控制台输出执行完成

           
        }catch (Exception e) {
            // 捕获异常并打印错误信息
            System.err.println("发生异常：" + e.getMessage());
            Log.ExceptionLogPrint(e.getMessage()); // 调用 ExceptionLogPrint 方法，传入异常信息
        }
    }

    public static EdgeDriver getDriver() { //防止反复创建实例
        System.setProperty("webdriver.edge.driver", "msedgedriver.exe");//设置浏览器驱动路径
        
        EdgeOptions options = new EdgeOptions();
        options.addArguments("--headless"); // 启用无头模式
        options.addArguments("--disable-gpu"); // 禁用 GPU 加速（可选）
        options.addArguments("--window-size=1920,1080"); // 设置窗口大小（可选）

        // 创建 Edge 浏览器实例
        return new EdgeDriver(options);
    }
}