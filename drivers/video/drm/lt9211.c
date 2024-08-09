#include <common.h>
#include <dm.h>
#include <i2c.h>
#include "linux/printk.h"
#include <power/regulator.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <dm/uclass.h>
#include "lt9211.h"
#include <backlight.h>
#define DRIVER_NAME		"lt9211"

#define RET_SUCCEED 0
#define RET_FAILED -1

struct video_timing *pVideoFormat;
//+++ hfh 20230221
//#ifndef LVDS_SSC
#define LVDS_SSC 1
//#endif

struct gpio_desc mipi_bl_en;
struct gpio_desc edp_bl_en;


#define MIPI_LANE_CNT  MIPI_4_LANE // 0: 4lane
#define MIPI_SETTLE_VALUE 0x0a //0x05  0x0a
#define LT9211_OUTPUT_PATTERN 0 //Enable it for output 9211 internal PATTERN, 1-enable

uint16_t hact, vact;
uint16_t hs, vs;
uint16_t hbp, vbp;
uint16_t htotal, vtotal;
uint16_t hfp, vfp;

//											//hfp, hs, hbp,hact,htotal,vfp, vs, vbp, vact,vtotal,pixclk_khz
struct video_timing video_640x400_60Hz     ={ 96, 64,  58, 640,   858,   87,  6,  32, 400,   525,  27000};
struct video_timing video_640x480_60Hz     ={ 8, 96,  40, 640,   800, 33,  2,  10, 480,   525,  25000};
struct video_timing video_720x480_60Hz     ={16, 62,  60, 720,   858,  9,  6,  30, 480,   525,  27000};
struct video_timing video_1024x600_60Hz   = {160, 20, 140,1024,  1344,  12,  3,  20,  600, 635  , 55000};
struct video_timing video_1280x720_60Hz    ={110,40, 220,1280,  1650,  5,  5,  20, 720,   750,  74250};
struct video_timing video_1280x720_30Hz    = {110,40, 220,1280,  1650,  5,  5,  20, 720,   750,  37125};
struct video_timing video_1366x768_60Hz    ={26, 110,110,1366,  1592,  13, 6,  13, 768,   800,  73000};  //81000
struct video_timing video_1280x1024_60Hz   ={100,100,208,1280,  1688,  5,  5,  32, 1024, 1066, 107960};
struct video_timing video_1920x1080_25Hz   = {88, 44, 148,1920,  2200,  4,  5,  36, 1080, 1125,  74250};
struct video_timing video_1920x1080_30Hz   ={88, 44, 148,1920,  2200,  4,  5,  36, 1080, 1125,  74250};
struct video_timing video_1920x1080_60Hz   ={88, 44, 148,1920,  2200,  4,  5,  36, 1080, 1125, 148500};
struct video_timing video_1920x360_60Hz    ={88, 44, 148,1920,  2200,  4,  5,  36, 360, 405, 54000};	
//struct video_timing video_1920x1080_60Hz   ={65, 10, 65,1920,  2060,  20,  5,  20, 1080, 1125, 148500};
struct video_timing video_1280x800_60Hz    ={20,20, 120,1280,  1440,  11,  1,  11, 800,   823,  71100};

struct video_timing video_3840x1080_60Hz   ={176,88, 296,3840,  4400,  4,  5,  36, 1080, 1125, 297000};
struct video_timing video_1920x1200_60Hz   ={48, 32,  80,1920,  2080,  3,  6,  26, 1200, 1235, 154000};
struct video_timing video_3840x2160_30Hz   ={176,88, 296,3840,  4400,  8,  10, 72, 2160, 2250, 297000};
struct video_timing video_3840x2160_60Hz   ={176,88, 296,3840,  4400,  8,  10, 72, 2160, 2250, 594000};
struct video_timing video_1920x720_60Hz    ={148, 44, 88,1920,  2200,  28,  5, 12,  720, 765, 88000};  
/* Logging */
#define RGA_DEBUG 1
#if RGA_DEBUG
#define DRIVER_DBG(format, args...)      printk(KERN_DEBUG "%s: " format, DRIVER_NAME, ## args)
#define DRIVER_ERR(format, args...)      printk(KERN_ERR "%s: " format, DRIVER_NAME, ## args)
#define DRIVER_WARNING(format, args...)  printk(KERN_WARN "%s: " format, DRIVER_NAME, ## args)
#define DRIVER_INFO(format, args...)     printk(KERN_INFO "%s: " format, DRIVER_NAME, ## args)
#else
#define DRIVER_DBG(format, args...)
#define DRIVER_ERR(format, args...)
#define DRIVER_WARNING(format, args...)
#define DRIVER_INFO(format, args...)
#endif

struct lt9211_info {
	struct udevice *dev;
	unsigned int vsel_min;
	unsigned int vsel_step;
	struct gpio_desc reset_gpio;
	struct gpio_desc ledpwd_gpio;   //control lvds backlight
	unsigned int sleep_vsel_id;
};
static struct lt9211_info  lt9211_instance;


static int lt9211_i2c_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	int ret;

	ret = dm_i2c_read(dev, reg, buff, len);
	if (ret) {
		dev_err(dev, "read reg[0x%02x] failed, ret=%d\n", reg, ret);
		return RET_FAILED;
	}

	return RET_SUCCEED;
}

static u8 lt9211_i2c_reg_read(uint reg)
{
	u8 val = 0;
	int ret;
	ret = lt9211_i2c_read(lt9211_instance.dev, reg, &val, 1);
	if (ret == RET_SUCCEED) {
		return val;
	}
	DRIVER_ERR("lt9211_i2c_reg_read reg[0x%02x] failed \n", reg);
	return 0;
}

