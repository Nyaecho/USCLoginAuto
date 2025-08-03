import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;

/*
 * 监控仪表盘UI类
 * 实现逻辑：
 * 显示三类数据
 * 定时自动刷新数据
 * 退出程序按钮
 * 捕获报错并同时弹窗和写入日志（待实现）
 * 报错弹窗包含两个按钮，其中一个为“关闭程序”，另一个为“重新启动”按钮（待实现）
 */

 /**
 * @brief 监控仪表盘UI类 
 * 用于实现所有的UI组件和逻辑
 * @Label timeLabel、 ipLabel、statusLabel
 */
public class UI extends JFrame {
    // 组件声明
    private JLabel timeLabel;// 校园网使用已使用时间标签
    private JLabel ipLabel;// 校园网使用已使用流量标签
    private JLabel statusLabel;// 校园网状态标签（正常/异常）
    private TrayIcon trayIcon; // 托盘图标，用于显示状态指示灯
    private Image false_State = getIconImage(Color.RED).getImage();//托盘状态指示灯
    private Image true_State = getIconImage(Color.GREEN).getImage();//托盘状态指示灯

    /**
     * @brief UI构造函数
     * 初始化窗口、组件和布局
     */
    @SuppressWarnings("static-access")
    public UI() {
        // 窗口基础设置
        setTitle("监控仪表盘");
        setSize(500, 300);
        setDefaultCloseOperation(JFrame.HIDE_ON_CLOSE); // 设置关闭操作为隐藏窗口
        setLayout(new BorderLayout(10, 10));
        setResizable(false); // 禁止调整窗口大小
        setLocationRelativeTo(null); // 窗口居中显示
        
        // 2. 数据展示面板
        /*
         * 创建一个面板，包含“校园网使用时间”、“校园网使用流量”和“校园网连接状态”三个指标。
         */
        JPanel dataPanel = new JPanel(new GridLayout(4, 1, 3, 10));
        dataPanel.setBorder(BorderFactory.createTitledBorder("系统状况"));
        
        // 时间字符串
        JPanel timePanel = createDataRow("已使用时间（截至登录时刻）:");
        timeLabel = new JLabel();
        timePanel.add(timeLabel);
        dataPanel.add(timePanel);
        
        // 网络流量
        JPanel ipPanel = createDataRow("连接IP地址：");
        ipLabel = new JLabel();
        ipPanel.add(ipLabel);
        dataPanel.add(ipPanel);
        
        
        // 系统状态
        JPanel statusPanel = createDataRow("校园网连接状态:");
        statusLabel = new JLabel();
        statusPanel.add(statusLabel);
        dataPanel.add(statusPanel);
        
        add(dataPanel, BorderLayout.CENTER);
        
        // 3. 关闭按钮（底部）
        JButton refreshButton = new JButton("退出程序");
        refreshButton.addActionListener(e -> Exit());
        JPanel buttonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        buttonPanel.add(refreshButton);
        add(buttonPanel, BorderLayout.SOUTH);

        // 4.创建托盘
        SystemTray(); // 添加系统托盘图标
    }
    
    private JPanel createDataRow(String title) { // 创建数据行面板基本方法
        JPanel panel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        JLabel titleLabel = new JLabel(title);
        panel.add(titleLabel);
        return panel;
        
    }

    /**
     * @brief 弹出错误对话框
     * @param message 传入具体的报错信息
     */
    public static void ErrorDialog(String message) { // 弹出错误对话框
        String[] options = {"重启", "退出"};
        int choice = JOptionPane.showOptionDialog(
                null,
                message,
                "出错啦！",
                JOptionPane.DEFAULT_OPTION,
                JOptionPane.ERROR_MESSAGE,
                null,
                options,
                options[0]
        );
        if (choice == 0) { // 重启
            try {
                // 获取当前Java运行环境
                String javaBin = System.getProperty("java.home") + "/bin/java";
                String jarPath = new java.io.File(UI.class.getProtectionDomain().getCodeSource().getLocation().toURI()).getPath();
                // 重新启动主类
                new ProcessBuilder(javaBin, "-jar", jarPath).start();
            } catch (Exception e) {
                JOptionPane.showMessageDialog(null, "重启失败：" + e.getMessage());
            }
            System.exit(0);
        } else if (choice == 1) { // 退出
            System.exit(0);
        }
    }
    
