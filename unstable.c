//CAB202 Assignment 2 - Jacob Lodge N9173069
#include <stdint.h>
#include <string.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <stdio.h>
#include "usb_serial.h"
#include <lcd_model.h>
#include "lcd.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cab202_adc.h"


#define FREQ     (8000000.0)
#define PRESCALE0 (1024.0) //  for a Freq of 7.8125Khz
#define PRESCALE1 (8.0)    //  for a Freq of 1Mhz
#define PRESCALE3 (256.0)  //  for a Freq of 31.25Khz
#define DEBOUNCE_MS (70)  // ms delay for debouncing 
#define MAX_WALLS 4

#define STATUS_BAR_HEIGHT 8
#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288   /* pi             */
#endif


//Globals
const int DELAY = 10;
bool pauseMode = false;
bool lvl2Set = false;
bool gameOver = false;
bool lcd = false;

//key inputs

//time keeping
volatile int overflow_counter0 = 0;
volatile uint32_t overflow_counter1 = 0;
volatile uint32_t overflow_counter11= 0;
volatile uint32_t overflow_counter3 = 0;
volatile double time_sec = 0;
volatile int time_min = 0;
volatile int last_sec = 0;
volatile int current_sec = 0;

int secondsPassed = 0;
int lastSecond = 0;
int minPassed = 0;
//Jerry State
//jerrys pixels
#define number_of_jerry_pixels 8
struct bit_map
{
    int x,y;
};
struct bit_map JERRY_IMG[number_of_jerry_pixels];
double jerry_x = 10;
double jerry_y = 10;
double jerry_start_x;
double jerry_start_y;
int jerry_height = 3;
int jerry_width = 3;

//super jerry shit
bool superJerry = false;
int jerry_cooldown = -1;
int milk_cooldown = -1;

//tom
#define number_of_tom_pixels 16
struct bit_map TOM_IMG[number_of_tom_pixels];
double tom_x = 20;
double tom_y = 20;
double tom_dx;
double tom_dy;
double tom_dir;
double tom_step;
int tom_start_x;
int tom_start_y;

//status bar info
int currentLevel = 1;
int jerry_lives = 5;
int score = 0;
int level_score = 0;
//game info
int gameStatus = 0;
// 0 main menu
// 1 active game
// 2 game overs

//potentiometers
int left_adc = 0;
int left_adc_scaler=0;

int right_adc = 0;
int right_adc_scaler = 0;
int right_adc_value = 0;
int walls_backward = false;
bool last_state_forward = true;

//fireworks
#define MAX_FIREWORK 5
struct fireWork{
    double startx;
    double starty;
    double x;
    double y;
    double dx;
    double dy;
    double t1;
    double t2;
    double startt1;
    double startt2;
    double d;
};

struct fireWork fw[MAX_FIREWORK];

//debouncing
volatile uint8_t switch_counterd = 0;
volatile uint8_t switch_counteru = 0;
volatile uint8_t switch_counterl = 0;
volatile uint8_t switch_counterr = 0;
volatile uint8_t switch_counterc = 0;
volatile uint8_t switch_countera = 0;
volatile uint8_t switch_counterb = 0;
                              //  UDLRCABF //firework on F
volatile uint8_t switchStatus = 0b00000000;

//array for what cheese and traps are active
volatile uint8_t cheeseStatus = 0b00000000; //cheeseStatus[7] is that its been rendered
volatile uint8_t trapStatus = 0b00000000; //trapStatus[7] is the potion


struct vwall //max of 120 pixels
{
    volatile int x1;
    volatile int x2;
    volatile int y1;
    volatile int y2;
    volatile int dx;
    volatile int dy;
    volatile int pixels[50][2];
    volatile int pixelCount;
};

//declaire first wall
struct vwall FIRSTWALL;
struct vwall SECONDWALL;
struct vwall THIRDWALL;
struct vwall FOURTHWALL;

struct CHEESE
{
    int x,y;
};
struct CHEESE gameCheese[5];
struct CHEESE gameTraps[5];
struct CHEESE milkBowl;

//door
int doorX;
int doorY;

//keypress
int16_t key_press;

//forward declerations
void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format,...);
void setup_usb_serial( void );
void usb_serial_send(char * message);
void tom_and_jerry_touch(void);
void move_jerry(void);
void move_tom(void);
void cheese_manager(void);
void trap_manager(void);
void usb_serial_read_string(char * message);

void readkeystroke(){

	int16_t char_code = usb_serial_getchar();
    key_press = char_code;
    char buffer[81];
	if ( char_code >= 0 ) {
		snprintf( buffer, sizeof(buffer), "received '%d'\r\n", char_code );
		usb_serial_send( buffer );
	}

}

void setup_firework(){
    for (int i = 0; i < MAX_FIREWORK; i++) // sets all the fireworks x and y to -1 to mean that its not in use
    {
        fw[i].x = -1;
        fw[i].y = -1;
    }

}
int idle_fire_work(){ //returns the location of an unused firework in the array ( i if successfull -1 if nothing found)
    for (int i = 0; i < MAX_FIREWORK; i++) // sets all the fireworks x and y to -1 to mean that its not in use
    {
        if(fw[i].x == -1 && fw[i].y == -1)
        {
            return i;
        }
    }
    //if none are available return -1
    return -1;
}
void prime_fire_work(int i){ //primes the firework with reference i
    fw[i].startx = jerry_x;
    fw[i].starty = jerry_y;
    fw[i].x = jerry_x;
    fw[i].y = jerry_y;
    fw[i].startt1 = abs(jerry_x - tom_x); // calculate difference in X at jerry x, tom x
    fw[i].startt2 = abs(jerry_y - tom_y); // diff in jerry x and tom x
    fw[i].t1 = abs(jerry_x - tom_x);
    fw[i].t2 = abs(jerry_y - tom_y);
    //prime other stuff
    fw[i].d = (sqrt(fw[i].t1*fw[i].t1 + fw[i].t2*fw[i].t2));
    fw[i].dx = fw[i].t1 * 0.1 / fw[i].d;
    fw[i].dy = fw[i].t2 * 0.1 / fw[i].d;
}

void update_fireworks() // will update all fireworks that are not at (-1,-1)
{
    for (int i = 0; i < MAX_FIREWORK; i++) // will move the firework
    {
        if(fw[i].x != -1 && fw[i].y != -1)
        {
            //need to update the difference
            fw[i].t1 = abs(fw[i].x - tom_x);
            fw[i].t2 = abs(fw[i].y - tom_y);
            //update the distance and dx dy
            fw[i].d = (sqrt(fw[i].t1*fw[i].t1 + fw[i].t2*fw[i].t2));
            //start
            if (fw[i].d == 0) // A collision happened HERE between tom and the firework
            {
                fw[i].x = -1;
                fw[i].y = -1;
                tom_x = tom_start_x;
                tom_y = tom_start_y;
            }
            else
            {
            //end
            fw[i].dx = fw[i].t1 * 0.1 / fw[i].d;
            fw[i].dy = fw[i].t2 * 0.1 / fw[i].d;
            //check weather it will be negative movement
            if(fw[i].x > tom_x)
            {
                fw[i].dx = fw[i].dx * -1;
            }
            if(fw[i].y > tom_y)
            {
                fw[i].dy = fw[i].dy * -1;
            }
            //move them the needed distance towards target
            fw[i].x = fw[i].dx + fw[i].x;
            fw[i].y = fw[i].dy + fw[i].y;
            //end else
            }
        }
    }

    //check for wall collisions
    for (int j = 0; j < MAX_FIREWORK; j++)
    {
        for (int k = 0; k < FIRSTWALL.pixelCount; k++)
        {
            if (round(fw[j].x) == FIRSTWALL.pixels[k][0] && round(fw[j].y) == FIRSTWALL.pixels[k][1])
            {
                fw[j].x = -1;
                fw[j].y = -1;
            }
            
        }
        for (int k = 0; k < SECONDWALL.pixelCount; k++)
        {
            if (round(fw[j].x) == SECONDWALL.pixels[k][0] && round(fw[j].y) == SECONDWALL.pixels[k][1])
            {
                fw[j].x = -1;
                fw[j].y = -1;
            }  
        }
        for (int k = 0; k < THIRDWALL.pixelCount; k++)
        {
            if (round(fw[j].x) == THIRDWALL.pixels[k][0] && round(fw[j].y) == THIRDWALL.pixels[k][1])
            {
                fw[j].x = -1;
                fw[j].y = -1;
            }  
        } 
        for (int k = 0; k < FOURTHWALL.pixelCount; k++)
        {
            if (round(fw[j].x) == FOURTHWALL.pixels[k][0] && round(fw[j].y) == FOURTHWALL.pixels[k][1])
            {
                fw[j].x = -1;
                fw[j].y = -1;
            }  
        } 
    } 


}
void draw_fireworks(){
        for (int i = 0; i < MAX_FIREWORK; i++)
    {
        if(fw[i].x != -1 && fw[i].y != -1)
        {
            draw_pixel(round(fw[i].x),round(fw[i].y),FG_COLOUR);
        }

    }
}

