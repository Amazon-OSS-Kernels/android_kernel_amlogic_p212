/*
 *
 * Copyright (C) 2016 Amazon
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/efuse.h>
#include <command.h>
#include <asm/arch/secure_apb.h>
#include <asm/arch/mailbox.h>
#include <asm/arch/thermal.h>
#include <asm/errno.h>
#ifdef CONFIG_IDME
#include <idme.h>
#endif

#ifndef INT32
typedef int INT32;
typedef unsigned int UINT32;
typedef short INT16;
#endif


#define DMF 1000
#define NUM_BTS_SENSORS 3
#define NUM_VS 2

#define THERMAL_MAX 57000
#define NORMAL_MAX 60000

#define STICKY0_REG (0xc8834400 + (0x70 <<2))
#define VENDOR_THERMAL_SHUTDOWN_MASK 0x00ff0000
#define PCB_THERMAL_SHUTDOWN 0x200000

int offset[NUM_VS][NUM_BTS_SENSORS] = {{3000, 11000, 12000},
					{3000, 11000, 12000}};
int alpha[NUM_VS][NUM_BTS_SENSORS]  = {{15,  15, 15},
					{15,  15, 15}};
int weight[NUM_VS][NUM_BTS_SENSORS] = {{0,   250, 850},
					{770,  100, 100}};
long off_temp[NUM_VS][NUM_BTS_SENSORS] = {{0,  0, 0},
					{0,  0, 0}};

#define AUX_IN0_NTC (0) //NTC6301
#define AUX_IN1_NTC (1)
#define AUX_IN2_NTC (2)

static int ntc_bts_read_temp(int i);

typedef struct{
    INT32 BTS_Temp;
    INT32 TemperatureR;
}BTS_TEMPERATURE;

#define AUX_CHANNEL_NUM 3

#define SARADC_VALUE "saradc_val"


/*
 *=====================================================================================
 *
 * function: get_current_temperature
 * Description: This return current virtual sensor temperature
 * parameter: NONE
 *=====================================================================================
 */
UINT32 get_current_temperature()
{
	int  i, j;
	long temp = 0, temp_h = 0;
	long tempv[NUM_VS] = {0};
	char buf[100] = {};

	setenv("tempvs", " ");

	for (j=0; j < NUM_VS; j++){
		for (i=0; i < NUM_BTS_SENSORS; i++)
		{
			temp = ntc_bts_read_temp(i);
			// printf("%s%d: off_temp = %d, temp=%d\n", __func__, i, off_temp[i], temp);

			if (off_temp[j][i] == 0) off_temp[j][i] = temp - offset[j][i];
			else
			{
				off_temp[j][i] = alpha[j][i] * (temp - offset[j][i]) +
					       (DMF - alpha[j][i]) * off_temp[j][i];
				off_temp[j][i] /= DMF;
			}
			tempv[j] += (weight[j][i] * off_temp[j][i])/DMF;

			// printf("%s%d: off_temp = %d, tempv=%d\n", __func__, i, off_temp[i], tempv);
		}
		printf("tempv=%d\n", tempv[j]);
		if(tempv[j] > temp_h)
			temp_h = tempv[j];
	}

	if ((readl(STICKY0_REG) & VENDOR_THERMAL_SHUTDOWN_MASK) == PCB_THERMAL_SHUTDOWN) {
		printf("last thermal shutdown\n");
		sprintf(buf, "%d", THERMAL_MAX);
	} else {
		printf("not thermal shutdown\n");
		sprintf(buf, "%d", NORMAL_MAX);
	}
	setenv("max_vs_temp", buf);
	memset(buf, 0, sizeof(buf));

	printf("temp_h=%d\n", temp_h);
	sprintf(buf, "%d", temp_h);
	setenv("tempvs", buf);
	memset(buf, 0, sizeof(buf));
	return 0;
}



struct ntc_ts_bts_channel_param {
	int g_RAP_pull_up_R;
	int g_TAP_over_critical_low;
	int g_RAP_pull_up_voltage;
	int g_RAP_ntc_table;
	int g_RAP_ADC_channel;
	int g_AP_TemperatureR;
	char *channelName;
};

