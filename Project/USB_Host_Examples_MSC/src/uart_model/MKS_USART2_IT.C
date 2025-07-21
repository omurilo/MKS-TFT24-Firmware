/********************   (C) COPYRIGHT 2014 www.makerbase.com.cn   ********************
 * �ļ���  ��MKS_USART2_IT.c
 * ����    ��Marlinͨ�Ŵ���ģ��
						 1. printerStaus = idle ,��PUSH��gcodeCmdTxFIFO�������usart2����,����usart2�յ���Ӧ��Push��gcodeCmdRxFIFO��
						 2. printerStaus = working��
						 		a) ʵʱ����gcodeTxFIFO�Ĵ�ӡ���ݣ�
						 		b) �������ݺ󣬳���5sδ�յ�Ӧ���ظ����͸����ݣ�ֱ���յ�Ӧ��
						 		c) �յ� Error:Line Number is not Last Line Number+1, Last Line: n������Nn+1���ݣ�
						 		d) �յ� Error:checksum mismatch, Last Line: n������Nn+1���ݡ�
						 		e) ��gcodeCmdTxFIFO������ʱ�����ȷ���gcodeCmdTxFIFO������,����Ӧ��Push��gcodeCmdRxFIFO��
						 3. �κ�ʱ��������󣬳���5sδ�յ�Ӧ���ظ����͸����ֱ���յ�Ӧ��
						 4. printerStaus״̬ת��ͼ����״̬ת��ͼ_pr��
 * ����    ��skyblue
**********************************************************************************/


#include "draw_ui.h"
#include "mks_tft_com.h"
#include "SZ_STM32F107VC_LIB.h"
#include "gcode.h"
#include "draw_mesh_leveling.h"  // Certifique-se de ter esse .h ou use extern

USART2DATATYPE usart2Data;

TFT_FIFO gcodeTxFIFO;			//gcode ���ݷ��Ͷ���
//TFT_FIFO gcodeRxFIFO;			//gcode	���ݽ��ն���

TFT_FIFO gcodeCmdTxFIFO;		//gcode ָ��Ͷ���
TFT_FIFO gcodeCmdRxFIFO;		//gcode	ָ����ն���

__IO u16 tftDelayCnt = 0;
__IO u16 fileEndCnt = 0;

unsigned char pauseFlag = 0;
FIREWARE_TYPE firmwareType = marlin;
unsigned char wait_cnt = 0;

unsigned char positionSaveFlag = 0;
unsigned char recover_flag = 0;

unsigned char SendOneTime = 0;

extern uint8_t repetier_repint_flag;
static uint8_t M105REC_OK_FLG = 0;

extern uint8_t FanSpeed_bak;
extern volatile uint8_t get_temp_flag;
extern uint8_t Get_Temperature_Flg;
extern unsigned char breakpoint_homeFlag;

extern uint8_t Filament_in_flg;
extern uint8_t Filament_out_flg;

extern uint8_t mesh_cap_active;
extern uint8_t current_mesh_row;

void mksUsart2Resend(void);

void mksUsart2Resend(void)
{
	//return;									//10s
		if(usart2Data.timerCnt>(15000/TICK_CYCLE)) 
		{
			if((printerStaus == pr_idle))
			{
				usart2Data.timerCnt = 0;	
				usart2Data.prWaitStatus = pr_wait_idle;
				usart2Data.timer = timer_stop;
				return;
			}
			
			if(usart2Data.timer == timer_running && usart2Data.prWaitStatus != pr_wait_idle)	//timer_running=1
			{
				switch(printerStaus)
					{
						//case pr_reprint:
						case pr_working:	//pr_working = 1
						case pr_pause:		//pr_pause = 2
								//USART2_SR |= 0x0040;	//����һ�η����ж�
								usart2TxStart();	
							break;
						case pr_idle:		//pr_idle = 0
								usart2TxStart();	//0303
							break;							//0303
						case pr_stop:		//pr_stop = 3
								//usart2CmdTxStart();
							break;
						default : break;
					}
				}
			usart2Data.timerCnt = 0;	
		}
}

void mksUsart2RepeatTx(void)		
{
		usart2Data.timerCnt++;
	
		if(tftDelayCnt != 0)	tftDelayCnt--;
	
		if(fileEndCnt != 0)		fileEndCnt--;
	
		//mksUsart2Resend();
	/*
		if(ADCConvertedValue < 0x0B60)  //4.7V
		{
			if(printerStaus == pr_working)	
			{
				BACKLIGHT = BACKLIGHT_OFF;	
				__set_PRIMASK(1);				//�ر����ж�
				__set_FAULTMASK(1);
				rePrintSaveData();
				__set_PRIMASK(0);			//�������ж�
				__set_FAULTMASK(0);
				BACKLIGHT = BACKLIGHT_ON;				
				
			}
		}
	*/
}