void milk_checker(){
    //lets check if we are on a milk
    if (BIT_IS_SET(trapStatus,7))
        {    
        int jleft = jerry_x;
        int jright = jerry_x + 2;
        int jtop = jerry_y;
        int jbottom = jerry_y + 2;

	    int cleft = milkBowl.x;
        int cright = milkBowl.x + 2;
        int ctop = milkBowl.y;
        int cbottom = milkBowl.y + 2;

        if( jleft <= cright &&
        jright >= cleft &&
        jtop <= cbottom &&
        jbottom >= ctop
        )
        {
        CLEAR_BIT(trapStatus,7);

        //set the super jerry countdown to 10 and the cool down to start
        milk_cooldown = 5;
        jerry_cooldown = 10;
            superJerry = true;
            char buffer2[81];
            snprintf( buffer2, sizeof(buffer2), "jerry ate the potion");
	        usb_serial_send( buffer2 );
        }
    }
}

void milk_manager(){
    if (!BIT_IS_SET(trapStatus,7))
    {
        SET_BIT(trapStatus,7);
        milkBowl.x = tom_x - 3;
        milkBowl.y = tom_y;
    }
    
}

void drawMilk(){
    if (BIT_IS_SET(trapStatus,7))
    {
    draw_pixel(milkBowl.x+1,milkBowl.y,FG_COLOUR);
    draw_pixel(milkBowl.x,milkBowl.y+1,FG_COLOUR);
    draw_pixel(milkBowl.x+2,milkBowl.y+1,FG_COLOUR);
    draw_pixel(milkBowl.x+1,milkBowl.y+2,FG_COLOUR);
    draw_pixel(milkBowl.x+1,milkBowl.y+1,FG_COLOUR);
    }
    

}

void timer_update(){ //function for holding game time, seperate from clock


    if(secondsPassed % 3 == 0 && secondsPassed != lastSecond)
    {
        trap_manager();
    }
    if (secondsPassed % 5 == 0 && secondsPassed != lastSecond && currentLevel == 2)
    {
        milk_manager();
    }
    
    if(secondsPassed % 2 == 0 && secondsPassed != lastSecond)
    {
        //cheese
        cheese_manager();
    }
    if(secondsPassed > 60)
    {
        minPassed++;
        secondsPassed = 0;
    }

    if (jerry_cooldown != -1 && secondsPassed != lastSecond)
    {
        jerry_cooldown--;
        if (jerry_cooldown == 0)
        {
            jerry_cooldown = -1;
            superJerry = false;
        }
            char buffer2[81];
            snprintf( buffer2, sizeof(buffer2), "time left = %d",jerry_cooldown);
	        usb_serial_send( buffer2 );
    }

    if(milk_cooldown != -1 && secondsPassed != lastSecond)
    {
        milk_cooldown--;
        if (milk_cooldown==0)
        {
            milk_cooldown =-1;
            SET_BIT(trapStatus,7);
        }
        
    }



lastSecond = secondsPassed;
}


void draw_cheese(){
    for (int i = 0; i < 5; i++)
    {
        if (BIT_IS_SET(cheeseStatus,i))
        {
            draw_pixel(gameCheese[i].x,gameCheese[i].y,FG_COLOUR);
            draw_pixel(gameCheese[i].x+1,gameCheese[i].y,FG_COLOUR);
            draw_pixel(gameCheese[i].x+1,gameCheese[i].y+1,FG_COLOUR);
            draw_pixel(gameCheese[i].x,gameCheese[i].y+1,FG_COLOUR);
        }
        
    }
    
}


bool cheeseFreeFromWall(int x, int y){

    int cheeseX[4];
    int cheeseY[4];

    cheeseX[0] = x;
    cheeseY[0] = y;
    cheeseX[1] = x+1;
    cheeseY[1] = y;
    cheeseX[2] = x;
    cheeseY[2] = y+1;
    cheeseX[3] = x+1;
    cheeseY[3] = y+1;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < FIRSTWALL.pixelCount; j++)
        {
            if (FIRSTWALL.pixels[j][0] == cheeseX[i] && FIRSTWALL.pixels[j][1] == cheeseY[i])
            {
                return false;
            }
            
        }
        for (int j = 0; j < SECONDWALL.pixelCount; j++)
        {
            if (SECONDWALL.pixels[j][0] == cheeseX[i] && SECONDWALL.pixels[j][1] == cheeseY[i])
            {
                return false;
            }
        }
        for (int j = 0; j < THIRDWALL.pixelCount; j++)
        {
            if (THIRDWALL.pixels[j][0] == cheeseX[i] && THIRDWALL.pixels[j][1] == cheeseY[i])
            {
                return false;
            }
            
        }
        for (int j = 0; j < FOURTHWALL.pixelCount; j++)
        {
            if (FOURTHWALL.pixels[j][0] == cheeseX[i] && FOURTHWALL.pixels[j][1] == cheeseY[i])
            {
                return false;
            }
        }
        
    }

    for (int j = 0; j < 4; j++)
    {

        for (int i = 0; i < 5; i++)
        {
        if (BIT_IS_SET(trapStatus,i))
        {
            if (gameTraps[i].x == cheeseX[j] && gameTraps[i].y == cheeseY[j])
            {
                return false;
            }
            if (gameTraps[i].x+2 == cheeseX[j] && gameTraps[i].y == cheeseY[j])
            {
                return false;
            }
            if (gameTraps[i].x+1 == cheeseX[j] && gameTraps[i].y+1 == cheeseY[j])
            {
                return false;
            }
            if (gameTraps[i].x == cheeseX[j] && gameTraps[i].y+2 == cheeseY[j])
            {
                return false;
            }
            if (gameTraps[i].x+2 == cheeseX[j] && gameTraps[i].y+2 == cheeseY[j])
            {
                return false;
            }
        }
        }

        
    }
    



    return true;
    
    
}

//CHEESE FUNCTIONS
void cheese_touch(){
    //int counter = -1;
    for ( int i = 0; i < 5; i++)
    {
        if (BIT_IS_SET(cheeseStatus,i))
        {    
        int jleft = jerry_x;
        int jright = jerry_x + 2;
        int jtop = jerry_y;
        int jbottom = jerry_y + 2;

	    int cleft = gameCheese[i].x+1;
        int cright = gameCheese[i].x + 2;
        int ctop = gameCheese[i].y+1;
        int cbottom = gameCheese[i].y + 2;

        if( jleft <= cright &&
        jright >= cleft &&
        jtop <= cbottom &&
        jbottom >= ctop
        )
        {
            //set the cheese array to being not used
            CLEAR_BIT(cheeseStatus,i);
            score++;
            level_score++;


            //lets check if the cheese eaten is 3, if so fireworks are enabled
            if (score > 2)
            {
                SET_BIT(switchStatus,0);
            }
            

            return;
        }
        }
        
    }
    
}

void trap_touch()
{
        for ( int i = 0; i < 5; i++)
    {
        if (BIT_IS_SET(trapStatus,i))
        {    
        int jleft = jerry_x;
        int jright = jerry_x + 2;
        int jtop = jerry_y;
        int jbottom = jerry_y + 2;

	    int cleft = gameTraps[i].x+1;
        int cright = gameTraps[i].x + 2;
        int ctop = gameTraps[i].y+1;
        int cbottom = gameTraps[i].y + 2;

        if( jleft <= cright &&
        jright >= cleft &&
        jtop <= cbottom &&
        jbottom >= ctop
        )
        {
            //set the cheese array to being not used
            if (superJerry == 0)
            {
                /* code */
            
            
            CLEAR_BIT(trapStatus,i);
            jerry_lives--;
            return;
            }
        }
        }
        
    }

}
void trap_manager(){
    int freeTrapBit = -1;
    for (int i = 0; i < 5; i++)
    {
        if(!BIT_IS_SET(trapStatus,i))
        {
            freeTrapBit = i;
            //set it so it is in use
            SET_BIT(trapStatus,i);
            break;
        }
    }

    if(freeTrapBit != -1)
    {
        gameTraps[freeTrapBit].x = round(tom_x);
        gameTraps[freeTrapBit].y = round(tom_y);
    }
    
}

