#include "stm32f1xx_hal.h"
#include "cfg.h"

motor_ctrl motor = {0};
int32_t cnt_all = 0, last_cnt = 0;
extern TIM_HandleTypeDef TIM1_Handler, htim3;
extern int16_t counter;
extern uint8_t start_cnt; //娑撳﹤宕屽▽鑳箯閸欐牗鐖ｈ箛妞剧秴
extern long long encoder_num;
extern uint32_t normal_cnt; //閼惧嘲褰囨潏鎾冲弳閹规洝骞忕拋鈩冩殶閸婏拷
extern Foc motor_foc;
extern float angle;
extern pulse_volt volt_out;
extern adrc Adrc1, Adrc2, Adrc3;
extern Foc motor_foc;
extern uint8_t g_adc_dma_sta;
extern uint16_t g_adc_ave[ADC_CH_NUM];
extern Filt filter;
float last_angle = 0.0;
uint8_t last_sta = 0;


pctr basic_ctrl[6] =
{
	&uhwl, &vhul, &vhwl,
	&whvl, &uhvl, &whul
};


uint8_t forward[6] = {5, 1, 3, 2, 6, 4};
uint8_t backward[6] = {4, 6, 2, 3, 1, 5};

uint8_t idx_f[6] = {1, 3, 2, 5, 0, 4};

//璇诲彇闇嶅皵浼犳劅鍣�
uint8_t hallsensor(void) 
{
	uint8_t state = 0x00;
	if(HAL_GPIO_ReadPin(GPIO_HALLSENSOR, GPIO_PIN_14) != RESET)
	{
		state |= 0x01;
	}
	if(HAL_GPIO_ReadPin(GPIO_HALLSENSOR, GPIO_PIN_13) != RESET)
	{
		state |= 0x02;
	}
	if(HAL_GPIO_ReadPin(GPIO_HALLSENSOR, GPIO_PIN_12) != RESET)
	{
		state |= 0x04;
	}
	
	return state;
}

uint8_t Is_Forward(void)
{
	/*uint16_t idx;
	idx = (idx_f[motor.step_now]+5)%6;
	if(forward[idx] == motor.step_last)*/
	if((angle>last_angle&&(angle-last_angle<320))|| last_angle - angle >=320)
	{
		return 1;//正转返回1
	}
	else
	{
		return 0;//倒转返回0
	}
}
//鍏鎹㈠悜鍑芥暟
void uhvl(void)
{
	TIM1_Handler.Instance->CCR4= motor.pulsea;
	TIM1_Handler.Instance->CCR2 = 0;
	TIM1_Handler.Instance->CCR3 = 0;

	UL_OFF;
	VL_ON;
	WL_OFF;
}

void uhwl(void)
{
	TIM1_Handler.Instance->CCR4 = motor.pulsea;
	TIM1_Handler.Instance->CCR2 = 0;
	TIM1_Handler.Instance->CCR3 = 0;

	UL_OFF;
	VL_OFF;
	WL_ON;
}

void vhul(void)
{
	TIM1_Handler.Instance->CCR4 = 0;
	TIM1_Handler.Instance->CCR2 = motor.pulseb;
	TIM1_Handler.Instance->CCR3 = 0;

	UL_ON;
	VL_OFF;
	WL_OFF;
}

void vhwl(void)
{
	TIM1_Handler.Instance->CCR4 = 0;
	TIM1_Handler.Instance->CCR2 = motor.pulseb;
	TIM1_Handler.Instance->CCR3 = 0;

	UL_OFF;
	VL_OFF;
	WL_ON;
}

void whul(void)
{
	TIM1_Handler.Instance->CCR4 = 0;
	TIM1_Handler.Instance->CCR2 = 0;
	TIM1_Handler.Instance->CCR3 = motor.pulsec;

	UL_ON;
	VL_OFF;
	WL_OFF;
}

void whvl(void)
{
	TIM1_Handler.Instance->CCR4 = 0;
	TIM1_Handler.Instance->CCR2 = 0;
	TIM1_Handler.Instance->CCR3 = motor.pulsec;

	UL_OFF;
	VL_ON;
	WL_OFF;
}