static int lt9211_i2c_write(struct udevice *dev, uint reg, const uint8_t *buff,
			int len)
{
	int ret;

	ret = dm_i2c_write(dev, reg, buff, len);
	if (ret) {
		DRIVER_INFO("write reg[0x%02x] failed, ret=%d\n", reg, ret);
		return ret;
	}

	return 0;
}
static int lt9211_i2c_reg_write(uint reg, uint value)
{
	u8 byte = value;
	int ret;
	//DRIVER_INFO("%s: reg=%x, value=%x \n", __func__, reg, value);
	ret = lt9211_i2c_write(lt9211_instance.dev, reg, &byte, 1);

	return ret;
}

static int lt9211_reset(void)
{
	int ret;
	ret = dm_gpio_is_valid(&lt9211_instance.reset_gpio);
	if(ret != 1)
		return -1;

	dm_gpio_set_value(&lt9211_instance.reset_gpio, 0);
	//DRIVER_INFO( "lyh----dm_gpio_set_value-----ret = %d \n",ret);
	mdelay(200);
	dm_gpio_set_value(&lt9211_instance.reset_gpio, 1);
	//DRIVER_INFO( "lyh----dm_gpio_set_value-----ret = %d \n",ret);
	//mdelay(0);

	return ret;
}

static int LT9211_ledpwd(void)
{
	int ret;
	ret = dm_gpio_is_valid(&lt9211_instance.ledpwd_gpio);
	if(ret != 1)
		return -1;
	dm_gpio_set_value(&lt9211_instance.ledpwd_gpio, 0);
	//DRIVER_INFO( "lyh----dm_gpio_set_value-----ret = %d \n",ret);
	mdelay(50);
	dm_gpio_set_value(&lt9211_instance.ledpwd_gpio, 1);
	//DRIVER_INFO( "lyh----dm_gpio_set_value-----ret = %d \n",ret);
	//mdelay(100);
	if(dm_gpio_is_valid(&mipi_bl_en) == 1)
		dm_gpio_set_value(&mipi_bl_en, 1);

	if(dm_gpio_is_valid(&edp_bl_en) == 1)
		dm_gpio_set_value(&edp_bl_en, 1);

	return ret;
}



static int LT9211_ChipID(void)
{
	u8 reg_value ;
	lt9211_i2c_reg_write(0xff,0x81);//register bank
	reg_value = lt9211_i2c_reg_read(0x00);
	//DRIVER_INFO("%02x ",lt9211_i2c_reg_read(0x01));
	//DRIVER_INFO("%02x ",lt9211_i2c_reg_read(0x02));
	if (lt9211_i2c_reg_read(0x00) == 0)
		return RET_FAILED;
	else{
		DRIVER_INFO("Chip ID:%02x \n",reg_value);
	}
	return RET_SUCCEED;
}

static void LT9211_SystemInt(void)
{
	  /* system clock init */	
	  lt9211_i2c_reg_write(0xff,0x82);
	  lt9211_i2c_reg_write(0x01,0x18);
	  lt9211_i2c_reg_write(0xff,0x86);
	  lt9211_i2c_reg_write(0x06,0x61); 	
	  lt9211_i2c_reg_write(0x07,0xa8); //fm for sys_clk
	  
	  lt9211_i2c_reg_write(0xff,0x87); //
	  lt9211_i2c_reg_write(0x14,0x08); //default value
	  lt9211_i2c_reg_write(0x15,0x00); //default value
	  lt9211_i2c_reg_write(0x18,0x0f);
	  lt9211_i2c_reg_write(0x22,0x08); //default value
	  lt9211_i2c_reg_write(0x23,0x00); //default value
	  lt9211_i2c_reg_write(0x26,0x0f); 
}

static void LT9211_MipiRxPhy(void)
{
#if 1//yelsin for WYZN CUSTOMER 2Lane 9211 mipi2RGB
		lt9211_i2c_reg_write(0xff,0xd0);
		lt9211_i2c_reg_write(0x00,MIPI_LANE_CNT);	// 0: 4 Lane / 1: 1 Lane / 2 : 2 Lane / 3: 3 Lane
#endif

	/* Mipi rx phy */
		lt9211_i2c_reg_write(0xff,0x82);
		lt9211_i2c_reg_write(0x02,0x44); //port A mipi rx enable
	/*port A/B input 8205/0a bit6_4:EQ current setting*/
	    lt9211_i2c_reg_write(0x05,0x36); //port A CK lane swap  0x32--0x36 for WYZN Glassbit2- Port A mipi/lvds rx s2p input clk select: 1 = From outer path.
        lt9211_i2c_reg_write(0x0d,0x26); //bit6_4:Port B Mipi/lvds rx abs refer current  0x26 0x76
		lt9211_i2c_reg_write(0x17,0x0c);
		lt9211_i2c_reg_write(0x1d,0x0c);
	
		lt9211_i2c_reg_write(0x0a,0x81);//eq control for LIEXIN  horizon line display issue 0xf7->0x80
		lt9211_i2c_reg_write(0x0b,0x00);//eq control  0x77->0x00
#ifdef  _Mipi_PortA_ 
	    /*port a*/
	    lt9211_i2c_reg_write(0x07,0x9f); //port clk enable  
	    lt9211_i2c_reg_write(0x08,0xfc); //port lprx enable
#endif  
#ifdef _Mipi_PortB_	
	    /*port a*/
		lt9211_i2c_reg_write(0x07,0x9f); //port clk enable  
		lt9211_i2c_reg_write(0x08,0xfc); //port lprx enable	
		/*port b*/
		lt9211_i2c_reg_write(0x0f,0x9F); //port clk enable
		lt9211_i2c_reg_write(0x10,0xfc); //port lprx enable
		lt9211_i2c_reg_write(0x04,0xa1);
#endif
/*port diff swap*/
		lt9211_i2c_reg_write(0x09,0x01); //port a diff swap
		lt9211_i2c_reg_write(0x11,0x01); //port b diff swap	

		/*port lane swap*/
		lt9211_i2c_reg_write(0xff,0x86);		
		lt9211_i2c_reg_write(0x33,0x1b); //port a lane swap	1b:no swap	
		lt9211_i2c_reg_write(0x34,0x1b); //port b lane swap 1b:no swap
#ifdef CSI_INPUTDEBUG
		lt9211_i2c_reg_write(0xff,0xd0);
		lt9211_i2c_reg_write(0x04,0x10);	//bit4-enable CSI mode
		lt9211_i2c_reg_write(0x21,0xc6);		
#endif 		
}