void draw_traps(){
    for (int i = 0; i < 5; i++)
    {
        if (BIT_IS_SET(trapStatus,i))
        {
            draw_pixel(gameTraps[i].x,gameTraps[i].y,FG_COLOUR);
            draw_pixel(gameTraps[i].x+2,gameTraps[i].y,FG_COLOUR);
            draw_pixel(gameTraps[i].x+1,gameTraps[i].y+1,FG_COLOUR);
            draw_pixel(gameTraps[i].x,gameTraps[i].y+2,FG_COLOUR);
            draw_pixel(gameTraps[i].x+2,gameTraps[i].y+2,FG_COLOUR);

        }
        
    }

}

void cheese_manager(){
    int freeCheeseBit = -1;
    for (int i = 0; i < 5; i++)
    {
        if(!BIT_IS_SET(cheeseStatus,i))
        {
            freeCheeseBit = i;
            // SET THAT BIT SO ITS IN USE
            SET_BIT(cheeseStatus,i);
            break;
        }
    }
    
    if (freeCheeseBit != -1) // then there is a free cheese
    {
        while(true)
        {
        gameCheese[freeCheeseBit].x = round(2+ rand() % LCD_X-3);
        gameCheese[freeCheeseBit].y = round(STATUS_BAR_HEIGHT + 1 + rand() % (LCD_Y - STATUS_BAR_HEIGHT - 2 ));

            if(cheeseFreeFromWall(gameCheese[freeCheeseBit].x,gameCheese[freeCheeseBit].y))
            {
                break;
            }
        
        }
    }
    
    
}

void draw_walls(){

    //draw_line(FIRSTWALL.x1,FIRSTWALL.y1,FIRSTWALL.x2,FIRSTWALL.y2,FG_COLOUR);

    for (int i = 0; i < FIRSTWALL.pixelCount; i++)
    {
        if (FIRSTWALL.pixels[i][1]>STATUS_BAR_HEIGHT)
        {
            draw_pixel(FIRSTWALL.pixels[i][0],FIRSTWALL.pixels[i][1],FG_COLOUR);
        }
        
        
    }
        for (int i = 0; i < SECONDWALL.pixelCount; i++)
    {
        if (SECONDWALL.pixels[i][1]>STATUS_BAR_HEIGHT)
        {
            draw_pixel(SECONDWALL.pixels[i][0],SECONDWALL.pixels[i][1],FG_COLOUR);
        }
    }
    for (int i = 0; i < THIRDWALL.pixelCount; i++)
    {
        if (THIRDWALL.pixels[i][1]>STATUS_BAR_HEIGHT)
        {
            draw_pixel(THIRDWALL.pixels[i][0],THIRDWALL.pixels[i][1],FG_COLOUR);
        }
    }
    for (int i = 0; i < FOURTHWALL.pixelCount; i++)
    {
        if (FOURTHWALL.pixels[i][1]>STATUS_BAR_HEIGHT)
        {
            draw_pixel(FOURTHWALL.pixels[i][0],FOURTHWALL.pixels[i][1],FG_COLOUR);
        }
    }
}
void countPixels(int x1, int y1, int x2, int y2) {
  if ( x1 == x2 ) {
		// Draw vertical line
		for ( int i = y1; (y2 > y1) ? i <= y2 : i >= y2; (y2 > y1) ? i++ : i-- ) {
            FIRSTWALL.pixels[FIRSTWALL.pixelCount][0]= x1;
            FIRSTWALL.pixels[FIRSTWALL.pixelCount][1]=i;
            FIRSTWALL.pixelCount=FIRSTWALL.pixelCount+1;
		}
	}
	else if ( y1 == y2 ) {
		// Draw horizontal line
		for ( int i = x1; (x2 > x1) ? i <= x2 : i >= x2; (x2 > x1) ? i++ : i-- ) {
            FIRSTWALL.pixels[FIRSTWALL.pixelCount][0]= i;
            FIRSTWALL.pixels[FIRSTWALL.pixelCount][1]=y1;
            FIRSTWALL.pixelCount=FIRSTWALL.pixelCount+1;
		}
	}
	else {
		//	Always draw from left to right, regardless of the order the endpoints are 
		//	presented.
		if ( x1 > x2 ) {
			int t = x1;
			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}

		// Get Bresenhaming...
		float dx = x2 - x1;
		float dy = y2 - y1;
		float err = 0.0;
		float derr = ABS(dy / dx);

		for ( int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x-- ) {

            FIRSTWALL.pixels[FIRSTWALL.pixelCount][0]= x;
            FIRSTWALL.pixels[FIRSTWALL.pixelCount][1]=y;
            FIRSTWALL.pixelCount=FIRSTWALL.pixelCount+1;
			err += derr;
			while ( err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2) ) {
                FIRSTWALL.pixels[FIRSTWALL.pixelCount][0]= x;
                FIRSTWALL.pixels[FIRSTWALL.pixelCount][1]=y;
                FIRSTWALL.pixelCount=FIRSTWALL.pixelCount+1;
				y += (dy > 0) - (dy < 0);
				err -= 1.0;
			}
		}
	}
}
//second one
void countPixels2(int x1, int y1, int x2, int y2) {
  if ( x1 == x2 ) {
		// Draw vertical line
		for ( int i = y1; (y2 > y1) ? i <= y2 : i >= y2; (y2 > y1) ? i++ : i-- ) {
            SECONDWALL.pixels[SECONDWALL.pixelCount][0]= x1;
            SECONDWALL.pixels[SECONDWALL.pixelCount][1]=i;
            SECONDWALL.pixelCount=SECONDWALL.pixelCount+1;
		}
	}
	else if ( y1 == y2 ) {
		// Draw horizontal line
		for ( int i = x1; (x2 > x1) ? i <= x2 : i >= x2; (x2 > x1) ? i++ : i-- ) {
            SECONDWALL.pixels[SECONDWALL.pixelCount][0]= i;
            SECONDWALL.pixels[SECONDWALL.pixelCount][1]=y1;
            SECONDWALL.pixelCount=SECONDWALL.pixelCount+1;
		}
	}
	else {
		//	Always draw from left to right, regardless of the order the endpoints are 
		//	presented.
		if ( x1 > x2 ) {
			int t = x1;
			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}

		// Get Bresenhaming...
		float dx = x2 - x1;
		float dy = y2 - y1;
		float err = 0.0;
		float derr = ABS(dy / dx);

		for ( int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x-- ) {

            SECONDWALL.pixels[SECONDWALL.pixelCount][0]= x;
            SECONDWALL.pixels[SECONDWALL.pixelCount][1]=y;
            SECONDWALL.pixelCount=SECONDWALL.pixelCount+1;
			err += derr;
			while ( err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2) ) {
                SECONDWALL.pixels[SECONDWALL.pixelCount][0]= x;
                SECONDWALL.pixels[SECONDWALL.pixelCount][1]=y;
                SECONDWALL.pixelCount=SECONDWALL.pixelCount+1;
				y += (dy > 0) - (dy < 0);
				err -= 1.0;
			}
		}
	}
}
//third wall function
void countPixels3(int x1, int y1, int x2, int y2) {
  if ( x1 == x2 ) {
		// Draw vertical line
		for ( int i = y1; (y2 > y1) ? i <= y2 : i >= y2; (y2 > y1) ? i++ : i-- ) {
            THIRDWALL.pixels[THIRDWALL.pixelCount][0]= x1;
            THIRDWALL.pixels[THIRDWALL.pixelCount][1]=i;
            THIRDWALL.pixelCount=THIRDWALL.pixelCount+1;
		}
	}
	else if ( y1 == y2 ) {
		// Draw horizontal line
		for ( int i = x1; (x2 > x1) ? i <= x2 : i >= x2; (x2 > x1) ? i++ : i-- ) {
            THIRDWALL.pixels[THIRDWALL.pixelCount][0]= i;
            THIRDWALL.pixels[THIRDWALL.pixelCount][1]=y1;
            THIRDWALL.pixelCount=THIRDWALL.pixelCount+1;
		}
	}
	else {
		//	Always draw from left to right, regardless of the order the endpoints are 
		//	presented.
		if ( x1 > x2 ) {
			int t = x1;
			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}

		// Get Bresenhaming...
		float dx = x2 - x1;
		float dy = y2 - y1;
		float err = 0.0;
		float derr = ABS(dy / dx);

		for ( int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x-- ) {

            THIRDWALL.pixels[THIRDWALL.pixelCount][0]= x;
            THIRDWALL.pixels[THIRDWALL.pixelCount][1]=y;
            THIRDWALL.pixelCount=THIRDWALL.pixelCount+1;
			err += derr;
			while ( err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2) ) {
                THIRDWALL.pixels[THIRDWALL.pixelCount][0]= x;
                THIRDWALL.pixels[THIRDWALL.pixelCount][1]=y;
                THIRDWALL.pixelCount=THIRDWALL.pixelCount+1;
				y += (dy > 0) - (dy < 0);
				err -= 1.0;
			}
		}
	}
}
//fourth wall function
void countPixels4(int x1, int y1, int x2, int y2) {
  if ( x1 == x2 ) {
		// Draw vertical line
		for ( int i = y1; (y2 > y1) ? i <= y2 : i >= y2; (y2 > y1) ? i++ : i-- ) {
            FOURTHWALL.pixels[FOURTHWALL.pixelCount][0]= x1;
            FOURTHWALL.pixels[FOURTHWALL.pixelCount][1]=i;
            FOURTHWALL.pixelCount=FOURTHWALL.pixelCount+1;
		}
	}
	else if ( y1 == y2 ) {
		// Draw horizontal line
		for ( int i = x1; (x2 > x1) ? i <= x2 : i >= x2; (x2 > x1) ? i++ : i-- ) {
            FOURTHWALL.pixels[FOURTHWALL.pixelCount][0]= i;
            FOURTHWALL.pixels[FOURTHWALL.pixelCount][1]=y1;
            FOURTHWALL.pixelCount=FOURTHWALL.pixelCount+1;
		}
	}
	else {
		//	Always draw from left to right, regardless of the order the endpoints are 
		//	presented.
		if ( x1 > x2 ) {
			int t = x1;
			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}

		// Get Bresenhaming...
		float dx = x2 - x1;
		float dy = y2 - y1;
		float err = 0.0;
		float derr = ABS(dy / dx);

		for ( int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x-- ) {

            FOURTHWALL.pixels[FOURTHWALL.pixelCount][0]= x;
            FOURTHWALL.pixels[FOURTHWALL.pixelCount][1]=y;
            FOURTHWALL.pixelCount=FOURTHWALL.pixelCount+1;
			err += derr;
			while ( err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2) ) {
                FOURTHWALL.pixels[FOURTHWALL.pixelCount][0]= x;
                FOURTHWALL.pixels[FOURTHWALL.pixelCount][1]=y;
                FOURTHWALL.pixelCount=FOURTHWALL.pixelCount+1;
				y += (dy > 0) - (dy < 0);
				err -= 1.0;
			}
		}
	}
}