void Stop_motor(void)   //閫氳繃灏咵N_GATE浠庨珮缃綆鍏虫柇妗ヨ噦
{
	SHUTOFF;
	
	TIM1_Handler.Instance->CCR4 = 0;
	TIM1_Handler.Instance->CCR2 = 0;
	TIM1_Handler.Instance->CCR3 = 0;
	
	UL_OFF;
	VL_OFF;
	WL_OFF;
	
	motor.run_flag = STOP;
}

void Start_motor(void)
{
		SHUTON;
		
		TIM1_Handler.Instance->CCR4 = 0;
		TIM1_Handler.Instance->CCR2 = 0;
		TIM1_Handler.Instance->CCR3 = 0;
		
		UL_OFF;
		VL_OFF;
		WL_OFF;
		
		motor.run_flag = START;
}

/*******************************************************************************
瀹氭椂鍣ㄤ腑鏂洖璋冨嚱鏁�
*******************************************************************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//uint8_t i;
	if(htim->Instance == TIM2)
	{
		if(motor.run_flag == START)    //鐢垫満澶勪簬杩愯鐘舵€佹墠浼氳鍙�
		{
			//last_sta = Is_Forward();
			motor.step_last = motor.step_now;
			motor.step_now = hallsensor();
			if(motor.step_now >= 1 && motor.step_now <= 6)
			{
				if(motor.dir == FORWARD) //鐢垫満姝ｈ浆
				{
					basic_ctrl[motor.step_now-1]();			
				}
				else
				{
					basic_ctrl[6 - motor.step_now]();	
				}
			}
		}
	}
	if(htim->Instance == TIM3)
	{
		if(__HAL_TIM_IS_TIM_COUNTING_DOWN(&htim3))
			counter--;
		else
			counter++;
	}
	if(htim->Instance == TIM4)
	{
		filter.trig = 1;
		if((last_angle>angle)&&(last_angle - angle< 0.1)||(last_angle<angle)&&(angle-last_angle<0.1))
			motor_foc.theta = 0;
		else{
			motor_foc.theta = angle - last_angle;
			motor_foc.theta = motor_foc.theta/3;
		}
		cal_motor();
		
		//adc_ave_filter();
		/*if(g_adc_dma_sta == 1)
		{
		adc_ave_filter();
		g_adc_dma_sta = 0;
		volt_out.a_out = adrcCal(&Adrc1, motor_foc.motor_r.Ia+12, g_adc_ave[0]);
		volt_out.b_out = adrcCal(&Adrc2, motor_foc.motor_r.Ib+12, g_adc_ave[1]);
		volt_out.c_out = adrcCal(&Adrc3, motor_foc.motor_r.Ic+12, g_adc_ave[2]);
		adc_dma_enable(ADC_CH_NUM*ADC_MEM_NUM);
			
		//printf("adc1:%d, adc2:%d, adc3:%d\n", g_adc_ave[0], g_adc_ave[1], g_adc_ave[2]);
		//motor.pulsea = (__Constrain(volt_out.a_out/V_SOURCE)+V_OFFSET)*(TIMARR+1);
		//motor.pulseb = (__Constrain(volt_out.b_out/V_SOURCE)+V_OFFSET)*(TIMARR+1);
		//motor.pulsec = (__Constrain(volt_out.c_out/V_SOURCE)+V_OFFSET)*(TIMARR+1);
		}*/
	}
}

void getEncode(uint8_t i)
{
	cnt_all = __HAL_TIM_GET_COUNTER(&htim3);
	cnt_all += counter*65536;
	cnt_all -= last_cnt;
	last_cnt = __HAL_TIM_GET_COUNTER(&htim3);
	cnt_all = cnt_all*3000/ENC_OLD;
}

void gtimRestart(void) 
{ 
	 __HAL_TIM_DISABLE(&htim3); /* 鍏抽棴瀹氭椂鍣� TIMX */ 
	 counter = 0; /* 绱姞鍣ㄦ竻闆� */ 
	 __HAL_TIM_SET_COUNTER(&htim3, 0); /* 璁℃暟鍣ㄦ竻闆� */ 
	 __HAL_TIM_ENABLE(&htim3); /* 浣胯兘瀹氭椂鍣� TIMX */ 

} 