static void LT9211_MipiRxDigital(void)
{	   
    	lt9211_i2c_reg_write(0xff,0x86);
#ifdef _Mipi_PortA_ 	
    	lt9211_i2c_reg_write(0x30,0x85); //mipirx HL swap	 	
#endif

#ifdef _Mipi_PortB_	
   		lt9211_i2c_reg_write(0x30,0x8f); //mipirx HL swap
#endif
		lt9211_i2c_reg_write(0xff,0xD8);
#ifdef _Mipi_PortA_ 	
		lt9211_i2c_reg_write(0x16,0x00); //mipirx HL swap	  bit7- 0:portAinput	
#endif

#ifdef _Mipi_PortB_	
		lt9211_i2c_reg_write(0x16,0x80); //mipirx HL swap bit7- portBinput
#endif	
	
		lt9211_i2c_reg_write(0xff,0xd0);	
		lt9211_i2c_reg_write(0x43,0x12); //rpta mode enable,ensure da_mlrx_lptx_en=0

		lt9211_i2c_reg_write(0x02,MIPI_SETTLE_VALUE); //mipi rx controller	
#if 0 //forWTWD yelsin0520	
		lt9211_i2c_reg_write(0x0c,0x40);		//MIPI read delay setting
		lt9211_i2c_reg_write(0x1B,0x00);	//PCR de mode delay.[15:8]
		lt9211_i2c_reg_write(0x1c,0x40);	//PCR de mode delay.[7:0]
		lt9211_i2c_reg_write(0x24,0x70);	
		lt9211_i2c_reg_write(0x25,0x40);
 #endif			  
}

static void LT9211_SetVideoTiming(struct video_timing *video_format)
{
	mdelay(50);
	lt9211_i2c_reg_write(0xff,0xd0);
	lt9211_i2c_reg_write(0x0d,(u8)(video_format->vtotal>>8)); //vtotal[15:8]
	lt9211_i2c_reg_write(0x0e,(u8)(video_format->vtotal)); //vtotal[7:0]
	lt9211_i2c_reg_write(0x0f,(u8)(video_format->vact>>8)); //vactive[15:8]
	lt9211_i2c_reg_write(0x10,(u8)(video_format->vact)); //vactive[7:0]
	lt9211_i2c_reg_write(0x15,(u8)(video_format->vs)); //vs[7:0]
	lt9211_i2c_reg_write(0x17,(u8)(video_format->vfp>>8)); //vfp[15:8]
	lt9211_i2c_reg_write(0x18,(u8)(video_format->vfp)); //vfp[7:0]	

	lt9211_i2c_reg_write(0x11,(u8)(video_format->htotal>>8)); //htotal[15:8]
	lt9211_i2c_reg_write(0x12,(u8)(video_format->htotal)); //htotal[7:0]
	lt9211_i2c_reg_write(0x13,(u8)(video_format->hact>>8)); //hactive[15:8]
	lt9211_i2c_reg_write(0x14,(u8)(video_format->hact)); //hactive[7:0]
	lt9211_i2c_reg_write(0x16,(u8)(video_format->hs)); //hs[7:0]
	lt9211_i2c_reg_write(0x19,(u8)(video_format->hfp>>8)); //hfp[15:8]
	lt9211_i2c_reg_write(0x1a,(u8)(video_format->hfp)); //hfp[7:0]	
}