void second_wall_setup(){
    FIRSTWALL.dx=1;
    FIRSTWALL.dy=1;
    FIRSTWALL.pixelCount=0;
    FIRSTWALL.x1=18;
    FIRSTWALL.y1=15;
    FIRSTWALL.x2=13;
    FIRSTWALL.y2=25;
    countPixels(FIRSTWALL.x1,FIRSTWALL.y1,FIRSTWALL.x2,FIRSTWALL.y2);

    SECONDWALL.dx=1;
    SECONDWALL.dy=0;
    SECONDWALL.pixelCount=0;
    SECONDWALL.x1=25;
    SECONDWALL.y1=35;
    SECONDWALL.x2=25;
    SECONDWALL.y2=45;
    countPixels2(SECONDWALL.x1,SECONDWALL.y1,SECONDWALL.x2,SECONDWALL.y2);
    THIRDWALL.dx=0;
    THIRDWALL.dy=1;
    THIRDWALL.pixelCount=0;
    THIRDWALL.x1=45;
    THIRDWALL.y1=10;
    THIRDWALL.x2=60;
    THIRDWALL.y2=10;
    countPixels3(THIRDWALL.x1,THIRDWALL.y1,THIRDWALL.x2,THIRDWALL.y2);
    FOURTHWALL.dx=1;
    FOURTHWALL.dy=1;
    FOURTHWALL.pixelCount=0;
    FOURTHWALL.x1=58;
    FOURTHWALL.y1=25;
    FOURTHWALL.x2=27;
    FOURTHWALL.y2=30;
    countPixels4(FOURTHWALL.x1,FOURTHWALL.y1,FOURTHWALL.x2,FOURTHWALL.y2);

}



//returns false if there will be a collision
//returns true if safe
int jerry_wall_check(int futurex, int futurey){ //gets where jerrys next X and Y is going to be then calculate where other bits
// will be, the points will  then check if they collide with a wall

    if (superJerry == 1)
    {
        return 1;
    }
    

    struct bit_map future_jerry[number_of_jerry_pixels];
    future_jerry[0].x = futurex;
    future_jerry[0].y = futurey;
    future_jerry[1].x = futurex+1;
    future_jerry[1].y = futurey;
    future_jerry[2].x = futurex+2;
    future_jerry[2].y = futurey;
    future_jerry[3].x = futurex+2;
    future_jerry[3].y = futurey+1;
    future_jerry[4].x = futurex+2;
    future_jerry[4].y = futurey+2;
    future_jerry[5].x = futurex+1;
    future_jerry[5].y = futurey+2;
    future_jerry[6].x = futurex;
    future_jerry[6].y = futurey+2;
    future_jerry[7].x = futurex;
    future_jerry[7].y = futurey+1;

    //if any of these are are a wall you cant move into it
    for (int i = 0; i < number_of_jerry_pixels; i++)
    {
        //first wall
        for (int j = 0; j < FIRSTWALL.pixelCount; j++)
        {
            if (future_jerry[i].x == FIRSTWALL.pixels[j][0] && future_jerry[i].y == FIRSTWALL.pixels[j][1])
            {
                return 0;
            }
        }
        //second wall
        for (int j = 0; j < SECONDWALL.pixelCount; j++)
        {
            if (future_jerry[i].x == SECONDWALL.pixels[j][0] && future_jerry[i].y == SECONDWALL.pixels[j][1])
            {
                return 0;
            }
        }
        //third wall
        for (int j = 0; j < THIRDWALL.pixelCount; j++)
        {
            if (future_jerry[i].x == THIRDWALL.pixels[j][0] && future_jerry[i].y == THIRDWALL.pixels[j][1])
            {
                return 0;
            }
        }
        //fourth wall
        for (int j = 0; j < FOURTHWALL.pixelCount; j++)
        {
            if (future_jerry[i].x == FOURTHWALL.pixels[j][0] && future_jerry[i].y == FOURTHWALL.pixels[j][1])
            {
                return 0;
            }
        }
    }
    


    return 1;
}

int tom_wall_check(int futureX ,int futureY){
    struct bit_map future_tom[number_of_tom_pixels];
    future_tom[0].x = futureX;
    future_tom[0].y = futureY;
    future_tom[1].x = futureX + 1;
    future_tom[1].y = futureY;
    future_tom[2].x = futureX + 2;
    future_tom[2].y = futureY;
    future_tom[3].x = futureX + 3;
    future_tom[3].y = futureY;
    future_tom[4].x = futureX + 4;
    future_tom[4].y = futureY;
    future_tom[5].x = futureX;
    future_tom[5].y = futureY+1;
    future_tom[6].x = futureX+4;
    future_tom[6].y = futureY+1;
    future_tom[7].x = futureX;
    future_tom[7].y = futureY+2;
    future_tom[8].x = futureX+4;
    future_tom[8].y = futureY+2;
    future_tom[9].x = futureX;
    future_tom[9].y = futureY+3;
    future_tom[10].x = futureX+4;
    future_tom[10].y = futureY+3;
    future_tom[11].x = futureX+4;
    future_tom[11].y = futureY+4;
    future_tom[12].x = futureX+1;
    future_tom[12].y = futureY+4;
    future_tom[13].x = futureX+2;
    future_tom[13].y = futureY+4;
    future_tom[14].x = futureX+3;
    future_tom[14].y = futureY+4;
    future_tom[15].x = futureX;
    future_tom[15].y = futureY+4;
    //now that we have where it is going to be lets check all of those points against the wall points
        //if any of these are are a wall you cant move into it
    for (int i = 0; i < number_of_tom_pixels; i++)
    {
        //first wall
        for (int j = 0; j < FIRSTWALL.pixelCount; j++)
        {
            if (future_tom[i].x == FIRSTWALL.pixels[j][0] && future_tom[i].y == FIRSTWALL.pixels[j][1])
            {
                return 0;
            }
        }
        //second wall
        for (int j = 0; j < SECONDWALL.pixelCount; j++)
        {
            if (future_tom[i].x == SECONDWALL.pixels[j][0] && future_tom[i].y == SECONDWALL.pixels[j][1])
            {
                return 0;
            }
        }
        //third wall
        for (int j = 0; j < THIRDWALL.pixelCount; j++)
        {
            if (future_tom[i].x == THIRDWALL.pixels[j][0] && future_tom[i].y == THIRDWALL.pixels[j][1])
            {
                return 0;
            }
        }
        //fourth wall
        for (int j = 0; j < FOURTHWALL.pixelCount; j++)
        {
            if (future_tom[i].x == FOURTHWALL.pixels[j][0] && future_tom[i].y == FOURTHWALL.pixels[j][1])
            {
                return 0;
            }
        }
    }
    return 1;
}


