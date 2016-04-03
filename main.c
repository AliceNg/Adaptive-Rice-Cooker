//changed pirority: water is main pirority
//the water temperature will need to maintain in 65C all the time
//reconfirm sub function: test one time only
//introduce overshoot, throw away check btn function, add few option

#include <16F777.h>
#fuses HS,NOWDT,NOPROTECT,PUT,BROWNOUT 
#use delay(clock=20000000) 
#use rs232(baud=9600, xmit=PIN_C6, rcv=PIN_C7, parity=N) //rs232 setting

// Hard boilded egg selection
#DEFINE HARDBOILED_OPTION '1' 
#DEFINE SOFTBOILED0_OPTION '2'
#DEFINE SOFTBOILED10_OPTION '3'
#DEFINE SOFTBOILED15_OPTION '4'
#DEFINE BEEF_MEDIUMRARE_OPTION '5'
#DEFINE BEEF_MEDIUM_OPTION '6'
#DEFINE BEEF_DONE_OPTION '7'
#DEFINE STOP_OPTION '8'

// critical temperature for each option
#DEFINE HARDBOILED_TEMP 75
#DEFINE SOFTBOILED0_TEMP 65
#DEFINE SOFTBOILED10_TEMP 65
#DEFINE SOFTBOILED15_TEMP 66
#DEFINE BEEF_MEDIUMRARE_TEMP 55
#DEFINE BEEF_MEDIUM_TEMP 60
#DEFINE BEEF_DONE_TEMP 70

#DEFINE HARDBOILED_TIME 360         //(45min*60sec)/5sec=540
#DEFINE SOFTBOILED0_TIME 2            //10sec
#DEFINE SOFTBOILED10_TIME 120      //10min
#DEFINE SOFTBOILED15_TIME 180      //15min
#DEFINE BEEF_MEDIUMRARE_TIME 720   //(1hr*60min*60sec)/5=720
#DEFINE BEEF_MEDIUM_TIME 720        //1hr
#DEFINE BEEF_DONE_TIME 720            //1hr

#DEFINE CH1 PIN_D7
#DEFINE CH2 PIN_D6
#DEFINE CH3 PIN_D5
#DEFINE MAX_DO PIN_C4 
#DEFINE MAX_CLK PIN_C3

#DEFINE FIRST_HEATER_TEMP 200   //200 for protection purpose only

#DEFINE EXCEL_LIMIT 250

char selectOption = '0';
int hp1, hp2, hp3 = 0;
int cook_time =0;
int cri_food_temp, max_heater_temp, cri_water_temp = 0;
long int value1, value2, value3 = 0;
long int waterTemp, heaterTemp, foodTemp = 0;
long int timer_cnt, food_cnt = 0;
long int food_remain = 0;
int excel_cnt=0;
int excel_name = 0;
int foodDone = 0;
int first = 0;
int a = 0;
int count = 1;


//button
#int_rda
void serial_isr(){  
   hp1=getch();
   hp2=getch();
   hp3=getch();
   if(hp3==0x0A)
   {  selectOption = hp1;   }
}

void clearBuffer(){
   selectOption = '0';
   hp1 = 0;
   hp2 = 0;
   hp3 = 0;  
}

//thermocouple
void init_temp() {
   output_low(MAX_CLK); 
   output_low(MAX_DO);

    if(count==1){
        output_low(CH1);  //enable ss1
        output_high(CH2); //disable SS2
        output_high(CH3); //disable SS3
    }
    else if(count==2){
        output_high(CH1);  //disable ss1
        output_low(CH2);   //enable SS2
        output_high(CH3);  //disable SS3 
    }
   else if(count==3){
        output_high(CH1);  //disable ss1
        output_high(CH2);  //disable SS2
        output_low(CH3);   //enable SS3
    }   
   setup_spi(SPI_MASTER | SPI_L_TO_H | SPI_CLK_DIV_16);
   
   output_high(CH1);
   output_high(CH2);
   output_high(CH3); 
} 

int16 read_temp(){
   BYTE datah, datal=0; 
   int16 data=0; 
   
    if(count==1){
        output_low(CH1);  //enable ss1
        output_high(CH2); //disable SS2
        output_high(CH3); //disable SS3
    }
   else if(count==2){
        output_high(CH1);  //disable ss1
        output_low(CH2);   //enable SS2
        output_high(CH3);  //disable SS3 
   }
   else if(count==3){
        output_high(CH1);  //disable ss1
        output_high(CH2);  //disable SS2
        output_low(CH3);   //enable SS3
    }   

   delay_cycles(1); 
   datah=SPI_READ(0); 
   datal=SPI_READ(0); 
   
   output_high(CH1);
   output_high(CH2);
   output_high(CH3); 

   if( bit_test(datal,2)){ 
      bit_set(data,15); 
      return(data); 
   } 
   data = datah<<8; 
   data = data | datal; 
   return(data); 
}

