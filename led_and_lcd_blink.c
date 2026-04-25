/******************************************************************************
 * STC89C52RC LED + LCD1602 计数器示例
 *
 * 硬件配置:
 *   - LED: P0^0
 *   - LCD 数据口: P2 (D0-D7)
 *   - LCD RS: P0^1, EN: P0^2
 *   - 晶振: 11.0592MHz
 *
 * 功能:
 *   - LED 每 500ms 翻转一次 (1Hz 闪烁)
 *   - LCD 第1行显示 "LCD COUNT NUM:"
 *   - LCD 第2行显示秒计数器
 ******************************************************************************/

#include <at89x52.h>

/*============================================================================
 * 宏定义
 *============================================================================*/

/* 硬件引脚定义 */
#define LED_PIN        P0_0
#define LCD_DATA_PORT  P2
#define LCD_RS_PIN     P0_1
#define LCD_EN_PIN     P0_2

/* 定时常量 (基于 50ms 定时周期) */
#define TIMER_PERIOD_MS      50        /* 定时器周期 */
#define LED_BLINK_TICKS      10        /* LED 翻转周期: 10 * 50ms = 500ms */
#define LCD_UPDATE_TICKS     20        /* LCD 更新周期: 20 * 50ms = 1000ms */

/* LCD 指令 */
#define LCD_CMD_CLEAR        0x01
#define LCD_CMD_HOME         0x02
#define LCD_CMD_8BIT_2LINE   0x38
#define LCD_CMD_DISPLAY_ON   0x0C
#define LCD_CMD_AUTO_INC     0x06
#define LCD_CMD_LINE1        0x80
#define LCD_CMD_LINE2        0xC0

/* 延时宏 */
#define NOP_DELAY()  { NOP(); NOP(); NOP(); }

#ifndef NOP
#define NOP()  __asm NOP __endasm
#endif

/*============================================================================
 * 全局变量
 *============================================================================*/

static unsigned int   g_led_timer_cnt = 0;      /* LED 计时计数器 */
static unsigned char   g_led_state = 0;         /* LED 当前状态 */
static unsigned int   g_lcd_timer_cnt = 0;     /* LCD 计时计数器 */
static unsigned long  g_lcd_counter = 0;       /* 秒计数器 */
static char            g_lcd_str_buf[12];       /* LCD 显示缓冲区 */

/*============================================================================
 * 函数声明
 *============================================================================*/

static void lcd_en_pulse(void);
static void lcd_cmd(unsigned char cmd);
static void lcd_dat(unsigned char dat);
static void lcd_init(void);
static void lcd_print(unsigned char row, unsigned char col, char *str);
static void lcd_print_num(unsigned long num, unsigned char row, unsigned char col);

void Timer2_Init(void);
void Toggle_LED(void);
void timer_tick(void);

/*============================================================================
 * LCD 驱动函数
 *============================================================================*/

/* 产生使能脉冲 */
static void lcd_en_pulse(void)
{
    LCD_EN_PIN = 1;
    NOP_DELAY();    /* 保持一段时间 */
    LCD_EN_PIN = 0;
}

/* 写命令 */
static void lcd_cmd(unsigned char cmd)
{
    LCD_RS_PIN = 0;
    LCD_DATA_PORT = cmd;
    lcd_en_pulse();

    if (cmd == LCD_CMD_CLEAR || cmd == LCD_CMD_HOME)
    {
        /* 清屏/归位需要较长延时 (>1.52ms) */
        unsigned int i = 2500;
        while (--i);
    }
    else
    {
        /* 普通命令延时 (>40us) */
        unsigned char i = 50;
        while (--i);
    }
}

/* 写数据 */
static void lcd_dat(unsigned char dat)
{
    LCD_RS_PIN = 1;
    LCD_DATA_PORT = dat;
    lcd_en_pulse();

    {
        unsigned char i = 50;
        while (--i);
    }
    LCD_RS_PIN = 0;
}

/* LCD 1602 初始化 (8位模式) */
static void lcd_init(void)
{
    unsigned char i;

    /* 上电延时 >15ms */
    for (i = 0; i < 15; i++)
    {
        unsigned int j = 11092;
        while (--j);
    }

    lcd_cmd(LCD_CMD_8BIT_2LINE);  /* 8位数据、2行显示、5x7 点阵 */
    lcd_cmd(LCD_CMD_DISPLAY_ON);  /* 显示开、光标关 */
    lcd_cmd(LCD_CMD_AUTO_INC);    /* 地址自增 */
    lcd_cmd(LCD_CMD_CLEAR);       /* 清屏 */
}

/* 在指定位置打印字符串 */
static void lcd_print(unsigned char row, unsigned char col, char *str)
{
    lcd_cmd((row == 0) ? (LCD_CMD_LINE1 + col) : (LCD_CMD_LINE2 + col));
    while (*str)
    {
        lcd_dat(*str++);
    }
}

/* 在指定位置打印数字 (无前导零) */
static void lcd_print_num(unsigned long num, unsigned char row, unsigned char col)
{
    char buf[11];
    unsigned char i = 0;
    unsigned char j;

    /* 数字转字符串 (倒序) */
    if (num == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (num > 0)
        {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
    }

    /* 设置起始位置 */
    lcd_cmd((row == 0) ? (LCD_CMD_LINE1 + col) : (LCD_CMD_LINE2 + col));

    /* 反向输出 (正序) */
    for (j = i; j > 0; j--)
    {
        lcd_dat(buf[j - 1]);
    }

    /* 清除剩余字符 */
    lcd_dat(' ');
}

/*============================================================================
 * 定时器函数
 *============================================================================*/

/* Timer2 初始化 (16位自动重载模式) */
void Timer2_Init(void)
{
    T2CON = 0x00;  /* 16位自动重载模式 */

    /* 11.0592MHz 晶振, 定时 50ms */
    TH2 = RCAP2H = 0x4C;
    TL2 = RCAP2L = 0x00;

    ET2 = 1;  /* 开启 Timer2 中断 */
    TR2 = 1;  /* 启动 Timer2 */
}

/* LED 状态翻转 */
void Toggle_LED(void)
{
    g_led_state = !g_led_state;
    LED_PIN = g_led_state;
}

/* 定时器嘀嗒处理 */
void timer_tick(void)
{
    g_led_timer_cnt++;
    g_lcd_timer_cnt++;
}

/* Timer2 中断服务函数 */
void timer2_isr(void) __interrupt (5) __using (1)
{
    TF2 = 0;  /* 清除中断标志 */
    timer_tick();
}

/*============================================================================
 * 主函数
 *============================================================================*/

void main(void)
{
    /* 初始化定时器 */
    Timer2_Init();

    /* 初始化 LCD */
    lcd_init();

    /* 开启总中断 */
    EA = 1;

    /* 显示标题 */
    lcd_print(0, 0, "LCD COUNT NUM:");

    /* 主循环 */
    while (1)
    {
        /* LED 闪烁控制: 每 500ms 翻转 */
        if (g_led_timer_cnt >= LED_BLINK_TICKS)
        {
            g_led_timer_cnt = 0;
            Toggle_LED();
        }

        /* LCD 计数器更新: 每 1 秒 */
        if (g_lcd_timer_cnt >= LCD_UPDATE_TICKS)
        {
            g_lcd_timer_cnt = 0;
            g_lcd_counter++;
            lcd_print_num(g_lcd_counter, 1, 0);
        }
    }
}