static void LT9211_DesscPll_mipi(struct video_timing *video_format)
{
	u32 pclk;
	u8 pll_lock_flag;
	u8 i;
	u8 pll_post_div;
	u8 pcr_m;
	pclk = video_format->pclk_khz; 
	//printk("\r\n LT9211_DesscPll: set rx pll = %ud", pclk);

	lt9211_i2c_reg_write(0xff,0x82);
	lt9211_i2c_reg_write(0x2d,0x48); 

	if(pclk > 80000){
	   	lt9211_i2c_reg_write(0x35,0x81);
		pll_post_div = 0x01;
	}
	else if(pclk > 20000){
		lt9211_i2c_reg_write(0x35,0x82);
		pll_post_div = 0x02;
	}
	else{
		lt9211_i2c_reg_write(0x35,0x83); 
		pll_post_div = 0x04;
	}

	pcr_m = (u8)((pclk*4*pll_post_div)/25000);
	//printk("\r\n LT9211_DesscPll: set rx pll pcr_m = 0x%cx \n", pcr_m);
	
	lt9211_i2c_reg_write(0xff,0xd0); 	
	lt9211_i2c_reg_write(0x2d,0x40); //M_up_limit
	lt9211_i2c_reg_write(0x31,0x10); //M_low_limit
	lt9211_i2c_reg_write(0x26,pcr_m|0x80);

	lt9211_i2c_reg_write(0xff,0x81); //dessc pll sw rst
	lt9211_i2c_reg_write(0x20,0xef);
	lt9211_i2c_reg_write(0x20,0xff);

	/* pll lock status */
	for(i = 0; i < 6 ; i++)
	{   
		lt9211_i2c_reg_write(0xff,0x81);	
		lt9211_i2c_reg_write(0x11,0xfb); /* pll lock logic reset */
		lt9211_i2c_reg_write(0x11,0xff);
		lt9211_i2c_reg_write(0xff,0x87);

		pll_lock_flag = lt9211_i2c_reg_read(0x04);
		if(pll_lock_flag & 0x01){
			//printk("\r\n LT9211_DesscPll: dessc pll locked \n");
			break;
		}
		else{
			lt9211_i2c_reg_write(0xff,0x81); //dessc pll sw rst
			lt9211_i2c_reg_write(0x20,0xef);
			lt9211_i2c_reg_write(0x20,0xff);
			//printk("\r\n LT9211_DesscPll: dessc pll unlocked,sw reset \n");
		}
	}
}
#if 0
#endif	
static void LT9211_TimingSet(void)
{
  	uint16_t hact ;	
	uint16_t vact ;	
	u8 fmt =0 ;		
	u8 pa_lpn = 0;
	mdelay(500);//500-->100
	lt9211_i2c_reg_write(0xff,0xd0);

	hact = (lt9211_i2c_reg_read(0x82)<<8) + lt9211_i2c_reg_read(0x83) ;
	hact = hact/3;
	fmt = (lt9211_i2c_reg_read(0x84) &0x0f);
	vact = (lt9211_i2c_reg_read(0x85)<<8) +lt9211_i2c_reg_read(0x86);
	pa_lpn = lt9211_i2c_reg_read(0x9c);	
	printk("hact = %d\r\n", hact);
	printk("vact = %d\r\n", vact);
	printk("fmt = %x\r\n", fmt);
	printk("pa_lpn = %x\r\n", pa_lpn);	

	mdelay(100);
 	if ((hact == video_1280x720_60Hz.hact ) &&( vact == video_1280x720_60Hz.vact )){
		pVideoFormat = &video_1280x720_60Hz;
	}else if ((hact == video_640x400_60Hz.hact ) &&( vact == video_640x400_60Hz.vact )){
	    pVideoFormat = &video_640x400_60Hz;
    }else if ((hact == video_1366x768_60Hz.hact ) &&( vact == video_1366x768_60Hz.vact )){
		pVideoFormat = &video_1366x768_60Hz;
    }else if ((hact == video_1280x1024_60Hz.hact ) &&( vact == video_1280x1024_60Hz.vact )){
		pVideoFormat = &video_1280x1024_60Hz;
    }else if ((hact == video_1920x1080_60Hz.hact ) &&( vact == video_1920x1080_60Hz.vact )){
		pVideoFormat = &video_1920x1080_60Hz;
    }else if ((hact == video_1920x1200_60Hz.hact ) &&( vact == video_1920x1200_60Hz.vact )){
		pVideoFormat = &video_1920x1200_60Hz;
    }else if ((hact == video_1920x360_60Hz.hact ) &&( vact == video_1920x360_60Hz.vact )){
		pVideoFormat = &video_1920x360_60Hz; 
	}else if ((hact == video_1280x800_60Hz.hact ) &&( vact == video_1280x800_60Hz.vact )){
		pVideoFormat = &video_1280x800_60Hz; 
	}else {      
		pVideoFormat = 0;//&video_1920x1080_60Hz;
		printk("\r\nvideo_none");
    }
	LT9211_SetVideoTiming(pVideoFormat);
	LT9211_DesscPll_mipi(pVideoFormat);
}