void pause(){
    if(pauseMode == false)
    {
        pauseMode = true;
    }
    else if(pauseMode == true)
    {   
        pauseMode = false;
    }
}

void game_state(){

    int fireworksOnScreen = 0;
    for (int i = 0; i < MAX_FIREWORK; i++)
    {
        if (fw[i].x != -1)
        {
            fireworksOnScreen++;
        }
        
    }

    int cheeseOnScreen = 0;
    for (int i = 0; i < 5; i++)
    {
        if (BIT_IS_SET(cheeseStatus,i))
        {
            cheeseOnScreen++;
        }
        
    }

    int trapsOnScreen = 0;
    for (int i = 0; i < 5; i++)
    {
        if (BIT_IS_SET(trapStatus,i))
        {
            trapsOnScreen++;
        }
        
    }
    
    


    char buffer[81];
    snprintf( buffer, sizeof(buffer), "Time:%02d:%1.0d Level:%d Lives:%d Score:%d \r\n",minPassed,secondsPassed,currentLevel,jerry_lives,score);
	usb_serial_send( buffer );
    snprintf( buffer, sizeof(buffer), "Fireworks:%d Mousetraps:%d Cheese:%d Cheese Eaten:%d \r\n",fireworksOnScreen,trapsOnScreen,cheeseOnScreen,level_score);
    usb_serial_send( buffer );
    snprintf( buffer, sizeof(buffer), "super jerry:%d paused:%d \r\n",superJerry,pauseMode);
    usb_serial_send( buffer );

}