void cali_temp(){
   count=1;
   init_temp();
   value1 = read_temp();
   value1 = value1>>3; 
   value1 = value1*0.25;
   waterTemp = (value1*1.0055)-3;   //long: y=1.00234X-3.029

   count=2;
   init_temp();
   value2 = read_temp();
   value2 = value2>>3; 
   value2 = value2*0.25;
   heaterTemp = value2-3.9;      //y=0.99989X-3.9318<-T1(short probe)

   count=3;
   init_temp();
   value3 = read_temp();
   value3 = value3>>3; 
   value3 = value3*0.25; 
   foodTemp = (value3*0.98)-1.15; //wproof: y=0.97812X-1.288
}

void dispTemp(){
   cali_temp();
   if(foodDone == 0)
   {   printf("lcd_shotex<Temperature#water:%luC#heater:%luC#food:%luC>\r\n",waterTemp, heaterTemp, foodTemp);   }
   else{
      food_remain = 1 + (((cook_time - food_cnt)*5)/60);
      if(food_remain <=0){
         printf("lcd_shotex<Temperature#water:%luC#heater:%luC#food:%luC#DONE>\r\n",waterTemp, heaterTemp, foodTemp);
      }else
      {  printf("lcd_shotex<Temperature#water:%luC#heater:%luC#food:%luC#%ldmins remain>\r\n",waterTemp, heaterTemp, foodTemp, food_remain);   }
   }
}

//excel + count cook time
#int_rtcc
void clock_isr(){
	timer_cnt++;
	if(timer_cnt>48830){  //wait 0.0001024sx48828=5sec
		timer_cnt=0;
		excel_cnt++;
		printf("xls_reffil<option_%d>\r\n", excel_name);//open test.xls
		if(excel_cnt >= EXCEL_LIMIT){
			excel_name++;
			excel_cnt = 0;
		}
		printf("xls_addx<A>\r\n"); //add text in column A                 
		printf("xls_writim\r\n");  //add time in column A              
		printf("xls_nexx\r\n");    //shift to column B            
		printf("xls_writex<%lu>\r\n",waterTemp); //add value1 temp                   
		printf("xls_nexx\r\n");    //shift to column C            
		printf("xls_writex<%lu>\r\n",heaterTemp); //add value2 temp
		printf("xls_nexx\r\n");    //shift to column D            
		printf("xls_writex<%lu>\r\n",foodTemp); //add value3 temp
		printf("xls_nexx\r\n");    //shift to column E            
		printf("xls_writex<%lu>\r\n",food_cnt); //for debug
		printf("xls_nexx\r\n");    //shift to column F            
		printf("xls_writex<%d>\r\n",cook_time); //for debug
		printf("xls_nexx\r\n");    //shift to column G            
		printf("xls_writex<%c>\r\n",selectOption); //for debug

		if(foodDone==1)
		{   food_cnt++;   }
	}
}

//heater on off
void onHeater(){
	output_high(pin_d3);
    printf("lcd_shotex<on heater>\r\n");
    delay_ms(500);
    if(first==0){
		max_heater_temp=FIRST_HEATER_TEMP;
		if(selectOption == HARDBOILED_OPTION || selectOption == SOFTBOILED0_OPTION || selectOption == SOFTBOILED10_OPTION || selectOption == SOFTBOILED15_OPTION){
			cri_water_temp = cri_food_temp;
		}
		else{
			cri_water_temp = cri_food_temp-5;
		}
		first=1;
    }
    else{
		max_heater_temp = cri_food_temp;
		cri_water_temp = cri_food_temp-1;
   }
}

void offHeater(){
   output_low(pin_d3);
}

//recheck is it error
void reconfirm(){
   //repeat 1 times (option:delay 4 second) to ensure the food temp reach the cri_food_temp
    dispTemp();
    delay_ms(1000);
    printf("lcd_shotex<Temperature#T_medium:%luC#T_heater:%luC#T_food:%luC#Test>\r\n",waterTemp, heaterTemp, foodTemp);//for debug
    delay_ms(1000);
}