static struct ntc_ts_bts_channel_param bts_channel_param[AUX_CHANNEL_NUM] =
{
	{
		10000, //39K,pull up resister for hvt, 10K EVT.
		195652, //base on 100K NTC temp default value -40 deg
		1800, //1.8V ,pull up voltage
		7,  //default is //NTCG104EF104F(100K)
		AUX_IN0_NTC,
		0,
		"ntcts_bts0"
	},
	{
		10000, //39K,pull up resister
		195652, //base on 100K NTC temp default value -40 deg
		1800, //1.8V ,pull up voltage
		7,  //default is //NTCG104EF104F(100K)
		AUX_IN1_NTC,
		0,
		"ntcts_bts1"
	},
	{
		10000, //39K,pull up resister
		195652, //base on 100K NTC temp default value -40 deg
		1800, //1.8V ,pull up voltage
		7,  //default is //NTCG104EF104F(100K)
		AUX_IN2_NTC,
		0,
		"ntcts_bts2"
	},
};

static BTS_TEMPERATURE BTS_Temperature_Table[] = {
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}
};

/*AP_NTC_BL197 */
BTS_TEMPERATURE BTS_Temperature_Table1[] = {
	{-40,74354}, {-35,74354}, {-30,74354}, {-25,74354}, {-20,74354},
	{-15,57626}, {-10,45068}, { -5,35548}, {  0,28267}, {  5,22650},
	{ 10,18280}, { 15,14855}, { 20,12151}, { 25,10000},/*10K*/ { 30,8279},
	{ 35,6892},  { 40,5768},  { 45,4852},  { 50,4101},  { 55,3483},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970}
};

/*AP_NTC_TSM_1*/
BTS_TEMPERATURE BTS_Temperature_Table2[] = {
	{-40,70603}, {-35,70603}, {-30,70603}, {-25,70603}, {-20,70603},
	{-15,55183}, {-10,43499}, { -5,34569}, {  0,27680}, {  5,22316},
	{ 10,18104}, { 15,14773}, { 20,12122}, { 25,10000},/*10K*/ { 30,8294},
	{ 35,6915},  { 40,5795},  { 45,4882},  { 50,4133},  { 55,3516},
	{ 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},
	{ 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},
	{ 60,3004},  { 60,3004},  { 60,3004},  { 60,3004}
};

/*AP_NTC_10_SEN_1*/
BTS_TEMPERATURE BTS_Temperature_Table3[] = {
	{-40,74354}, {-35,74354}, {-30,74354}, {-25,74354}, {-20,74354},
	{-15,57626}, {-10,45068}, { -5,35548}, {  0,28267}, {  5,22650},
	{ 10,18280}, { 15,14855}, { 20,12151}, { 25,10000},/*10K*/ { 30,8279},
	{ 35,6892},  { 40,5768},  { 45,4852},  { 50,4101},  { 55,3483},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970}
};

/*AP_NTC_10(TSM0A103F34D1RZ)*/
BTS_TEMPERATURE BTS_Temperature_Table4[] = {
	{-40,188500}, {-35,144290}, {-30,111330}, {-25,86560}, {-20,67790},
	{-15,53460},  {-10,42450},  { -5,33930},  {  0,27280}, {  5,22070},
	{ 10,17960},  { 15,14700},  { 20,12090},  { 25,10000},/*10K*/ { 30,8310},
	{ 35,6940},   { 40,5830},   { 45,4910},   { 50,4160},  { 55,3540},
	{ 60,3020},   { 65,2590},   { 70,2230},   { 75,1920},  { 80,1670},
	{ 85,1450},   { 90,1270},   { 95,1110},   { 100,975},  { 105,860},
	{ 110,760},   { 115,674},   { 120,599},   { 125,534}
};

/*AP_NTC_47*/
BTS_TEMPERATURE BTS_Temperature_Table5[] = {
	{-40,483954}, {-35,483954}, {-30,483954}, {-25,483954}, {-20,483954},
	{-15,360850}, {-10,271697}, { -5,206463}, {  0,158214}, {  5,122259},
	{ 10,95227},  { 15,74730},  { 20,59065},  { 25,47000},/*47K*/ { 30,37643},
	{ 35,30334},  { 40,24591},  { 45,20048},  { 50,16433},  { 55,13539},
	{ 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},
	{ 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},
	{ 60,11210},  { 60,11210},  { 60,11210},  { 60,11210}
};