void mksUsart2Polling(void)
{
	unsigned char G28Gode[4]="G28\n";
	unsigned char G92Gode[13]="G92 X0 Y0 Z0\n";

	unsigned char i;  //???
	unsigned char buf[20];

	mksBeeperAlarm();

	if(positionSaveFlag == 1 && printerStaus == pr_pause)		//�ƶ��󱣴�λ��	
	{
		positionSaveFlag = 0;
		positionSaveProcess();
	}
	//��ȡZ������
	if((gCfgItems.getzpos_flg == 1)&&(disp_state == MOVE_MOTOR_UI))
	{
		gCfgItems.getzpos_flg = 0;
		Get_Z_Pos_display_value();
	}
	
	mksUsart2Resend();
	
	switch(printerStaus)
	{
		case pr_working:		//pr_working = 1
/*--------------reprint test--------------------*/	
				//if(mksReprintTest()) return;		
/*--------------reprint test--------------------*/	
				RePrintData.printerStatus = printer_normal;
				usart2Data.timer = timer_running;
				heatFlag = 0;
				if(pauseFlag == 1)
				{
					pauseFlag = 0;
					if(RePrintData.saveFlag == SAVE_DIS)
						relocalZ();
					else
						relacalSave();
				}
				
				
				switch(usart2Data.printer)
				{
					case printer_idle:				//printer_idle 0
								udiskBufferInit();
								//HC-chen 4.21
								gCfgItems.printSpeed = 100;
								SET_MOVE_SPEED_COMMAND(buf, gCfgItems.printSpeed);
								pushFIFO(&gcodeCmdTxFIFO, buf);
								gCfgItems.printExtSpeed0 = 100;
								SET_EXTRU_SPEED_COMMAND(buf, gCfgItems.printExtSpeed0);
								pushFIFO(&gcodeCmdTxFIFO, buf);
								
								usart2Data.printer = printer_working;
								if(gCfgItems.pwd_reprint_flg == 1)
								{
									gCfgItems.pwd_reprint_flg = 0;
									BreakPointReGcode();
								}
								while(checkFIFO(&gcodeTxFIFO)!= fifo_full)
								{
									//����repetier�̼���ԭ�򣬷��������
									//���֤�����λ��ʱ����Ϊ(0,0,0)��
									//��ֻ��������ͨ���͡�
									if((firmwareType == repetier)&&(gCfgItems.machineType != 2)&&(SendOneTime == 1)&&(repetier_repint_flag == 1))
									{
										repetier_repint_flag = 0;
										SendOneTime = 0;
										pushFIFO(&gcodeTxFIFO,G28Gode);
										pushFIFO(&gcodeTxFIFO,G92Gode);
									}
								
									udiskFileR(srcfp);												//���ļ�
									pushTxGcode();
									
									#if debug_flg == 1
									T_function(1200,1);
									#endif

									//chen 10.7
									if(srcfp->fsize <UDISKBUFLEN)
									{
										if((udiskBuffer.state[(udiskBuffer.current+1)%2] == udisk_buf_empty)
											&& (udiskBuffer.p - udiskBuffer.buffer[udiskBuffer.current] == srcfp->fsize))break;
									}
								}
								printerInit();
								break;
					case printer_waiting:			//printer_waiting 2
								//relocalZ();
								usart2Data.printer = printer_working;
								//while(checkFIFO(&gcodeTxFIFO)!= fifo_full)
								for(i=0;i<FIFO_NODE;i++)
								{
									udiskFileR(srcfp);												//���ļ�
									pushTxGcode();
									if(checkFIFO(&gcodeTxFIFO)== fifo_full) break;
								}
								
								if(popFIFO(&gcodeTxFIFO,&usart2Data.usart2Txbuf[0]) != fifo_empty)	//��������
								{	
									usart2Data.prWaitStatus = pr_wait_data;
									usart2TxStart();
								}
						break;
					case printer_working:	//printer_working  1
									udiskFileR(srcfp);
									pushTxGcode();
		
						break;
					default :break;
				}

			break;
		case pr_pause:	//pr_pause = 2
				usart2Data.timer = timer_stop;
		
				if(homeXY()) 	
				{
					pauseFlag = 1;
				}
				rePrintSaveData();
				if(checkFIFO(&gcodeCmdTxFIFO) !=fifo_empty && usart2Data.prWaitStatus == pr_wait_idle)
				{
					popFIFO(&gcodeCmdTxFIFO,&usart2Data.usart2Txbuf[0]);
					usart2Data.prWaitStatus = pr_wait_cmd;
					usart2TxStart();
				}
				break;
		case pr_idle:		//pr_idle = 0
					if(checkFIFO(&gcodeCmdTxFIFO) !=fifo_empty && usart2Data.prWaitStatus == pr_wait_idle)
						{
							popFIFO(&gcodeCmdTxFIFO,&usart2Data.usart2Txbuf[0]);
							usart2Data.prWaitStatus = pr_wait_cmd;
							usart2Data.timer = timer_running;	//0303
							usart2TxStart();
						}				
					break;
		case pr_stop:		//pr_stop = 3
					
					
					udiskFileStaus = udisk_file_end;
					printerStaus = pr_idle;		//��ӡ����
					usart2Data.printer = printer_idle;
					usart2Data.prWaitStatus = pr_wait_idle;
					usart2Data.timer = timer_stop;						//�����ʱ��
					
					//tftDelay(3);
					//printerInit();
					//tftDelay(2);

					I2C_EE_Init(100000);
					
					I2C_EE_BufferRead(&dataToEeprom, BAK_REPRINT_INFO,  4);
					dataToEeprom &= 0x00ffffff;
					dataToEeprom |= (uint32_t)(printer_normal << 24 ) & 0xff000000;
					I2C_EE_BufferWrite(&dataToEeprom, BAK_REPRINT_INFO,  4); 		// �����־(uint8_t) | ��λunit (uint8_t) | saveFlag(uint8_t)| null(uint8_t)
		
					printerStop();

					break;
		
		case pr_reprint:		//����
				//rePrintProcess();					
				//printerStaus = pr_working;		//print test
				//usart2Data.printer = printer_waiting;
				#if 0
				Get_Temperature_Flg = 1;
				usart2Data.timer == timer_running;
				if(checkFIFO(&gcodeCmdTxFIFO) !=fifo_empty && usart2Data.prWaitStatus == pr_wait_idle)
				{
						popFIFO(&gcodeCmdTxFIFO,&usart2Data.usart2Txbuf[0]);
						usart2Data.prWaitStatus = pr_wait_cmd;
						usart2TxStart();
				}
				#endif
			break;
	case breakpoint_reprint:
				if(breakpoint_homeXY()) 	
				{
					gcodeLineCnt = 0;
				}
				if(checkFIFO(&gcodeCmdTxFIFO) !=fifo_empty && usart2Data.prWaitStatus == pr_wait_idle)
				{
					popFIFO(&gcodeCmdTxFIFO,&usart2Data.usart2Txbuf[0]);
					usart2Data.prWaitStatus = pr_wait_cmd;
					usart2TxStart();
				}				
				if(gCfgItems.findpoint_start == 1)
				{
					if(breakpoint_fixPositionZ())
					{
						gCfgItems.findpoint_start = 0;
						breakpoint_homeFlag = 0;
						//BreakPointReGcode();
						Get_Temperature_Flg = 1;
						get_temp_flag = 1;
						I2C_EE_Init(400000);
						start_print_time();
						reset_print_time();
						usart2Data.printer = printer_idle;
						printerStaus = pr_working;			
						//gCfgItems.pwd_reprint_flg = 0;
						
					}
				}
			break;		
		default : break;
			
	}
}