void input_checker(){

    // does the life checking
    if (jerry_lives == -1)
    {
        gameOver = true;
    }
    


    //lets check if the pause button has been set
    if (gameStatus == 1 && BIT_IS_SET(switchStatus,1))
    {
        pause();
    }

   
    if (gameStatus == 1 && key_press == 112)
    {
        pause();
        key_press = 0;
    }
    
    // l is 108
    if (gameStatus == 1 && key_press == 108)
    {
        currentLevel = 2;
        key_press = 0;
    }

    if(gameStatus == 1 && BIT_IS_SET(switchStatus,2))
    {
        currentLevel = 2;
    }

    if(gameStatus == 1 && key_press == 105)
    {
        game_state();
        key_press = 0;
    }
    

}
double get_current_time(){
    time_sec = ( overflow_counter0 * 256.0 + TCNT0 ) * PRESCALE0 / FREQ;
    current_sec = time_sec;
    if (current_sec != last_sec)
    {
        if (!pauseMode)
        {
            secondsPassed++;
        }
        
    }
    
    last_sec = current_sec;

    if(time_sec >= 60.00){
        overflow_counter0 = 0.0;
        time_min++;
        time_sec = 0.0;
        
    }
    if (gameStatus==0)
    {
        overflow_counter0 = 0.0;
        time_sec = 0;
        time_min = 0;
    }
    return time_sec;
}
//setup a 8 bit timer
void setup_timer0(void) {

		// Timer 0 in normal mode (WGM02), 
		// with pre-scaler 1024 ==> ~30Hz overflow 	(CS02,CS01,CS00).
		// Timer overflow on. (TOIE0)
		CLEAR_BIT(TCCR0B,WGM02);
		SET_BIT(TCCR0B,CS02);  //1
		CLEAR_BIT(TCCR0B,CS01); //0
		SET_BIT(TCCR0B,CS00);   //1
		
		//enabling the timer overflow interrupt
		SET_BIT(TIMSK0, TOIE0);

}
//setup a 16bit timer
void setup_timer1(void) {

		// Timer 1 in normal mode (WGM12. WGM13), 
		// with pre-scaler 8 ==> 	(CS12,CS11,CS10).
		// Timer overflow on. (TOIE1)

		CLEAR_BIT(TCCR1B,WGM12);
		CLEAR_BIT(TCCR1B,WGM13);
		CLEAR_BIT(TCCR1B,CS12);    //0 see table 14-6
		SET_BIT(TCCR1B,CS11);      //1
		CLEAR_BIT(TCCR1B,CS10);     //0
		
	    //enabling the timer overflow interrupt
		SET_BIT(TIMSK1, TOIE1);
		
}
//setup a 16bit timer
void setup_timer3(void) {

	   // Timer 3 in normal mode (WGM32. WGM33), 
		// with pre-scaler 256 ==> 	(CS32,CS31,CS30).
		// Timer overflow on. (TOIE3)

		CLEAR_BIT(TCCR3B,WGM32);
		CLEAR_BIT(TCCR3B,WGM33);
		SET_BIT(TCCR3B,CS32);     //1 see table 14-6
		CLEAR_BIT(TCCR3B,CS31);   //0
		CLEAR_BIT(TCCR3B,CS30);   //0
		
		//enabling the timer overflow interrupt
		SET_BIT(TIMSK3, TOIE3);

}
void setup_tom(){
    tom_start_x = LCD_X - 7;
    tom_start_y = LCD_Y - 9;
    tom_x = tom_start_x;
    tom_y = tom_start_y;
    tom_dir = rand() * M_PI * 2 / RAND_MAX;
    tom_step = 0.1;

    tom_dx = tom_step * cos(tom_dir);
    tom_dy = tom_step * sin(tom_dir);
}
//is that out of border
int borderCheck(int x, int y, int width,int height){
    if ( x < 0 || y < STATUS_BAR_HEIGHT + 1 || x >= LCD_X || x + width >= LCD_X+1 || y + height-1 >= LCD_Y|| y >= LCD_Y ) {
		return 0;
	}
    else
    {
        return 1;
    }
    
}
//What to do when those timers overflow!
ISR(TIMER0_OVF_vect) {	
	overflow_counter0 ++;
    

    //function for debouncing the switches
    uint8_t mask = 0b00000111;
    //test down
    switch_counterd = ((switch_counterd<<1) & mask) | BIT_IS_SET(PINB,7);
    if(switch_counterd == mask)
    {
        SET_BIT(switchStatus,6); 
    }
    else if(switch_counterd == 0)
    {
        CLEAR_BIT(switchStatus,6);
    }
    //test up
    switch_counteru = ((switch_counteru<<1) & mask) | BIT_IS_SET(PIND,1);
    if(switch_counteru == mask)
    {
        SET_BIT(switchStatus,7); 
    }
    else if(switch_counteru == 0)
    {
        CLEAR_BIT(switchStatus,7);
    }
    //test left
    switch_counterl = ((switch_counterl<<1) & mask) | BIT_IS_SET(PINB,1);
    if(switch_counterl == mask)
    {
        SET_BIT(switchStatus,5); 
    }
    else if(switch_counterl == 0)
    {
        CLEAR_BIT(switchStatus,5);
    }
    //test right
    switch_counterr = ((switch_counterr<<1) & mask) | BIT_IS_SET(PIND,0);
    if(switch_counterr == mask)
    {
        SET_BIT(switchStatus,4); 
    }
    else if(switch_counterl == 0)
    {
        CLEAR_BIT(switchStatus,4);
    }
    //test right switch
    switch_counterb = ((switch_counterb<<1) & mask) | BIT_IS_SET(PINF,5);
    if(switch_counterb == mask)
    {
        SET_BIT(switchStatus,1); 
    }
    else if(switch_counterb == 0)
    {
        CLEAR_BIT(switchStatus,1);
    }
    //test left switch
    switch_countera = ((switch_countera<<1) & mask) | BIT_IS_SET(PINF,6);
    if(switch_countera == mask)
    {
        SET_BIT(switchStatus,2); 
    }
    else if(switch_countera == 0)
    {
        CLEAR_BIT(switchStatus,2);
    }
    //test centre
    switch_counterc = ((switch_counterc<<1) & mask) | BIT_IS_SET(PINB,0);
    if(switch_counterc == mask)
    {
        SET_BIT(switchStatus,3); 
    }
    else if(switch_counterc == 0)
    {
        CLEAR_BIT(switchStatus,3);
    }

}
ISR(TIMER1_OVF_vect) {

    update_fireworks();

    if (gameStatus != 0) // make sure its playing
    {
	overflow_counter1 ++;
    if(overflow_counter1 > 500/right_adc_value)
    { 
        //check which way the walls should be moving

        //inverts the wall
        if (!walls_backward && last_state_forward)
        {
            /* code */
        }
        else if (walls_backward && last_state_forward)
        {
            FIRSTWALL.dx *= -1;
            FIRSTWALL.dy *= -1;
            SECONDWALL.dx *= -1;
            SECONDWALL.dy *= -1;
            THIRDWALL.dx *= -1;
            THIRDWALL.dy *= -1;
            FOURTHWALL.dx *= -1;
            FOURTHWALL.dy *= -1;
            last_state_forward = false;
        }
        else if (!walls_backward && !last_state_forward)
        {
            FIRSTWALL.dx *= -1;
            FIRSTWALL.dy *= -1;
            SECONDWALL.dx *= -1;
            SECONDWALL.dy *= -1;
            THIRDWALL.dx *= -1;
            THIRDWALL.dy *= -1;
            FOURTHWALL.dx *= -1;
            FOURTHWALL.dy *= -1;
            last_state_forward = true;
        }
        
        
        
        


        for (int i = 0; i < FIRSTWALL.pixelCount; i++)
        {
            if(FIRSTWALL.pixels[i][0] + FIRSTWALL.dx == -1 && FIRSTWALL.pixels[i][1] + FIRSTWALL.dy == -1 )
            {
                FIRSTWALL.pixels[i][0] = FIRSTWALL.pixels[i][0] + LCD_X;
                FIRSTWALL.pixels[i][1] = FIRSTWALL.pixels[i][1] + LCD_Y;
            }
            else if (FIRSTWALL.pixels[i][0] + FIRSTWALL.dx == -1)
            {
                FIRSTWALL.pixels[i][0] = FIRSTWALL.pixels[i][0] + LCD_X;
            }
            else if (FIRSTWALL.pixels[i][1] + FIRSTWALL.dy == -1)
            {
                FIRSTWALL.pixels[i][1] = FIRSTWALL.pixels[i][1] + LCD_Y;
            }
            FIRSTWALL.pixels[i][0] = (FIRSTWALL.pixels[i][0] + FIRSTWALL.dx) % LCD_X;
            FIRSTWALL.pixels[i][1] = (FIRSTWALL.pixels[i][1] + FIRSTWALL.dy) % LCD_Y;

        }
        for (int i = 0; i < SECONDWALL.pixelCount; i++)
        {
            if(SECONDWALL.pixels[i][0] + SECONDWALL.dx == -1 && SECONDWALL.pixels[i][1] + SECONDWALL.dy == -1 )
            {
                SECONDWALL.pixels[i][0] = SECONDWALL.pixels[i][0] + LCD_X;
                SECONDWALL.pixels[i][1] = SECONDWALL.pixels[i][1] + LCD_Y;
            }
            else if (SECONDWALL.pixels[i][0] + SECONDWALL.dx == -1)
            {
                SECONDWALL.pixels[i][0] = SECONDWALL.pixels[i][0] + LCD_X;
            }
            else if (SECONDWALL.pixels[i][1] + SECONDWALL.dy == -1)
            {
                SECONDWALL.pixels[i][1] = SECONDWALL.pixels[i][1] + LCD_Y;
            }
            SECONDWALL.pixels[i][0] = (SECONDWALL.pixels[i][0] + SECONDWALL.dx) % LCD_X;
            SECONDWALL.pixels[i][1] = (SECONDWALL.pixels[i][1] + SECONDWALL.dy) % LCD_Y;

        }
        for (int i = 0; i < THIRDWALL.pixelCount; i++)
        {
            if(THIRDWALL.pixels[i][0] + THIRDWALL.dx == -1 && THIRDWALL.pixels[i][1] + THIRDWALL.dy == -1 )
            {
                THIRDWALL.pixels[i][0] = THIRDWALL.pixels[i][0] + LCD_X;
                THIRDWALL.pixels[i][1] = THIRDWALL.pixels[i][1] + LCD_Y;
            }
            else if (THIRDWALL.pixels[i][0] + THIRDWALL.dx == -1)
            {
                THIRDWALL.pixels[i][0] = THIRDWALL.pixels[i][0] + LCD_X;
            }
            else if (THIRDWALL.pixels[i][1] + THIRDWALL.dy == -1)
            {
                THIRDWALL.pixels[i][1] = THIRDWALL.pixels[i][1] + LCD_Y;
            }
            THIRDWALL.pixels[i][0] = (THIRDWALL.pixels[i][0] + THIRDWALL.dx) % LCD_X;
            THIRDWALL.pixels[i][1] = (THIRDWALL.pixels[i][1] + THIRDWALL.dy) % LCD_Y;

        }
        for (int i = 0; i < FOURTHWALL.pixelCount; i++)
        {
            if(FOURTHWALL.pixels[i][0] + FOURTHWALL.dx == -1 && FOURTHWALL.pixels[i][1] + FOURTHWALL.dy == -1 )
            {
                FOURTHWALL.pixels[i][0] = FOURTHWALL.pixels[i][0] + LCD_X;
                FOURTHWALL.pixels[i][1] = FOURTHWALL.pixels[i][1] + LCD_Y;
            }
            else if (FOURTHWALL.pixels[i][0] + FOURTHWALL.dx == -1)
            {
                FOURTHWALL.pixels[i][0] = FOURTHWALL.pixels[i][0] + LCD_X;
            }
            else if (FOURTHWALL.pixels[i][1] + FOURTHWALL.dy == -1)
            {
                FOURTHWALL.pixels[i][1] = FOURTHWALL.pixels[i][1] + LCD_Y;
            }
            FOURTHWALL.pixels[i][0] = (FOURTHWALL.pixels[i][0] + FOURTHWALL.dx) % LCD_X;
            FOURTHWALL.pixels[i][1] = (FOURTHWALL.pixels[i][1] + FOURTHWALL.dy) % LCD_Y;

        }
        if (!jerry_wall_check(jerry_x, jerry_y)) //wall has crushed jerry
        {
            jerry_x = jerry_start_x;
            jerry_y = jerry_start_y;
        }
          
        overflow_counter1=0;
    }
    overflow_counter11++;
    if (overflow_counter11 > left_adc_scaler * 5)
    {
        move_jerry();
        if (!pauseMode)
        {
            move_tom();
        }
        overflow_counter11 = 0;
    }
    

    }
}
ISR(TIMER3_OVF_vect) {
	overflow_counter3 ++;
}
void start_screen(){
    clear_screen();
    draw_string(10,1,"Jacob Lodge",FG_COLOUR);
    draw_string(10,9,"N9713069",FG_COLOUR);
    draw_string(10,20,"Cat and Mouse",FG_COLOUR);
    show_screen();
    int keypress = 1;
    while (keypress)
    {
        get_current_time(); //keep calling for the time so it resets
        if(BIT_IS_SET(switchStatus,1))
        {
        keypress = 0;
        gameStatus = 1;
        pauseMode = true;
        }
    }
    

}
void setup_jerry(){
    jerry_start_x = 0;
    jerry_start_y = STATUS_BAR_HEIGHT +1;
    jerry_x = jerry_start_x;
    jerry_y = jerry_start_y;
    jerry_lives = 5;
    //score = 0;
    level_score = 0;
}
void update_jerry_bitmap(){ //will take jerrys x and y and update the rest of the bitmap
    JERRY_IMG[0].x = jerry_x;
    JERRY_IMG[0].y = jerry_y;
    JERRY_IMG[1].x = jerry_x+1;
    JERRY_IMG[1].y = jerry_y;
    JERRY_IMG[2].x = jerry_x+2;
    JERRY_IMG[2].y = jerry_y;
    JERRY_IMG[3].x = jerry_x+2;
    JERRY_IMG[3].y = jerry_y+1;
    JERRY_IMG[4].x = jerry_x+2;
    JERRY_IMG[4].y = jerry_y+2;
    JERRY_IMG[5].x = jerry_x+1;
    JERRY_IMG[5].y = jerry_y+2;
    JERRY_IMG[6].x = jerry_x;
    JERRY_IMG[6].y = jerry_y+2;
    JERRY_IMG[7].x = jerry_x;
    JERRY_IMG[7].y = jerry_y+1;
}
void update_tom_bitmap(){
    TOM_IMG[0].x = tom_x;
    TOM_IMG[0].y = tom_y;
    TOM_IMG[1].x = tom_x + 1;
    TOM_IMG[1].y = tom_y;
    TOM_IMG[2].x = tom_x + 2;
    TOM_IMG[2].y = tom_y;
    TOM_IMG[3].x = tom_x + 3;
    TOM_IMG[3].y = tom_y;
    TOM_IMG[4].x = tom_x + 4;
    TOM_IMG[4].y = tom_y;
    TOM_IMG[5].x = tom_x;
    TOM_IMG[5].y = tom_y+1;
    TOM_IMG[6].x = tom_x+4;
    TOM_IMG[6].y = tom_y+1;
    TOM_IMG[7].x = tom_x;
    TOM_IMG[7].y = tom_y+2;
    TOM_IMG[8].x = tom_x+4;
    TOM_IMG[8].y = tom_y+2;
    TOM_IMG[9].x = tom_x;
    TOM_IMG[9].y = tom_y+3;
    TOM_IMG[10].x = tom_x+4;
    TOM_IMG[10].y = tom_y+3;
    TOM_IMG[11].x = tom_x+4;
    TOM_IMG[11].y = tom_y+4;
    TOM_IMG[12].x = tom_x+1;
    TOM_IMG[12].y = tom_y+4;
    TOM_IMG[13].x = tom_x+2;
    TOM_IMG[13].y = tom_y+4;
    TOM_IMG[14].x = tom_x+3;
    TOM_IMG[14].y = tom_y+4;
    TOM_IMG[15].x = tom_x;
    TOM_IMG[15].y = tom_y+4;
}
void setup(void){

    //new functions
    second_wall_setup();
    //end new functions
    //setup usb serial for debug WILL BLOCK THE APPLICATION
    
    setup_jerry();
    

    //setup the ADC
    adc_init();

    //seed the rng timer
    srand(adc_read(1));

    setup_tom();
    setup_firework();
    // this will block the setup
    clear_screen();
    setup_timer0();
    setup_timer1();
    setup_timer3();
    // Set the input
    CLEAR_BIT(DDRD,1); // joystick up
    CLEAR_BIT(DDRB,7); // joystick down
    CLEAR_BIT(DDRB,1); // joystick left
    CLEAR_BIT(DDRD,0); // joystick right
    CLEAR_BIT(DDRF,6); // BUTTON LEFT
    CLEAR_BIT(DDRD,5); // BUTTON RIGHT

    //Enable the ADC, Pot 0, Pot 1
    CLEAR_BIT(DDRF,0); //Pot 0
    CLEAR_BIT(DDRF,1); // Pot 1

    //Enable the LEDs as output
    SET_BIT(DDRB, 2);
    SET_BIT(DDRB, 3);
    //enable gloabal interrupts
    sei();
}


