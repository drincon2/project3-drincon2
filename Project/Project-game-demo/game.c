/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include "shape.h"
#include <abCircle.h>

#define GREEN_LED BIT6

AbRect shipBody = {abRectGetBounds, abRectCheck, {1,2}};
AbRect leftWing = {abRectGetBounds, abRectCheck, {1,2}};
AbRect rightWing = {abRectGetBounds, abRectCheck, {1,2}};
 
//AbRect rect10 = {abRectGetBounds, abRectCheck, {5,5}}; /**< 10x10 rectangle */
//AbRArrow rightArrow = {abRArrowGetBounds, abRArrowCheck, 30};

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
};

/* SHIP */
Layer shipBodyLayer = {
    (AbShape *)&shipBody, 
    {(screenWidth/2), (screenHeight/2)+50},
    {0,0}, {0,0}, 
    COLOR_WHITE, 
    0
};

Layer leftWingLayer = {
    (AbShape *)&leftWing, 
    {(screenWidth/2)-3, (screenHeight/2)+53},
    {0,0}, {0,0}, 
    COLOR_WHITE, 
    &shipBodyLayer,
};

Layer rightWingLayer = {
    (AbShape *)&rightWing, 
    {(screenWidth/2)+3, (screenHeight/2)+53},
    {0,0}, {0,0}, 
    COLOR_WHITE, 
    &leftWingLayer,
};

/* ASTEROIDS */
Layer asteroid4 = {  
  (AbShape *)&circle9,
  {(screenWidth/2)-20, (screenHeight/2)-5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &rightWingLayer,
};
  

Layer asteroid3 = {		/**< Layer with an orange circle */
  (AbShape *)&circle8,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_GRAY,
  &asteroid4,
};


Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  &asteroid3,
};

Layer asteroid1 = {		/**< Layer with a red square */
  (AbShape *)&circle4,
  {screenWidth/2, screenHeight/2}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &fieldLayer,
};

Layer asteroid0 = {		/**< Layer with an orange circle */
  (AbShape *)&circle14,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_GRAY,
  &asteroid1,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */
MovLayer ml_shipBody = {&shipBodyLayer, {0,0}, 0};
MovLayer ml_leftWing = {&leftWingLayer, {0,0}, &ml_shipBody};
MovLayer ml_rightWing = {&rightWingLayer, {0,0}, &ml_leftWing};
MovLayer ml_asteroid4 = { &asteroid4, {1,1}, &ml_rightWing};
MovLayer ml_asteroid3 = { &asteroid3, {1,1}, &ml_asteroid4}; /**not all layers move */
MovLayer ml_asteroid1 = { &asteroid1, {1,2}, &ml_asteroid3 }; 
MovLayer ml_asteroid0 = { &asteroid0, {2,1}, &ml_asteroid1 }; 
//MovLayer ship = { &shipBodyLayer, {0,0}, &ml0}

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  



//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }	/**< if outside of fence */
      
      if(abCircleCheck(&circle14,&(asteroid0.pos), &(ml -> layer -> pos)) && axis == 1) {
        /*stop SHIP*/
        ml_shipBody.velocity.axes[0] = 0;
        ml_leftWing.velocity.axes[0] = 0;
        ml_rightWing.velocity.axes[0] = 0;
        ml_shipBody.velocity.axes[1] = 0;
        ml_leftWing.velocity.axes[1] = 0;
        ml_rightWing.velocity.axes[1] = 0;
        
        /*stop ASTEROIDS*/
        ml_asteroid0.velocity.axes[0] = 0;
        ml_asteroid1.velocity.axes[0] = 0;
        ml_asteroid3.velocity.axes[0] = 0;
        ml_asteroid4.velocity.axes[0] = 0;
        ml_asteroid0.velocity.axes[1] = 0;
        ml_asteroid1.velocity.axes[1] = 0;
        ml_asteroid3.velocity.axes[1] = 0;
        ml_asteroid4.velocity.axes[1] = 0;
        
        /*display GAME OVER */
         drawString5x7(screenWidth/2-32,screenHeight/2+12, "GAME OVER", COLOR_WHITE, COLOR_BLACK);
      }
    
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}


/* ALLOW PLAYER TO MOVE USING SWITCHES */
/* Set velocity of ship */
void set_velocity(int axis, int v) {
    ml_shipBody.velocity.axes[axis] = v;
    ml_leftWing.velocity.axes[axis] = v;
    ml_rightWing.velocity.axes[axis] = v;
}

/* Allow the player to move the ship by using the switches */
void move_ship() {
    u_int switches = p2sw_read();
   
    unsigned char s1_pressed = (switches & 1) ? 0 : 1;
    unsigned char s2_pressed = (switches & 2) ? 0 : 1;
    unsigned char s3_pressed = (switches & 4) ? 0 : 1;
    unsigned char s4_pressed = (switches & 8) ? 0 : 1;
    char left = 0;
    char up= 0;
    char down = 0;
    char right = 0;
    
    if(s1_pressed) {
        left = 1;
    }
    if(s2_pressed) {
        up = 1;
    }
    if(s3_pressed) {
        down = 1;
    }
    if(s4_pressed) {
        right = 1;
    }
    
    if(left) {
        set_velocity(0,-2);
    }
    
    else if(up) {
        set_velocity(1,-2);
    }
    else if(down) {
        set_velocity(1,2);
    }
    else if(right) {
        set_velocity(0,2);
    }
    else {
        set_velocity(0,0);
    }
}



u_int bgColor = COLOR_BLACK;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);

  shapeInit();

  layerInit(&asteroid0);
  layerDraw(&asteroid0);
  

  layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */
  
  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml_asteroid0, &asteroid0);
    move_ship();
    //checkCollisions();
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    mlAdvance(&ml_rightWing, &fieldFence);
    if (p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}

