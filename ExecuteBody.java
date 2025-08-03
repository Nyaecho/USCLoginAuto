import org.openqa.selenium.By;
import org.openqa.selenium.StaleElementReferenceException;
import org.openqa.selenium.WebDriver;
import org.openqa.selenium.WebElement;
import org.openqa.selenium.edge.EdgeDriver;
import org.openqa.selenium.edge.EdgeOptions;
import org.openqa.selenium.support.ui.WebDriverWait;
import org.openqa.selenium.support.ui.ExpectedConditions;
import java.time.Duration;
import java.net.InetAddress;
import java.io.File;
import java.lang.Exception;

class ExecuteBody {
    
    private String user = JosnAnalyze.USER;
    private String pw = JosnAnalyze.PW;
    private String TestIP = JosnAnalyze.url_Test;
    private String SchoolIP = JosnAnalyze.url_School;
    
    /**
     * @brief 执行浏览器自动化脚本
     * @Note 访问指定的校园网登录页面，输入账号和密码，点击登录按钮，并获取登录后的 IP 地址和使用时间。
     * @param options
     * @return 返回一个字符串数组，包含 IP 地址和使用时间。如果发生异常，返回 null。执行过后销毁实例
     */
    
     public String[] Login(WebDriver driver){
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

            String[] Data = getData(driver); // 获取 IP 和使用时间

            //关闭实例和驱动
            driver.quit(); // 关闭浏览器实例
            System.out.println("浏览器自动化执行完成");//控制台输出执行完成
            return Data; // 返回 IP 和使用时间

           
        }catch (StaleElementReferenceException e){ //返回null表明需要重新执行
            // 捕获 StaleElementReferenceException 异常，表示元素不再存在
            System.err.println("元素引用失效");
            if (driver != null) {
                driver.quit(); // 确保关闭浏览器实例
            }
            return null; // 返回 null，表示执行失败
        }catch (Exception e) {
            // 捕获异常并打印错误信息
            Log.ExceptionLogPrint(Log.ERROR, e.getMessage()); // 传入异常信息
            if (driver != null) {
                driver.quit(); // 确保关闭浏览器实例
            }
            throw new RuntimeException("发生异常", e); // 抛出运行时异常并传入原始异常
        }
    }

    /**
     * @brief 从登录页面中获取 IP 地址和使用时间
     * @return String[] 0:ip 1:time 2:流量
     * @param WebDriver
     * @param *int 当传入任意数字时,则表示需要自行载入网页
     * @implNote 需要自行捕获异常,比如找不到元素
     */
    public String[] getData(WebDriver driver) {
        WebDriverWait wait = new WebDriverWait(driver, Duration.ofSeconds(10));//设置等待元素加载时间
        WebElement IpResult = wait.until(ExpectedConditions.visibilityOfElementLocated(By.xpath("//*[@id='ip']"))); // IP元素的 ID
        WebElement TimeResult = wait.until(ExpectedConditions.visibilityOfElementLocated(By.xpath("//*[@id=\'used_time\']"))); // 使用时长元素的 ID
        String Data[] = new String[2]; // 创建一个字符串数组来存储 IP
        Data[0] = IpResult.getText();// 获取 IP 的文本内容
        Data[1] = TimeResult.getText(); // 获取使用时间的文本内容
        Log.LogPrint(Data[0], Data[1]);
        return Data; // 返回包含 IP 和使用时间的字符串数组
    }
    public String[] getData (WebDriver driver, int a){ //当传入任意数字时,则表示需要自行载入网页
        driver.get("http://210.43.112.9/srun_portal_pc?ac_id=5&theme=basic");
        return getData(driver);
    } 

    /**
     * @brief 获取 Edge 浏览器驱动实例
     */
    public EdgeDriver getDriver() { //用于创建实例
        if (!(JosnAnalyze.Value != null && new File(JosnAnalyze.Value).exists())) {
            Log.ExceptionLogPrint(Log.ERROR, "浏览器驱动文件不存在，请检查配置文件中的路径是否正确");
            throw new RuntimeException("浏览器驱动文件不存在" );
        }
        System.setProperty(JosnAnalyze.Key, JosnAnalyze.Value);//设置浏览器驱动路径
        
        EdgeOptions options = new EdgeOptions();
        options.addArguments("--headless"); // 启用无头模式
        options.addArguments("--disable-gpu"); // 禁用 GPU 加速
        options.addArguments("--window-size=1920,1080"); // 设置窗口大小

        // 创建 Edge 浏览器实例
        return new EdgeDriver(options);
    }


    /**
     * @brief 判断网络连接是否可达
     * @Note 判断是否为校园网以及是否可以使用校园网
     * 当出现规则外连接时抛出 noConnectSchool
     * @return 返回是否可达，true 表示已经连接，false 表示未连接或不可达
     * 
     */
    public boolean JudgePing() { // 判断连接可达性
        int timeout = 2000; // 超时时间（毫秒）
        try {
            InetAddress SchoolAddress = InetAddress.getByName(SchoolIP);
            InetAddress TextAddress = InetAddress.getByName(TestIP); // 获取 IP 地址
            
            if (SchoolAddress.isReachable(timeout)) {//当前连接到校园网时
                if(TextAddress.isReachable(timeout)) { //检测校园网是否成功连接
                    return true; // 返回 true，表示连接成功
                }
                return false; //返回 false,即判断没有连接或者连接失败
            }
            throw new noConnectSchool(); // 抛出自定义异常，表示未连接到校园网或者ping出错
        }catch(noConnectSchool e){
            throw new noConnectSchool(); //向上抛出使得上游线程捕捉到
        }catch (Exception e) {
            throw new RuntimeException(e.getMessage());
        }
    }

}


class noConnectSchool extends RuntimeException{}