//buzzer on after food cooked
void buzzerOn(){
    printf("aud_plarep<siren3>\r\n");      //play siren3.mp3
    delay_ms(3000);
    printf("aud_plaoff\r\n");            //stop play siren3.mp3
    delay_ms(1000);
}

void initiate(){
	offHeater();             //initiate it as low
	dispTemp();                 //to flush away the first reading
	delay_ms(200);
}

void resetFlag(){
	first = 0;
	timer_cnt=0;
	foodDone = 0;
	food_cnt =0;
	cook_time = 0;
}


void main(){
   
	set_tris_c(0b10000000);       //tx (outpt), rx(inpt)
	set_tris_d(0b00000000);       //d7,d6,d5 are enable bit(outpt)

	initiate();

	enable_interrupts(int_rda);
	enable_interrupts(GLOBAL);

	while(TRUE){
		printf("lcd_shotex<Please Select>\r\n");
		delay_ms(400);

		if (selectOption != '0'){ //button is pressed
      
			if (selectOption == HARDBOILED_OPTION) {  
				cook_time = HARDBOILED_TIME;
				cri_food_temp = HARDBOILED_TEMP;
			}
			else if (selectOption == SOFTBOILED0_OPTION) {
				cook_time = SOFTBOILED0_TIME;
				cri_food_temp = SOFTBOILED0_TEMP;
			}
			else if (selectOption == SOFTBOILED10_OPTION) {
				cook_time = SOFTBOILED10_TIME;
				cri_food_temp = SOFTBOILED10_TEMP;
			}
			else if (selectOption == SOFTBOILED15_OPTION) {
				cook_time = SOFTBOILED15_TIME;
				cri_food_temp = SOFTBOILED15_TEMP;
			}
			else if (selectOption == BEEF_MEDIUMRARE_OPTION){
				cook_time = BEEF_MEDIUMRARE_TIME;
				cri_food_temp = BEEF_MEDIUMRARE_TEMP;   
			}
			else if (selectOption == BEEF_MEDIUM_OPTION){
				cook_time = BEEF_MEDIUM_TIME;
				cri_food_temp = BEEF_MEDIUM_TEMP;
			}
			else if (selectOption == BEEF_DONE_OPTION){
				cook_time = BEEF_DONE_TIME;
				cri_food_temp = BEEF_DONE_TEMP;   
			}
			else if (selectOption == STOP_OPTION){
				goto end;
			}

			onHeater();

			set_rtcc(0);               //turn on real time clock
			setup_counters(RTCC_INTERNAL, RTCC_DIV_2);
			enable_interrupts(int_rtcc);
			enable_interrupts(GLOBAL);

			do{
				dispTemp();
				delay_ms(1000);

				//if water reach critical water temp
				if(waterTemp >= cri_water_temp){
					water_done:
					offHeater();
					do{
						dispTemp();
						delay_ms(1000);
						if(foodTemp >= cri_food_temp)   //if food done
						{     
							if(foodDone == 0)
							{  goto food_done;   }   
						}
					}while (waterTemp >= cri_water_temp && food_cnt < cook_time && selectOption != STOP_OPTION); //keep loop until the water below cri_water_temp
					if(a == 0)
					{   onHeater();  }
					else if(a == 1){                 //if the prog jump from case "heater too hot"
						a = 0;
						goto heater_done;
					}
				}
				//if food reach critical temp
				else if(foodTemp >= cri_food_temp){
					food_done:
					offHeater();
					if(foodDone == 0)
					{   reconfirm();   }
				 
					//if confirm food temp meet the targeted temperature
					if(foodTemp >= cri_food_temp){	//Extend cook time
						foodDone = 1;
					} 
					else{
						onHeater();   //not neccessary
					}
				}
				//if heater reach max_heater_temp
				else if(heaterTemp >= max_heater_temp){
					heater_done:
					offHeater();
					do{
						dispTemp();
						delay_ms(1000);
						if(foodTemp >= cri_food_temp)         //if food done
						{  goto food_done;   }
						else if(waterTemp >= cri_water_temp){   //if water done
							a = 1;
							goto water_done;  
						}
					}while(heaterTemp >= max_heater_temp && food_cnt < cook_time && selectOption != STOP_OPTION);      //keep loop until the heater below max_heater_temp
					onHeater();
				}

			}while (food_cnt < cook_time && selectOption != STOP_OPTION);

			offHeater();
			disable_interrupts(int_rtcc);
			if(selectOption != STOP_OPTION){
				buzzerOn();
			}

			end:
			clearBuffer();
			resetFlag();
      }
   }  
}