void setup_2(){
    //lets reset the score on level
    level_score = 0;
    tom_x = tom_start_x;
    tom_y = tom_start_y;
    jerry_x = jerry_start_x;
    jerry_y = jerry_start_y;
    //lets clear the traps
    for (int i = 0; i < 5; i++)
    {
        CLEAR_BIT(trapStatus,i);
        CLEAR_BIT(cheeseStatus,i);
    }
    //lets clear the cheese


}

bool DoorFreeFromWallCheese(int x, int y){

    int doorX[12];
    int doorY[12];

    doorX[0]=x;
    doorY[0]=y;
    doorX[1]=x+1;
    doorY[1]=y;
    doorX[2]=x+2;
    doorY[2]=y;
    doorX[3]=x;
    doorY[3]=y+1;
    doorX[4]=x+2;
    doorY[4]=y+1;
    doorX[5]=x;
    doorY[5]=y+2;
    doorX[6]=x+2;
    doorY[6]=y+2;
    doorX[7]=x;
    doorY[7]=y+3;
    doorX[8]=x+2;
    doorY[8]=y+3;
    doorX[9]=x;
    doorY[9]=y+4;
    doorX[10]=x+1;
    doorY[10]=y+4;
    doorX[11]=x+2;
    doorY[11]=y+4;
    for (int i = 0; i < 11; i++)
    {
        for (int j = 0; j < FIRSTWALL.pixelCount; j++)
        {
            if (FIRSTWALL.pixels[j][0] == doorX[i] && FIRSTWALL.pixels[j][1] == doorY[i])
            {
                return false;
            }
            
        }
        for (int j = 0; j < SECONDWALL.pixelCount; j++)
        {
            if (SECONDWALL.pixels[j][0] == doorX[i] && SECONDWALL.pixels[j][1] == doorY[i])
            {
                return false;
            }
        }
        for (int j = 0; j < THIRDWALL.pixelCount; j++)
        {
            if (THIRDWALL.pixels[j][0] == doorX[i] && THIRDWALL.pixels[j][1] == doorY[i])
            {
                return false;
            }
            
        }
        for (int j = 0; j < FOURTHWALL.pixelCount; j++)
        {
            if (FOURTHWALL.pixels[j][0] == doorX[i] && FOURTHWALL.pixels[j][1] == doorY[i])
            {
                return false;
            }
        }
        
    }
    return true;
    
    
}

void door_checker(){
    if (!BIT_IS_SET(cheeseStatus,7) && level_score > 4)
    {
        SET_BIT(cheeseStatus,7); //set the bit so it aint used

        while(true)
        {
        doorX = round(rand() % LCD_X-3);
        doorY = round(STATUS_BAR_HEIGHT + 1 + rand() % (LCD_Y - STATUS_BAR_HEIGHT - 4 ));
        if(DoorFreeFromWallCheese(doorX,doorY))
        {
            break;
        }
        }
    }

    if (level_score > 4)
    {
    //check if jerry is at the next door
    int jleft = jerry_x;
    int jright = jerry_x + 2;
    int jtop = jerry_y;
    int jbottom = jerry_y + 2;

    int dleft = doorX;
    int dright = doorX + 3;
    int dtop = doorY;
    int dbottom = doorY + 4;

    if( jleft <= dright &&
        jright >= dleft &&
        jtop <= dbottom &&
        jbottom >= dtop
    )
    {
        //lets goto the next level
        if (currentLevel == 1)
        {
            currentLevel = 2;
            CLEAR_BIT(cheeseStatus,7);
        }
        else if(currentLevel == 2)
        {
            gameOver = true;
        }
        
        
        
    }
    }
    

}

void draw_door(){
    if (level_score > 4)
    {
        draw_pixel(doorX,doorY,FG_COLOUR);
        draw_pixel(doorX+1,doorY,FG_COLOUR);
        draw_pixel(doorX+2,doorY,FG_COLOUR);

        draw_pixel(doorX,doorY+1,FG_COLOUR);
        draw_pixel(doorX+2,doorY+1,FG_COLOUR);
        
        draw_pixel(doorX,doorY+2,FG_COLOUR);
        draw_pixel(doorX+2,doorY+2,FG_COLOUR);
                
        draw_pixel(doorX,doorY+3,FG_COLOUR);
        draw_pixel(doorX+2,doorY+3,FG_COLOUR);

        draw_pixel(doorX,doorY+4,FG_COLOUR);
        draw_pixel(doorX+1,doorY+4,FG_COLOUR);
        draw_pixel(doorX+2,doorY+4,FG_COLOUR);
    }
    
}