/*NTCG104EF104F(100K)*/
BTS_TEMPERATURE BTS_Temperature_Table6[] = {
	{-40,4251000}, {-35,3005000}, {-30,2149000}, {-25,1554000}, {-20,1135000},
	{-15,837800},  {-10,624100},  { -5,469100},  {  0,355600},  {  5,271800},
	{ 10,209400},  { 15,162500},  { 20,127000},  { 25,100000},/*100K*/ { 30,79230},
	{ 35,63180},   { 40,50680},   { 45,40900},   { 50,33190},   { 55,27090},
	{ 60,22220},   { 65,18320},   { 70,15180},   { 75,12640},   { 80,10580},
	{ 85, 8887},   { 90, 7500},   { 95, 6357},   { 100,5410},   { 105,4623},
	{ 110,3965},   { 115,3415},   { 120,2951},   { 125,2560}
};

/*NCP15XH103F03RC*/
BTS_TEMPERATURE BTS_Temperature_Table7[] = {
	{-40,195652}, {-35,148171}, {-30,113347}, {-25,87559}, {-20,68237},
	{-15,53650},  {-10,42506},  { -5,33892},  {  0,27219},  {  5,22021},
	{ 10,17926},  { 15,14674},  { 20,12080},  { 25,10000},/*100K*/ { 30,8315},
	{ 35,6948},  { 40,5833},  { 45,4917},   { 50,4161},  { 55,3535},
	{ 60,3014},   { 65,2586},   { 70,2228},   { 75,1925},  { 80,1669},
	{ 85,1452},   { 90,1268},   { 95,1110},   { 100,974},  { 105,858},
	{ 110,758},   { 115,671},   { 120,596},    { 125,531}
};

/* =========== bts temp read ========== */

/* convert register to temperature  */
static INT16 ntc_bts_thermistor_conver_temp(INT32 Res)
{
	int i=0;
	int asize=0;
	INT32 RES1=0,RES2=0;
	INT32 TAP_Value=-200,TMP1=0,TMP2=0;

	asize = (sizeof(BTS_Temperature_Table)/sizeof(BTS_TEMPERATURE));
	if(Res >= BTS_Temperature_Table[0].TemperatureR)
	{
		TAP_Value = -40; /* min */
	}
	else if(Res <= BTS_Temperature_Table[asize-1].TemperatureR)
	{
		TAP_Value = 125; /* max */
	}
	else
	{
		RES1 = BTS_Temperature_Table[0].TemperatureR;
		TMP1 = BTS_Temperature_Table[0].BTS_Temp;

		for(i=0; i < asize; i++)
		{
			if(Res >= BTS_Temperature_Table[i].TemperatureR)
			{
				RES2 = BTS_Temperature_Table[i].TemperatureR;
				TMP2 = BTS_Temperature_Table[i].BTS_Temp;
				break;
			}
			else
			{
				RES1 = BTS_Temperature_Table[i].TemperatureR;
				TMP1 = BTS_Temperature_Table[i].BTS_Temp;
			}
		}
		TAP_Value = (((Res-RES2)*TMP1)+((RES1-Res)*TMP2))/(RES1-RES2);
	}

	return TAP_Value;
}


/* convert ADC_AP_temp_volt to register */
/* Volt to Temp formula same with 6589  */
static INT16 saradc_volt_to_temp(int index, UINT32 dwVolt)
{
    INT32 TRes;
    INT32 dwVCriAP = 0;
    INT32 BTS_TMP = -100;

#ifdef CONFIG_IDME
    char buf[24] = "";
    if (!idme_get_var_external("board_id", buf, sizeof(buf))) {
        if(!strcmp(buf,"2900001010030017") || !strcmp(buf,"2900000000030017"))	//hvt and proto
            bts_channel_param[index].g_RAP_pull_up_R = 39000;
    }
#endif
    /* SW workaround-----------------------------------------------------
      dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) / (TAP_OVER_CRITICAL_LOW + 39000);
      dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) / (TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
    */
    dwVCriAP = (bts_channel_param[index].g_TAP_over_critical_low * bts_channel_param[index].g_RAP_pull_up_voltage) / 
    			(bts_channel_param[index].g_TAP_over_critical_low + bts_channel_param[index].g_RAP_pull_up_R);

    if(dwVolt > dwVCriAP)
    {
        TRes = bts_channel_param[index].g_TAP_over_critical_low;
    }
    else
    {
        TRes = (bts_channel_param[index].g_RAP_pull_up_R*dwVolt) / (bts_channel_param[index].g_RAP_pull_up_voltage-dwVolt);
    }

    bts_channel_param[index].g_AP_TemperatureR = TRes;

    /* convert register to temperature */
    BTS_TMP = ntc_bts_thermistor_conver_temp(TRes);

    return BTS_TMP;
}

