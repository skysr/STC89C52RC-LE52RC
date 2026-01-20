#include <at89x52.h>


// 定义LED引脚
#define LED P0_0

#define QC1602_DATA  P2 

#define QC1602RS P0_1
#define QC1602EN P0_2

// 全局变量
// 全局变量
unsigned int led_timer2_count = 0;
unsigned char led_state = 0;
unsigned int lcd_update_count_flag = 0;
unsigned long lcd_update_count = 0;
char lcd_update_count_str[10] = {0};

// 函数声明
void Timer2_Init(void);
// LED状态切换函数声明
void Toggle_LED(void);
//lcd的的命令函数
static void lcd_cmd(unsigned char cmd);

/* 使能脉冲 */
static void lcd_en_pulse(void)
{
    QC1602EN = 1;
    ;;;
    QC1602EN = 0;
}

/* 写命令 */
static void lcd_cmd(unsigned char cmd)
{
    QC1602RS = 0;            // 命令
    QC1602_DATA = cmd;       // 一次送出 8 位
    lcd_en_pulse();
    if(cmd == 0x01 || cmd == 0x02)
    { unsigned int i = 2500; while(--i); }  // 清屏需要 >1.52 ms
    else
    { unsigned char i = 50; while(--i); }   // 其余 >40 us
}

/* 写数据 */
static void lcd_dat(unsigned char dat)
{
    QC1602RS = 1;            // 数据
    QC1602_DATA = dat;
    lcd_en_pulse();
    { unsigned char i = 50; while(--i); }
    QC1602RS = 0;
}

/* 1602 初始化（8 位模式） */
void QC1602_Init8(void)
{
    unsigned char i;
    /* 上电延时 >15 ms */
    for(i = 0; i < 15; i++){ unsigned int j = 11092; while(--j); }

    lcd_cmd(0x38);           // 8 位、2 行、5×7
    lcd_cmd(0x0C);           // 显示开，光标关
    lcd_cmd(0x06);           // 地址自增
    lcd_cmd(0x01);           // 清屏
}

/* 在指定行列打印字符串 */
void QC1602_Print8(unsigned char row, unsigned char col, char *str)
{
    lcd_cmd(0x80 | (row ? 0x40 + col : col));
    while(*str) lcd_dat(*str++);
}

// 定时器2初始化函数
void Timer2_Init(void)
{
    // 设置定时器2为16位自动重载模式
    T2CON = 0x00;    // 清除所有控制位
    
    //11.0592 MHz 定时50ms 
    TH2 = RCAP2H = 0x4C;
    TL2 = RCAP2L = 0x00;


    // 开启定时器2中断
    ET2 = 1;
    
    // 启动定时器2
    TR2 = 1;
}

// LED状态切换函数
void Toggle_LED(void)
{
    led_state = !led_state;
    LED = led_state;
}

//long lcd_update_count 转成字符串
void lcd_update_count_to_str(unsigned long count, char *str)
{
    unsigned char i;
    for(i = 0; i < 10; i++)
    {
        str[9 - i] = '0' + count % 10;
        count /= 10;
    }
}


void main()
{
    // 初始化定时器2
    Timer2_Init();
    // ① 8 位初始化
    QC1602_Init8();                 
    
    // 开启总中断
    EA = 1;

    QC1602_Init8();                  // ① 8 位初始化
    QC1602_Print8(0, 0, "hello 8bit"); // ② 第 1 行显示
    QC1602_Print8(1, 0, "D0-D7 used"); // ③ 第 2 行显示

    QC1602_Print8(0, 0, "LCD COUNT NUM:");
    
    while(1)
    {
        // 主循环中不需要做任何操作，所有工作由中断处理
        // 可以在这里添加其他任务
        // 每200ms切换一次LED状态
        if(led_timer2_count >= 10)
        {
            led_timer2_count = 0;
            Toggle_LED();
        }
        if(lcd_update_count_flag >= 20)
        {
            lcd_update_count_flag = 0;
            lcd_update_count++;
            lcd_update_count_to_str(lcd_update_count, lcd_update_count_str);
            QC1602_Print8(1, 0, lcd_update_count_str);
        }

    }
}
void timer_push(void)
{
    // 嘀嗒计时器，50ms计数器增加一次
    led_timer2_count++;
    lcd_update_count_flag++;
}

// 定时器2中断服务函数
void timer2_isr(void) __interrupt (5) __using (1)
{
    // 清除中断标志（自动重载模式下不需要手动清除TF2）
    TF2 = 0;
    timer_push();
}