void mksUsart2Init(void)
{
		firmwareType = marlin;
		wait_cnt = 0;

		usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
		usart2Data.txP = &usart2Data.usart2Txbuf[0];
		usart2Data.txBase = usart2Data.txP;
		usart2Data.printer = printer_idle;
		usart2Data.timer = timer_stop;
		usart2Data.prWaitStatus = pr_wait_idle;
		USART2_SR &= 0xffbf;		//����жϱ�־λ
		USART2_SR &= 0xffdf;
		USART2_CR1 |= 0x40;			//��������ж�����
		USART2_SR &= 0xffbf;		//����жϱ�־λ

		RePrintData.saveFlag = SAVE_DIS;
		
		udiskBufferInit();
	
		rePrintInit();
}

void get_zoffset_value()
{
	uint8_t i,size;
	//chen 8.31 ������M851���zoffset��ֵecho:Z Offset : -0.15
	if((usart2Data.usart2Txbuf[0] == 'M') 
		&& ((usart2Data.usart2Txbuf[1] == '2' || usart2Data.usart2Txbuf[1] == '8'))
		&& ((usart2Data.usart2Txbuf[2] == '9') ||(usart2Data.usart2Txbuf[2] == '5')) 
		&& ((usart2Data.usart2Txbuf[3] == '0' ) || (usart2Data.usart2Txbuf[3] == '1' ))
		&& (usart2Data.usart2Txbuf[4]=='\n' ||usart2Data.usart2Txbuf[5] == 'Z'))
	{
		if(usart2Data.usart2Rxbuf[0] == 'e' && usart2Data.usart2Rxbuf[1] == 'c' && usart2Data.usart2Rxbuf[2] == 'h' 
			&& usart2Data.usart2Rxbuf[3] == 'o' && usart2Data.usart2Rxbuf[4] == ':' ) //&& usart2Data.usart2Rxbuf[5] == 'Z')
		{
			#if 0
			//���з��Ż��߳���10���ֽ�������forѭ��
			//��ֵ�������±�Ϊ16���ֽڿ�ʼ
			
			//�ظ�echo:Z Offset : -0.15
			if(usart2Data.usart2Txbuf[4]=='\n')
			{
				for(i=0;i<=10 && usart2Data.usart2Rxbuf[i+16] != '\n' ;i++)
				{
					gCfgItems.disp_zoffset_buf[i] = usart2Data.usart2Rxbuf[i+16];
				}
			}
			//�ظ�echo:Z Offset 0.15
			else
			{
				for(i=0;i<=10 && usart2Data.usart2Rxbuf[i+14] != '\n' ;i++)
				{
					gCfgItems.disp_zoffset_buf[i] = usart2Data.usart2Rxbuf[i+14];
				}
			}
			
			//���Zoffset��ֵ
			//gCfgItems.zoffsetValue = atof(disp_zoffset_buf);
			//DecStr2Float(disp_zoffset_buf,&gCfgItems.zoffsetValue);
			//�ӱ�־λ��main��ˢ����ʾ
			#else
			size = sizeof(gCfgItems.disp_zoffset_buf)-1;
			for(i=0;i<size && usart2Data.usart2Rxbuf[i+11] != '\n' ;i++)
			{
				gCfgItems.disp_zoffset_buf[i] = usart2Data.usart2Rxbuf[i+11];
			}
			gCfgItems.disp_zoffset_buf[i] = 0;
			#endif
			gCfgItems.zoffset_disp_flag = ENABLE;
			
		}
	}
}