void move_jerry(){

    if (BIT_IS_SET(switchStatus,6) || key_press == 115) { //down
        if (borderCheck(jerry_x,jerry_y+1,jerry_width,jerry_height) && jerry_wall_check(jerry_x,jerry_y+1))
        {
             jerry_y = jerry_y + 1;
             tom_and_jerry_touch();
             cheese_touch();
        }
    }
    if(BIT_IS_SET(switchStatus,7) || key_press == 119){ //up
        if (borderCheck(jerry_x,jerry_y-1,jerry_width,jerry_height) && jerry_wall_check(jerry_x,jerry_y-1))
        {
             jerry_y = jerry_y-1;
             tom_and_jerry_touch();
             cheese_touch();
        }
    }
    if(BIT_IS_SET(switchStatus,5) || key_press == 97){ //left
        if (borderCheck(jerry_x-1,jerry_y,jerry_width,jerry_height) && jerry_wall_check(jerry_x-1,jerry_y))
        {
             jerry_x = jerry_x - 1;
             tom_and_jerry_touch();
             cheese_touch();
        }
    }
    if(BIT_IS_SET(switchStatus,4) || key_press == 100){ //right
        if (borderCheck(jerry_x+1,jerry_y,jerry_width,jerry_height) && jerry_wall_check(jerry_x+1,jerry_y))
        {
             jerry_x = jerry_x + 1;
             tom_and_jerry_touch();
             cheese_touch();
        }
    }

    if((BIT_IS_SET(switchStatus,3) && BIT_IS_SET(switchStatus,0))||(BIT_IS_SET(switchStatus,0) && key_press == 102)){
        int holder = idle_fire_work();
        if (holder != -1)
        {
            prime_fire_work(holder);

        }
        


    }

    //door checker
    door_checker();
    //lets check for the milk bowl
    milk_checker();
    trap_touch();
    key_press = 0; //reset keypress

}

void draw_jerry(){


    for (int i = 0; i < number_of_jerry_pixels; i++)
    {
        draw_pixel(JERRY_IMG[i].x,JERRY_IMG[i].y,FG_COLOUR);
    }
}

void move_tom(){
    int new_x = round(tom_x + tom_dx);
    int new_y = round(tom_y + tom_dy);
    int bounced = 0;

    if (!tom_wall_check(new_x,new_y))
    {
        tom_dy = -tom_dy;
        tom_dx = -tom_dx;
        bounced = 1;
        tom_dir = rand() * M_PI * 2 / RAND_MAX;
        tom_step = 0.1;
        tom_dx = tom_step * cos(tom_dir);
        tom_dy = tom_step * sin(tom_dir);
        tom_and_jerry_touch();
        trap_touch();
    }
    

    if (new_x < 1 || new_x > LCD_X - 5) {
        tom_dx = -tom_dx;
        bounced = 1;
        tom_dir = rand() * M_PI * 2 / RAND_MAX;
        tom_step = 0.1;
        tom_dx = tom_step * cos(tom_dir);
        tom_dy = tom_step * sin(tom_dir);
        tom_and_jerry_touch();
        trap_touch();
    }

    if (new_y < STATUS_BAR_HEIGHT + 1 || new_y > LCD_Y - 5){
        tom_dy = -tom_dy;
        bounced = 1;
        tom_dir = rand() * M_PI * 2 / RAND_MAX;
        tom_step = 0.1;
        tom_dx = tom_step * cos(tom_dir);
        tom_dy = tom_step * sin(tom_dir);
        tom_and_jerry_touch();
        trap_touch();
    }
    if(bounced == 0){
        tom_x += tom_dx;
        tom_y += tom_dy;
        tom_and_jerry_touch();
        trap_touch();
    }

}

void draw_tom(){
        for (int i = 0; i < number_of_tom_pixels; i++)
    {
        draw_pixel(TOM_IMG[i].x,TOM_IMG[i].y,FG_COLOUR);
    }
}

void draw_status_bar(){
    char buffer[88];

    draw_line(0,STATUS_BAR_HEIGHT,88,STATUS_BAR_HEIGHT,FG_COLOUR);
    //double time = get_current_time();
    //draw_formatted(40,0,buffer,sizeof(buffer), " T:%02d:%1.0f", time_min,time);
    draw_formatted(0,0,buffer,sizeof(buffer),"L%dL%dS%d",currentLevel,jerry_lives,score);
    draw_formatted(40,0,buffer,sizeof(buffer), " T:%02d:%1.0d", minPassed,secondsPassed);


}

void tom_and_jerry_touch(){
    int jleft = jerry_x;
    int jright = jerry_x + 2;
    int jtop = jerry_y;
    int jbottom = jerry_y + 2;

    int tleft = tom_x;
    int tright = tom_x + 4;
    int ttop = tom_y;
    int tbottom = tom_y + 4;

    if( jleft <= tright &&
        jright >= tleft &&
        jtop <= tbottom &&
        jbottom >= ttop
    )
    {           
                if (superJerry == 0)
                {
                jerry_lives--;
                jerry_x = jerry_start_x;
                jerry_y = jerry_start_y;

                tom_x = tom_start_x;
                tom_y = tom_start_y;
                }
                if (superJerry == 1)
                {
                score++;
                tom_x = tom_start_x;
                tom_y = tom_start_y;
                }
                


              

                //find whats at the address and purge it
    }

}

void collision_manager(){ //will hold all the functions for collisions
    tom_and_jerry_touch();
}

void read_adc(){
    left_adc = adc_read(0);
    left_adc_scaler = left_adc/100;

    right_adc = adc_read(1);
    right_adc_scaler = right_adc/100;

    if (right_adc <= 411 ) // give or take 100 for neutral
    {
        //min 0
        /* code */
        right_adc_scaler = ABS(411 - right_adc);
        walls_backward = true;
    }
    else if (right_adc >= 611 ) //give or take 100 for neutral
    {
        //max = 1024
        right_adc_scaler = ABS(611 - right_adc);
        /* code */
        walls_backward = false;
    }
    else
    {
        right_adc_scaler = 0;
        walls_backward = false;
    }  
    //vlaue
    right_adc_value = right_adc_scaler /10;
   
}

void game_over_screen(){
    
    bool keypress = false;
    gameStatus = 2;
    while (!keypress)
    {
        clear_screen();
        draw_string(2,10,"game over ",FG_COLOUR);
        draw_string(2,17,"press right ",FG_COLOUR);
        draw_string(2,30,"to restart ",FG_COLOUR);
        show_screen();
        if (BIT_IS_SET(PINF,5))
        {
            keypress =  true;
        }
        
    }
    //reset other stuff
        secondsPassed = 0;
        lastSecond = 0;
        minPassed = 0;
        CLEAR_BIT(switchStatus,1);
        for (int i = 0; i < 5; i++)
        {
        CLEAR_BIT(trapStatus,i);
        CLEAR_BIT(cheeseStatus,i);
        }
        score = 0;
        level_score = 0;
        currentLevel = 1;
}

void process(){
    if (currentLevel == 2 && lvl2Set)
    {
        readkeystroke(); //read key press
        get_current_time();
        read_adc();
        timer_update();
        clear_screen();
        input_checker();
        draw_status_bar();
        draw_jerry();
        draw_tom();
        draw_walls();
        draw_cheese();
        draw_traps();
        draw_door();
        update_fireworks();
        draw_fireworks();
        drawMilk();
        //debug
        move_jerry();
        if (!pauseMode)
        {
        move_tom();
        }
        update_jerry_bitmap();
        update_tom_bitmap();
        collision_manager();
        show_screen();
    }
    else if(currentLevel == 2 && !lvl2Set)
    {
        setup_2();
        lvl2Set = true;
    }
    else{
    readkeystroke(); //read key press
    get_current_time();
    read_adc();
    timer_update();
    clear_screen();
    input_checker();
    draw_status_bar();
    draw_jerry();
    draw_tom();
    draw_walls();
    draw_cheese();
    draw_traps();
    draw_door();
    update_fireworks();
    draw_fireworks();
    drawMilk();
    //debug
    move_jerry();
    if (!pauseMode)
    {
        move_tom();
    }
    update_jerry_bitmap();
    update_tom_bitmap();
    collision_manager();
    show_screen();
    }
}

//---- Main ----

int main(void){
    set_clock_speed(CPU_8MHz); //set the clock speed
    lcd_init(LCD_DEFAULT_CONTRAST);
    setup_usb_serial();
    //for the restart
    while (true)
    {
    setup(); //setup the teensy
    start_screen();
        while (!gameOver)
       {
        process(); //game logic
        _delay_ms(50);
       }
       game_over_screen();
       gameStatus = 0;
       gameOver = false;
    }

    return 0;
}

//--- End Main ----


//helper functions
void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format,...)
{
		va_list args;
		va_start(args,format);
		vsnprintf(buffer,buffer_size,format,args);
		draw_string(x,y,buffer,FG_COLOUR);
}
void usb_serial_send(char * message) {
	usb_serial_write((uint8_t *) message, strlen(message));
}

void setup_usb_serial(void) {
	draw_string(10, 10, "Connect USB...", FG_COLOUR);
	show_screen();

	usb_init();

	while ( !usb_configured() ) {
		// Block until USB is ready.
	}

}