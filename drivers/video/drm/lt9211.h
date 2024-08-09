
#ifndef		_LT9211_H
#define		_LT9211_H

//////////////////////LT9211 Config////////////////////////////////
#define _Mipi_PortA_
//#define _Mipi_PortB_  //yelsin For WYZN customer only 


typedef enum LT9211_OUTPUTMODE_ENUM
{
    OUTPUT_RGB888         =0,
    OUTPUT_BT656_8BIT     =1,
    OUTPUT_BT1120_16BIT   =2,
    OUTPUT_LVDS_2_PORT    =3,
	  OUTPUT_LVDS_1_PORT    =4,
    OUTPUT_YCbCr444       =5,
    OUTPUT_YCbCr422_16BIT
}Video_Output_mode_TypeDef;

#define LT9211_OutPutModde  OUTPUT_LVDS_1_PORT

typedef enum VIDEO_INPUTMODE_ENUM
{
    INPUT_RGB888          =1,
    INPUT_YCbCr444        =2 ,
    INPUT_YCbCr422_16BIT  =3
}Video_Input_Mode_TypeDef;

#define Video_Input_Mode  INPUT_RGB888


//#define lvds_format_JEIDA

//#define lvds_sync_de_only


//////////option for debug///////////
int lt9211_init(void);
extern struct gpio_desc mipi_bl_en;

typedef struct video_timing{
	u16 hfp;
	u16 hs;
	u16 hbp;
	u16 hact;
	u16 htotal;
	u16 vfp;
	u16 vs;
	u16 vbp;
	u16 vact;
	u16 vtotal;
	u32 pclk_khz;
}VideoTiming;


typedef struct Lane_No{
	u8	swing_high_byte;
	u8	swing_low_byte;
	u8	emph_high_byte;
    u8	emph_low_byte;
}LaneNo;

typedef enum  _MIPI_LANE_NUMBER
{	
	MIPI_1_LANE = 1,
	MIPI_2_LANE = 2,
	MIPI_3_LANE = 3,	
	MIPI_4_LANE = 0   //default 4Lane
} MIPI_LANE_NUMBER__TypeDef;

typedef enum  _REG8235_PIXCK_DIVSEL   ////dessc pll to generate pixel clk
{	
//[1:0]PIXCK_DIVSEL 
//00 176M~352M
//01 88M~176M
//10 44M~88M
//11 22M~44M	
	PIXCLK_LARGER_THAN_176M  = 0x80,
	PIXCLK_88M_176M          = 0x81,
	PIXCLK_44M_88M           = 0x82,	
	PIXCLK_22M_44M           = 0x83   //default 4Lane
} REG8235_PIXCK_DIVSEL_TypeDef;

#define  PCLK_KHZ_44000    44000 
#define  PCLK_KHZ_88000    88000
#define  PCLK_KHZ_176000   176000
#define  PCLK_KHZ_352000   352000 

#endif
/**********************END OF FILE******************/