void mksUsart2IrqHandlerUser(void)
{
		unsigned char i;
		if( USART2_SR & 0x0020)	//rx 
		{
				*(usart2Data.rxP++) = USART2_DR & 0xff;
				USART2_SR &= 0xffdf;
			
				if(*(usart2Data.rxP-1) == '\n')		//0x0A �յ�������
				{						
					if(RePrintData.saveEnable)	getSavePosition();
					
					if(gCfgItems.getzpos_enable == 1) getZPosition();//�ƶ������Z����ʾֵ
					
					if(usart2Data.usart2Rxbuf[0] =='w' &&  usart2Data.usart2Rxbuf[1] =='a' && usart2Data.usart2Rxbuf[2] =='i' &&  usart2Data.usart2Rxbuf[3] =='t')
					{	//repetier ȥ�����յ��� wait �ַ�
						if (mesh_cap_active) {
							if (isdigit(usart2Data.usart2Rxbuf[0]) || usart2Data.usart2Rxbuf[0] == '-' || usart2Data.usart2Rxbuf[0] == ' ') {
								mesh_leveling_parse_line((char *)usart2Data.usart2Rxbuf);

								// (opcional) redesenhar os pontos enquanto recebe
								GUI_Lock();
								mesh_leveling_draw_points();
								GUI_Unlock();

								if (current_mesh_row >= GRID_ROWS) {
									mesh_cap_active = 0;

									GUI_SetColor(GREEN);
									GUI_DispStringAt("Mesh leveling done", 60, 270);
								}
							}
						}
						usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
						wait_cnt++;
						if(wait_cnt > 2)
							firmwareType = repetier;
					}
					else
						{
							wait_cnt = 0;
							//if(firmwareType != repetier)
								usart2Data.timerCnt = 0; //��ʱ������
							switch(printerStaus)
							{
								case pr_working:	//��ӡ�� pr_working = 1
								case pr_pause:  //��ͣ pr_pause = 2
								//case pr_reprint:
										switch(usart2Data.prWaitStatus)
										{
											case pr_wait_idle:			//0
												pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);	//reretier
												break;
											case pr_wait_cmd:			//pr_wait_cmd=1 	������еȴ���Ӧ
												if((firmwareType == repetier))
												{
													//��repetier�̼�����������������������Ӧ֮��ŷ�
													//��һ�����
													//M105
													if((usart2Data.usart2Txbuf[0] == 'M')&&(usart2Data.usart2Txbuf[1] == '1')\
													&&(usart2Data.usart2Txbuf[2] == '0')&&(usart2Data.usart2Txbuf[3] == '5'))
													{
															pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
															//��ֹ�ڼ��ȵ�ʱ�򲻶Ϸ���һ������
															if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')
															{
																	M105REC_OK_FLG=1;
															}
															//M105:
															if((M105REC_OK_FLG == 1)&&(usart2Data.usart2Rxbuf[0] =='T' &&  usart2Data.usart2Rxbuf[1] ==':'))
															{
																M105REC_OK_FLG=0;
																usart2Data.prWaitStatus = pr_wait_idle;
																prTxNext();
															}												
													}
													#if 1
													//M104/M109/M140/M190
													else if((usart2Data.usart2Txbuf[0] == 'M')&&(usart2Data.usart2Txbuf[1] == '1')\
													&&((usart2Data.usart2Txbuf[2] == '0')||(usart2Data.usart2Txbuf[2] == '4')\
													||(usart2Data.usart2Txbuf[2] == '9'))\
													&&((usart2Data.usart2Txbuf[3] == '0')||(usart2Data.usart2Txbuf[3] == '4')\
													||(usart2Data.usart2Txbuf[3] == '9')))
													{
														pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
														if((usart2Data.usart2Rxbuf[0] =='T')&&(usart2Data.usart2Rxbuf[1] =='a'))
														{
																usart2Data.prWaitStatus = pr_wait_idle;
																prTxNext();
														}
													}
													//M106/M107
													else if((usart2Data.usart2Txbuf[0] == 'M')&&(usart2Data.usart2Txbuf[1] == '1')\
													&&(usart2Data.usart2Txbuf[2] == '0')&&((usart2Data.usart2Txbuf[3] == '7')\
													||(usart2Data.usart2Txbuf[3] == '6'))&&(FanSpeed_bak != gCfgItems.fanSpeed))
													{
														pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
														if(usart2Data.usart2Rxbuf[0] =='F' &&  usart2Data.usart2Rxbuf[1] =='a')
														{
															usart2Data.prWaitStatus = pr_wait_idle;
															prTxNext();
														}
													}
													#endif
													else
													{
														pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
														//M105:
														if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')
														{
															usart2Data.prWaitStatus = pr_wait_idle;
															prTxNext();
														}
													}
													
												}
												else
												{
													get_zoffset_value();
													pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
													//M105:
													if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')
													{
														usart2Data.prWaitStatus = pr_wait_idle;
														prTxNext();
													}
												}												
												break;
											case pr_wait_data:

												if(firmwareType != repetier)
												{
													if(resendProcess()) break;
												}
												else
												{
													if(resendProcess_repetier()) break;
												}
												
												if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')	
												{
													if(recOkProcess()) 
													{
														usart2Data.resendCnt = 0;
														usart2Data.prWaitStatus = pr_wait_idle;
														prTxNext();
													}
													else	//ok : T xxx
														pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
												}
												else //�յ�������push ��CMD���� ��OK
													pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
												
												break;
											default : break;
										} //switch(usart2Data.prWaitStatus) 
									break;
								case pr_idle:		//	pr_idle=0				//�Ǵ�ӡ�� ,�����������ⲿ��ѯgcodeCmdTxFIFO�ǿգ���������
								case pr_stop:		//	pr_stop=3
										if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')
										{
											usart2Data.prWaitStatus = pr_wait_idle;
											usart2Data.timer = timer_stop;		//0303
										}
										pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
										break;							
								default :break;															
								}//switch(printerStaus)

								usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
							//memset(&usart2Data.usart2Rxbuf[0],0,sizeof(usart2Data.usart2Rxbuf));		//test_add
						}
				}
				if(usart2Data.rxP >= &usart2Data.usart2Rxbuf[0] + USART2BUFSIZE-1)
					usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
			
		}

		
		if(USART2_SR & 0x0040)	//tx
		{
			USART2_SR &= 0xffbf;

			if(usart2Data.txP >= usart2Data.txBase + USART2BUFSIZE-1)
			{
				usart2Data.txP = usart2Data.txBase;
				return;
			}
			
			if(*usart2Data.txP != '\r')
			{
				USART2_DR = *(usart2Data.txP++);
				//mension 2018.1.26
				while(!(USART2_SR & 0x0040));
			}

			if(*usart2Data.txP =='\n')
				*(usart2Data.txP+1) = '\r';
			
		}		

		if(USART2_SR & 0x0008)
		{
				*(usart2Data.rxP++) = USART2_DR & 0xff;
				USART2_SR &= 0xffdf;			
		}
	
}