    // 核心功能方法
    //String[] 0:ip 1:time 2:流量
    public void Update(String[] Data) { 
        // 更新时间和IP地址
        timeLabel.setText(Data[1]); // 设置已使用时间
        ipLabel.setText(Data[0]); // 设置连接IP地址
    }

    public void refreshStatus(Boolean isConnected) { // 由连接状态检测线程触发更新
        // 刷新校园网连接状态
        if (isConnected) {
            statusLabel.setText("● 正常");
            statusLabel.setForeground(Color.GREEN);
            if (trayIcon != null) {
                trayIcon.setImage(true_State); // 更新托盘图标为绿色
            }

        } else {
            statusLabel.setText("● 异常,重连ing...");
            statusLabel.setForeground(Color.RED);
            ExecuteBody EB = new ExecuteBody(); // 创建ExecuteBody类的实例
            //Update(EB.Login(EB.getDriver())); // 重新执行登录操作
            if (trayIcon != null) {
                trayIcon.setImage(false_State); // 更新托盘图标为红色
            }
        }
    }

    private void Exit() { // 退出程序
        int choice = JOptionPane.showConfirmDialog(this, "确定要关闭整个程序吗(而不是隐藏窗口)？", "退出确认", JOptionPane.YES_NO_OPTION);
        if (choice == JOptionPane.YES_OPTION) {
            System.exit(0); // 退出程序
        }else if (choice == JOptionPane.NO_OPTION) {
            setVisible(false); // 隐藏窗口
        }
    }
    
    /**
     * @brief 创建系统托盘图标
     * 添加右键菜单和双击事件
     * 托盘图标显示连接状态指示灯（红色/绿色）
     * 双击托盘图标恢复窗口并置顶
     */
    private void SystemTray() {
        if (!SystemTray.isSupported()) {
            System.err.println("不支持托盘");
            Log.ExceptionLogPrint(Log.INFO, "系统托盘添加失败，可能是不支持添加托盘");
            return;
        }
        
        final SystemTray tray = SystemTray.getSystemTray();
        
        // 创建右键弹出菜单
        PopupMenu popup = new PopupMenu();
        MenuItem exitItem = new MenuItem("Exit");
        exitItem.addActionListener(e -> System.exit(0));
        popup.add(exitItem);

        // 创建托盘图标
        trayIcon = new TrayIcon(false_State, "校园网连接状态指示器", popup);
        trayIcon.setImageAutoSize(true);
        
        // 添加单击事件
        trayIcon.addMouseListener(new MouseAdapter() {
            public void mouseClicked(MouseEvent e) {
                if (SwingUtilities.isLeftMouseButton(e)) {
                    setVisible(true);
                    setExtendedState(JFrame.NORMAL); // 恢复窗口状态
                    toFront(); // 窗口置顶
                }
            }
        });

        // 添加到系统托盘
        try {
            tray.add(trayIcon);
        } catch (AWTException ex) {
            System.err.println("托盘添加出错");
            Log.ExceptionLogPrint(Log.ERROR, "托盘添加出错: " + ex.getMessage());
            
        }
    }

    /**
     * @brief 获取对应颜色的点状状态图标的ImageIcon
     * @param color
     * @return ImageIcon
     */
    private ImageIcon getIconImage(Color color) {
        // 2. 创建空白图像（16x16像素，ARGB色彩空间）
        BufferedImage image = new BufferedImage(16, 16, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g2d = image.createGraphics();
        
        // 设置透明背景
        g2d.setComposite(AlphaComposite.Clear);
        g2d.fillRect(0, 0, 16, 16);
        g2d.setComposite(AlphaComposite.SrcOver);
        
        // 设置抗锯齿渲染
        g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, 
                            RenderingHints.VALUE_ANTIALIAS_ON);
        
        // 绘制状态指示灯（居中圆点）
        int dotSize = 10; // 圆点直径
        int x = (16 - dotSize) / 2;
        int y = (16 - dotSize) / 2;
        
        g2d.setColor(color);
        g2d.fillOval(x, y, dotSize, dotSize);
        
        // 添加白色边框增强对比度
        g2d.setColor(Color.WHITE);
        g2d.setStroke(new BasicStroke(1f));
        g2d.drawOval(x, y, dotSize, dotSize);
        
        g2d.dispose();

        return new ImageIcon(image);
    }



    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            new UI().setVisible(true); // 确保GUI线程安全[9](@ref)
        });
    }
}