static void LT9211_MipiPcr(void)
{	
	u8 loopx;
	u8 pcr_m;
	
    lt9211_i2c_reg_write(0xff,0xd0); 	
    lt9211_i2c_reg_write(0x0c,0x60);  //fifo position
	lt9211_i2c_reg_write(0x1c,0x60);  //fifo position
	lt9211_i2c_reg_write(0x24,0x70);  //pcr mode( de hs vs)
			
	lt9211_i2c_reg_write(0x2d,0x30); //M up limit
	lt9211_i2c_reg_write(0x31,0x02); //M down limit

	/*stage1 hs mode*/
	lt9211_i2c_reg_write(0x25,0xf0);  //line limit
	lt9211_i2c_reg_write(0x2a,0x30);  //step in limit
	lt9211_i2c_reg_write(0x21,0x4f);  //hs_step
	lt9211_i2c_reg_write(0x22,0x00); 

	/*stage2 hs mode*/
	lt9211_i2c_reg_write(0x1e,0x01);  //RGD_DIFF_SND[7:4],RGD_DIFF_FST[3:0]
	lt9211_i2c_reg_write(0x23,0x80);  //hs_step
    /*stage2 de mode*/
	lt9211_i2c_reg_write(0x0a,0x02); //de adjust pre line
	lt9211_i2c_reg_write(0x38,0x02); //de_threshold 1
	lt9211_i2c_reg_write(0x39,0x04); //de_threshold 2
	lt9211_i2c_reg_write(0x3a,0x08); //de_threshold 3
	lt9211_i2c_reg_write(0x3b,0x10); //de_threshold 4
		
	lt9211_i2c_reg_write(0x3f,0x04); //de_step 1
	lt9211_i2c_reg_write(0x40,0x08); //de_step 2
	lt9211_i2c_reg_write(0x41,0x10); //de_step 3
	lt9211_i2c_reg_write(0x42,0x20); //de_step 4

	lt9211_i2c_reg_write(0x2b,0xa0); //stable out
	//msleep(100);
    lt9211_i2c_reg_write(0xff,0xd0);   //enable HW pcr_m
	pcr_m = lt9211_i2c_reg_read(0x26);
	pcr_m &= 0x7f;
	lt9211_i2c_reg_write(0x26,pcr_m);
	lt9211_i2c_reg_write(0x27,0x0f);

	lt9211_i2c_reg_write(0xff,0x81);  //pcr reset
	lt9211_i2c_reg_write(0x20,0xbf); // mipi portB div issue
	lt9211_i2c_reg_write(0x20,0xff);
	mdelay(5);
	lt9211_i2c_reg_write(0x0B,0x6F);
	lt9211_i2c_reg_write(0x0B,0xFF);
	mdelay(500);//800->120
	{
		for(loopx = 0; loopx < 10; loopx++) //Check pcr_stable 10
		{
			mdelay(100);
			lt9211_i2c_reg_write(0xff,0xd0);
			if(lt9211_i2c_reg_read(0x87)&0x08){
				printk("LT9211 pcr stable\n");
				break;
			}
			else{
				printk("\nLT9211 pcr unstable!!!!\n");
			}
		}
	}
	lt9211_i2c_reg_write(0xff,0xd0);
	//printk("LT9211 pcr_stable_M=%x \n",(lt9211_i2c_reg_read(0x94)&0x7F));
}

static void LT9211_TxDigital(void)
{
    //printk("\r\nLT9211 OUTPUT_MODE: \n");
	if( LT9211_OutPutModde == OUTPUT_RGB888 ){
		printk("\rLT9211 set to OUTPUT_RGB888\n");
    	lt9211_i2c_reg_write(0xff,0x85);
    	lt9211_i2c_reg_write(0x88,0x50);
		lt9211_i2c_reg_write(0x60,0x00);
		lt9211_i2c_reg_write(0x6d,0x07);//0x07
		lt9211_i2c_reg_write(0x6E,0x00);
		lt9211_i2c_reg_write(0xff,0x81);
		lt9211_i2c_reg_write(0x36,0xc0); //bit7:ttltx_pixclk_en;bit6:ttltx_BT_clk_en
	}else if( LT9211_OutPutModde == OUTPUT_BT656_8BIT ){
		printk("\rLT9211 set to OUTPUT_BT656_8BIT");
	 	lt9211_i2c_reg_write(0xff,0x85);
    lt9211_i2c_reg_write(0x88,0x40);
        lt9211_i2c_reg_write(0x60,0x34); // bit5_4 BT TX module hsync/vs polarity bit2-Output 8bit BT data enable
        lt9211_i2c_reg_write(0x6d,0x00);//0x08 YC SWAP BIT[2:0]=000-outputBGR
		lt9211_i2c_reg_write(0x6e,0x07);//0x07-low 16BIT ; 0x06-High 8
		
		lt9211_i2c_reg_write(0xff,0x81);
		lt9211_i2c_reg_write(0x0d,0xfd);
		lt9211_i2c_reg_write(0x0d,0xff);
		lt9211_i2c_reg_write(0xff,0x81);
		lt9211_i2c_reg_write(0x36,0xc0); //bit7:ttltx_pixclk_en;bit6:ttltx_BT_clk_en
	 }else if( LT9211_OutPutModde == OUTPUT_BT1120_16BIT ){ 
    	printk("\rLT9211 set to OUTPUT_BT1120_16BIT");		
		lt9211_i2c_reg_write(0xff,0x85);
    	lt9211_i2c_reg_write(0x88,0x40);
		lt9211_i2c_reg_write(0x60,0x33); //output 16 bit BT1120
		lt9211_i2c_reg_write(0x6d,0x08);//0x08 YC SWAP
		lt9211_i2c_reg_write(0x6e,0x06);//HIGH 16BIT   0x07-BT low 16bit ;0x06-High 16bit BT
		
		lt9211_i2c_reg_write(0xff,0x81);
		lt9211_i2c_reg_write(0x0d,0xfd);
		lt9211_i2c_reg_write(0x0d,0xff);		
	}else if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde == OUTPUT_LVDS_1_PORT) ) {
		printk("LT9211 set to OUTPUT_LVDS\n");
		lt9211_i2c_reg_write(0xff,0x85); /* lvds tx controller */
		lt9211_i2c_reg_write(0x59,0x50); 	//bit4-LVDSTX Display color depth set 1-8bit, 0-6bit; 
		lt9211_i2c_reg_write(0x5a,0xaa); 
		lt9211_i2c_reg_write(0x5b,0xaa);
		if( LT9211_OutPutModde == OUTPUT_LVDS_2_PORT ){
			lt9211_i2c_reg_write(0x5c,0x01);	//lvdstx port sel 01:dual;00:single
		}else{
			lt9211_i2c_reg_write(0x5c,0x00);
		}
		lt9211_i2c_reg_write(0x88,0x50);	
		lt9211_i2c_reg_write(0xa1,0x77); 	
		lt9211_i2c_reg_write(0xff,0x86);	
		lt9211_i2c_reg_write(0x40,0x40); //tx_src_sel
		/*port src sel*/
		lt9211_i2c_reg_write(0x41,0x34);	
		lt9211_i2c_reg_write(0x42,0x10);
		lt9211_i2c_reg_write(0x43,0x23); //pt0_tx_src_sel
		lt9211_i2c_reg_write(0x44,0x41);
		lt9211_i2c_reg_write(0x45,0x02); //pt1_tx_src_scl
		
#if 1//PORT_OUT_SWAP
		//printk("\r\nSAWP reg8646 = 0x%x\n",lt9211_i2c_reg_read(0x46));
		//lt9211_i2c_reg_write(0x46,0x40); //pt0/1_tx_src_scl yelsin
#endif 		

#ifdef lvds_format_JEIDA
    lt9211_i2c_reg_write(0xff,0x85);
		lt9211_i2c_reg_write(0x59,0xd0); 	
		lt9211_i2c_reg_write(0xff,0xd8);
		lt9211_i2c_reg_write(0x11,0x40);
#endif	
	}  		
}