static int get_hw_bts_temp(int index)
{

	int ret = 0, i, ret_value = 0, ret_temp = 0, output;
	int times=2, Channel = bts_channel_param[index].g_RAP_ADC_channel;
	int v_pullup = bts_channel_param[index].g_RAP_pull_up_voltage;
	int adc_range = 1024; /*10bit*/
	static int valid_temp;
	char str[128];

	i = times;
	sprintf(str, "saradc open %d ",Channel);
	run_command(str, 0);
	while (i--)
	{
		run_command("saradc getval", 0);
		ret_temp = (int)simple_strtoul(getenv(SARADC_VALUE), NULL, 0);
		if (ret_temp > 0) //FIXME
		{
			valid_temp = ret_temp;
			ret += ret_temp;
			printf("[thermal_auxadc_get_data]: ret_temp=%d\n",ret_temp);
			break;
		}
		run_command("sleep 1", 0);
	}

	if(ret == 0)
	{
		printf("SARADC channel %d reading error\n",Channel );
	}

	ret = ret*v_pullup/adc_range; /* 82's ADC power */
	printf("Channel = %d\n", Channel);
	printf("saradc reading 0x%x, output mV = %d\n",ret_temp, ret);
	output = saradc_volt_to_temp(index, ret);
	printf("BTS output temperature = %d\n",output);

	return output;
}

int ntc_bts_get_hw_temp(int index)
{
	int t_ret=0;

	/* get HW AP temp (TSAP)
	   cat /sys/class/power_supply/AP/AP_temp
	*/
	t_ret = get_hw_bts_temp(index);
	t_ret = t_ret * 1000;

	if (t_ret > 60000) {/* abnormal high temp */
		printf("[Power/BTS_Thermal] abnormal T_AP=%d\n", t_ret);
	}
	if (t_ret < -1000) {/* abnormal high temp */
		printf("[Power/BTS_Thermal] abnormal T_AP=%d\n", t_ret);
	}

	printf("[ntcts_bts_get_hw_temp] T_AP, %d\n", t_ret);
	return t_ret;
}

/* ========= bts table/tzbts_param handling =========== */

void ntc_bts_copy_table(BTS_TEMPERATURE *des,BTS_TEMPERATURE *src)
{
	int i=0;
	int j=0;

	j = (sizeof(BTS_Temperature_Table)/sizeof(BTS_TEMPERATURE));
	for(i=0; i<j; i++)
	{
		des[i] = src[i];
	}
}

void ntc_bts_prepare_table(int table_num)
{
	printf("Thermal %s with %d\n", __func__, table_num);

	switch(table_num)
	{
		case 1://AP_NTC_BL197
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table1);

			break;
		case 2://AP_NTC_TSM_1
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table2);

			break;
		case 3://AP_NTC_10_SEN_1
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table3);

			break;
		case 4://AP_NTC_10
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table4);

			break;
		case 5://AP_NTC_47
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table5);

			break;
		case 6://NTCG104EF104F
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table6);

			break;
		case 7://NCP15XH103F03RC
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table7);

			break;

		default://AP_NTC_10
			ntc_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table4);

			break;
	}
}

static int ntc_bts_read_temp(int i)
{
	if (i >= AUX_CHANNEL_NUM)
		return -127000;

	ntc_bts_prepare_table(bts_channel_param[i].g_RAP_ntc_table);

	return ntc_bts_get_hw_temp(i);
}



U_BOOT_CMD(
	read_vs_temp,	1,	0,	get_current_temperature,
	"vs temp-system",
	"read_vs pos"
);