static void LT9211_TxPhy(void)
{		
	
	lt9211_i2c_reg_write(0xff,0x82);
	if( (LT9211_OutPutModde == OUTPUT_RGB888) || (LT9211_OutPutModde ==OUTPUT_BT656_8BIT) || (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) )
	{
		lt9211_i2c_reg_write(0x62,0x01); //ttl output enable
		lt9211_i2c_reg_write(0x6b,0xff);
	}else if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde ==OUTPUT_LVDS_1_PORT) ){
		/* dual-port lvds tx phy */	
		lt9211_i2c_reg_write(0x62,0x00); //ttl output disable
		if(LT9211_OutPutModde == OUTPUT_LVDS_2_PORT){
			lt9211_i2c_reg_write(0x3b,0xb8);
		}else{
			lt9211_i2c_reg_write(0x3b,0x38);
		}
		// lt9211_i2c_reg_write(0x3b,0xb8); //dual port lvds enable	
		lt9211_i2c_reg_write(0x3e,0x92); 
		lt9211_i2c_reg_write(0x3f,0x48); 	
		lt9211_i2c_reg_write(0x40,0x31); 		
		lt9211_i2c_reg_write(0x43,0x80); 		
		lt9211_i2c_reg_write(0x44,0x00);
		lt9211_i2c_reg_write(0x45,0x00);//0x45 for swing
		lt9211_i2c_reg_write(0x49,0x00);
		lt9211_i2c_reg_write(0x4a,0x01);
		lt9211_i2c_reg_write(0x4e,0x00);		
		lt9211_i2c_reg_write(0x4f,0x00);
		lt9211_i2c_reg_write(0x50,0x00);
		lt9211_i2c_reg_write(0x53,0x00);
		lt9211_i2c_reg_write(0x54,0x01);
		lt9211_i2c_reg_write(0xff,0x81);
		lt9211_i2c_reg_write(0x20,0x7b); 
		lt9211_i2c_reg_write(0x20,0xff); //mlrx mltx calib reset
	}
}

static void LT9211_Txpll(void)
{
	u8 loopx;
    if( LT9211_OutPutModde == OUTPUT_BT656_8BIT ){
		lt9211_i2c_reg_write(0xff,0x82);
		lt9211_i2c_reg_write(0x2d,0x40);
		lt9211_i2c_reg_write(0x30,0x50);
		lt9211_i2c_reg_write(0x33,0x55);
	 }else if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || \
	    (LT9211_OutPutModde == OUTPUT_LVDS_1_PORT) || (LT9211_OutPutModde == OUTPUT_RGB888)|| \
	    (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) ){
			lt9211_i2c_reg_write(0xff,0x82);
			lt9211_i2c_reg_write(0x36,0x01); //b7:txpll_pd

		 	if( LT9211_OutPutModde == OUTPUT_LVDS_1_PORT ){
			    lt9211_i2c_reg_write(0x37,0x29);
		    }else{
	  			lt9211_i2c_reg_write(0x37,0x2a);
		 	}
	/* ---hfh 20230221 begin */	 
#if 0
	    lt9211_i2c_reg_write(0x38,0x06);
		lt9211_i2c_reg_write(0x39,0x30);
		lt9211_i2c_reg_write(0x3a,0x8e);
		lt9211_i2c_reg_write(0xff,0x87);
		lt9211_i2c_reg_write(0x37,0x14);
		lt9211_i2c_reg_write(0x13,0x00);
		lt9211_i2c_reg_write(0x13,0x80);	 
#endif
	/* ---hfh 20230221 end */	
#if !LVDS_SSC		 //Not Open SSC ����չƵ 
		lt9211_i2c_reg_write(0x38,0x06);
		lt9211_i2c_reg_write(0x39,0x30);
		lt9211_i2c_reg_write(0x3a,0x8e);
		lt9211_i2c_reg_write(0xff,0x87);
		lt9211_i2c_reg_write(0x37,0x14);
		lt9211_i2c_reg_write(0x13,0x00);
		lt9211_i2c_reg_write(0x13,0x80);
#endif
	
	 #if LVDS_SSC	//Open SSC ��չƵ
		lt9211_i2c_reg_write(0x38,0x06);
		lt9211_i2c_reg_write(0x39,0x30);
		lt9211_i2c_reg_write(0x3a,0x0e);
		
		lt9211_i2c_reg_write(0xff,0x87);
		lt9211_i2c_reg_write(0x37,0x0e);//14  0x0e
		lt9211_i2c_reg_write(0x13,0x00);
		lt9211_i2c_reg_write(0x13,0x80);
		
		lt9211_i2c_reg_write(0x2f,0x06);  //0x06:no ssc; 0x16:ssc_en: 
		
		lt9211_i2c_reg_write(0x30,0x03);  //prd[13:8]
		lt9211_i2c_reg_write(0x31,0x41);  //prd[7:0]
		
		lt9211_i2c_reg_write(0x32,0x00);  //delta1[13:8]
		lt9211_i2c_reg_write(0x33,0x0E);  //delta1[7:0]  0x0e:1% , 0x1c:2%, 0x2a:3% , 0x38:4% , 0x46:5%
		
		lt9211_i2c_reg_write(0x34,0x00);  //delta[13:8]
		lt9211_i2c_reg_write(0x35,0x1C);  //delta[7:0]  0x1c: 1% , 0x38:2% , 0x54: 3% , 0x71 : 4% , 0x8D:5%
	#endif	 
	
		mdelay(100);
		for(loopx = 0; loopx < 10; loopx++) //Check Tx PLL cal
		{
			lt9211_i2c_reg_write(0xff,0x87);			
				if(lt9211_i2c_reg_read(0x1f)& 0x80){
					if(lt9211_i2c_reg_read(0x20)& 0x80){
						//printk("\r\nLT9211 tx pll lock");
					}else{
						//printk("\r\nLT9211 tx pll unlocked");
					}					
					//printk("\r\nLT9211 tx pll cal done");
					break;
				}else{
					//printk("\r\nLT9211 tx pll unlocked");
				}
			}
  } 
		printk("lt9211 system success \n");	
#if !LVDS_SSC
		lt9211_i2c_reg_write(0xff,0x82);
		lt9211_i2c_reg_write(0x3a,0x0e);
		lt9211_i2c_reg_write(0xff,0x87);     
		lt9211_i2c_reg_write(0x2f,0x16); //0x06:no ssc; 0x16:ssc_en: 
        //printk("\r\n LT9211 add SSC by hfh 20230223 LT2911R_LvdsTxpll: SSC Enabled!!!");
 #endif
}


static void LT9211_RXCSC(void)
{ 
	     lt9211_i2c_reg_write(0xff,0xf9);
  if( LT9211_OutPutModde == OUTPUT_RGB888 )
	{
		if( Video_Input_Mode == INPUT_RGB888 ){
			 lt9211_i2c_reg_write(0x86,0x00);
			 lt9211_i2c_reg_write(0x87,0x00);
		}else if ( Video_Input_Mode == INPUT_YCbCr444 ){
			 lt9211_i2c_reg_write(0x86,0x0f);
			 lt9211_i2c_reg_write(0x87,0x00);
		}else if ( Video_Input_Mode == INPUT_YCbCr422_16BIT ){
			 lt9211_i2c_reg_write(0x86,0x00);
			 lt9211_i2c_reg_write(0x87,0x03);				
		}
	}else if( (LT9211_OutPutModde == OUTPUT_BT656_8BIT) || (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) || (LT9211_OutPutModde ==OUTPUT_YCbCr422_16BIT) )
	{  
		 if( Video_Input_Mode == INPUT_RGB888 ){
			 lt9211_i2c_reg_write(0x86,0x0f);
			 lt9211_i2c_reg_write(0x87,0x30);
		 }else if ( Video_Input_Mode == INPUT_YCbCr444 ){
			 lt9211_i2c_reg_write(0x86,0x00);
			 lt9211_i2c_reg_write(0x87,0x30);
		 }else if ( Video_Input_Mode == INPUT_YCbCr422_16BIT ){
			 lt9211_i2c_reg_write(0x86,0x00);
			 lt9211_i2c_reg_write(0x87,0x00);				
		}	 
	}else if( LT9211_OutPutModde == OUTPUT_YCbCr444 ){
		
		if( Video_Input_Mode == INPUT_RGB888 ){
			 lt9211_i2c_reg_write(0x86,0x0f);
			 lt9211_i2c_reg_write(0x87,0x00);
		 }else if ( Video_Input_Mode == INPUT_YCbCr444 ){
			 lt9211_i2c_reg_write(0x86,0x00);
			 lt9211_i2c_reg_write(0x87,0x00);
		 }else if ( Video_Input_Mode == INPUT_YCbCr422_16BIT ){
			 lt9211_i2c_reg_write(0x86,0x00);
			 lt9211_i2c_reg_write(0x87,0x03);				
		}
	}
}

static void LT9211_ClockCheckDebug(void)
{
#ifdef _uart_debug_

	u32 fm_value;

	lt9211_i2c_reg_write(0xff,0x86);
	lt9211_i2c_reg_write(0x00,0x0a);
	msleep(300);
    fm_value = 0;
	fm_value = (lt9211_i2c_reg_read(0x08) &(0x0f));
    fm_value = (fm_value<<8) ;
	fm_value = fm_value + lt9211_i2c_reg_read(0x09);
	fm_value = (fm_value<<8) ;
	fm_value = fm_value + lt9211_i2c_reg_read(0x0a);
	printk("\r\ndessc pixel clock: ");
	printdec_u32(fm_value);
#endif
}

static void LT9211_VideoCheckDebug(void)
{
#ifdef _uart_debug_
	u8 sync_polarity;

	lt9211_i2c_reg_write(0xff,0x86);
	sync_polarity = lt9211_i2c_reg_read(0x70);
	vs = lt9211_i2c_reg_read(0x71);

	hs = lt9211_i2c_reg_read(0x72);
    hs = (hs<<8) + lt9211_i2c_reg_read(0x73);
	
	vbp = lt9211_i2c_reg_read(0x74);
    vfp = lt9211_i2c_reg_read(0x75);

	hbp = lt9211_i2c_reg_read(0x76);
	hbp = (hbp<<8) + lt9211_i2c_reg_read(0x77);

	hfp = lt9211_i2c_reg_read(0x78);
	hfp = (hfp<<8) + lt9211_i2c_reg_read(0x79);

	vtotal = lt9211_i2c_reg_read(0x7A);
	vtotal = (vtotal<<8) + lt9211_i2c_reg_read(0x7B);

	htotal = lt9211_i2c_reg_read(0x7C);
	htotal = (htotal<<8) + lt9211_i2c_reg_read(0x7D);

	vact = lt9211_i2c_reg_read(0x7E);
	vact = (vact<<8)+ lt9211_i2c_reg_read(0x7F);

	hact = lt9211_i2c_reg_read(0x80);
	hact = (hact<<8) + lt9211_i2c_reg_read(0x81);

	printk("\r\nsync_polarity = %x", sync_polarity);

  printk("\r\nhfp, hs, hbp, hact, htotal = ");
	printdec_u32(hfp);
	printdec_u32(hs);
	printdec_u32(hbp);
	printdec_u32(hact);
	printdec_u32(htotal);

	printk("\r\nvfp, vs, vbp, vact, vtotal = ");
	printdec_u32(vfp);
	printdec_u32(vs);
	printdec_u32(vbp);
	printdec_u32(vact);
	printdec_u32(vtotal);
#endif
}


static void LT9211_BT_Set(void)
{
	 uint16_t tmp_data;
	if( (LT9211_OutPutModde == OUTPUT_BT1120_16BIT) || (LT9211_OutPutModde == OUTPUT_BT656_8BIT) )
	{
		tmp_data = hs+hbp;
		lt9211_i2c_reg_write(0xff,0x85);
		lt9211_i2c_reg_write(0x61,(u8)(tmp_data>>8));
		lt9211_i2c_reg_write(0x62,(u8)tmp_data);
		lt9211_i2c_reg_write(0x63,(u8)(hact>>8));
		lt9211_i2c_reg_write(0x64,(u8)hact);
		lt9211_i2c_reg_write(0x65,(u8)(htotal>>8));
		lt9211_i2c_reg_write(0x66,(u8)htotal);		
		tmp_data = vs+vbp;
		lt9211_i2c_reg_write(0x67,(u8)tmp_data);
		lt9211_i2c_reg_write(0x68,0x00);
		lt9211_i2c_reg_write(0x69,(u8)(vact>>8));
		lt9211_i2c_reg_write(0x6a,(u8)vact);
		lt9211_i2c_reg_write(0x6b,(u8)(vtotal>>8));
		lt9211_i2c_reg_write(0x6c,(u8)vtotal);		
	}
}

static void LT9211_Config(void)
{
	LT9211_SystemInt();
#if !LT9211_OUTPUT_PATTERN
	LT9211_MipiRxPhy();
	LT9211_MipiRxDigital();
	LT9211_TimingSet();
 	LT9211_MipiPcr();
#endif
	LT9211_TxDigital();
	LT9211_TxPhy();
#if LT9211_OUTPUT_PATTERN
	LT9211_Pattern(&video_1920x1080_60Hz);
#endif
  if (0)//yelsin	if( LT9211_OutPutModde == OUTPUT_RGB888 )
	 {
    	//LT9211_InvertRGB_HSVSPoarity();//only for TTL output	 
	 }	
	mdelay(10);
	LT9211_Txpll();
	LT9211_RXCSC();
	LT9211_ClockCheckDebug();
	LT9211_VideoCheckDebug();
  	LT9211_BT_Set();

}
static int lt9211_probe(struct udevice *dev)
{
	int ret = 0;

	lt9211_instance.dev = dev;

	return ret;
}

static int lt9211_ofdata_to_platdata(struct udevice *dev)
{
	return 0;
}


int lt9211_init(void)
{
	struct udevice *dev = lt9211_instance.dev;
	
	int ret;
	ret = gpio_request_by_name(dev, "reset_gpio", 0,&(lt9211_instance.reset_gpio), GPIOD_IS_OUT);
	if (ret) {
		DRIVER_ERR("Cannot get reset GPIO: %d\n", ret);
		return ret;
	}
	if (dm_gpio_is_valid(&lt9211_instance.reset_gpio) != 1){
		DRIVER_ERR( "reset_gpio is invalid \n");
		return -1;
	}
		
	ret = gpio_request_by_name(dev, "ledpwd-gpios", 0,&lt9211_instance.ledpwd_gpio, GPIOD_IS_OUT);
	if (ret) {
		DRIVER_ERR("Cannot get ledpwd GPIO: %d\n", ret);
		return ret;
	}

	if (dm_gpio_is_valid(&lt9211_instance.ledpwd_gpio) != 1){
		DRIVER_ERR( "ledpwd_gpio is invalid \n");
		return -1;
	}

	lt9211_reset();

	if (LT9211_ChipID() == RET_FAILED){
		DRIVER_ERR("Failed to read lt9211 chip \n");
		goto err_to_dev_reg;
	}

	LT9211_Config();
	LT9211_ledpwd();

err_to_dev_reg:
	return 0;
}

static const struct udevice_id lt9211_id[] = {
	{
		.compatible = "mipi2lvds,lt9211",},
	{ },
};

U_BOOT_DRIVER(lt9211) = {
	.name = "lt9211",
	.id = UCLASS_REGULATOR,
	.probe = lt9211_probe,
	.of_match = lt9211_id,
	.ofdata_to_platdata = lt9211_ofdata_to_platdata,
	//.priv_auto_alloc_size = sizeof(struct rk860x_regulator_info),
};


