/**Game title: Shape shifter
 * 
 * Description: This is a simple game where the player controls a ship and must 
 * avoid asteroids. The player wins points by time, that is, the more time the 
 * player avoids the asteroids the more points he/she gets. The player loses 
 * the game if the ship crashes with one of the asteroids.
 * 
 * Mechanics: The game is called Shape Shifter because the pieces that make up 
 * the ship can be merged into one by bumping into the left and right edges of 
 * the screen, reducing the hitbox of the ship itself and making the game easier. 
 * However, the pieces can come apart if the ship bumps into the top and bottom 
 * edges of the screen, which obvously extends the hitbox and makes the game 
 * harder.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include "shape.h"
#include <abCircle.h>
#include "buzzer.h"

#define GREEN_LED BIT6

// Scoring
int score = 0;
int score_count = 0;

// Ship custom shape parts
AbRect shipBody = {abRectGetBounds, abRectCheck, {1,2}};
AbRect leftWing = {abRectGetBounds, abRectCheck, {1,2}};
AbRect rightWing = {abRectGetBounds, abRectCheck, {1,2}};

// Playing field
AbRectOutline fieldOutline = {
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
};

// Ship custom shape
Layer shipBodyLayer = {
    (AbShape *)&shipBody, 
    // Ship body at center and bottom of the screen
    {(screenWidth/2), (screenHeight/2)+50},
    // Last and next position
    {0,0}, {0,0}, 
    // Color of ship body set to white
    COLOR_WHITE, 
    0
};

Layer leftWingLayer = {
    (AbShape *)&leftWing, 
    // Left wing sligthly to the left and down of ship body 
    {(screenWidth/2)-3, (screenHeight/2)+53},
    {0,0}, {0,0}, 
    COLOR_WHITE, 
    &shipBodyLayer,
};

Layer rightWingLayer = {
    (AbShape *)&rightWing, 
    // Right wing slightly to the right and down of ship body
    {(screenWidth/2)+3, (screenHeight/2)+53},
    {0,0}, {0,0}, 
    COLOR_WHITE, 
    &leftWingLayer,
};

// Asteroids
Layer asteroid4 = {  
  (AbShape *)&circle9,
  // Asteroid 4 to the left and slightly above center of the screen
  {(screenWidth/2)-20, (screenHeight/2)-5},
  {0,0}, {0,0},
  // Color of asteroid set to white
  COLOR_WHITE,
  &rightWingLayer,
};
  

Layer asteroid3 = {
  (AbShape *)&circle8,
  // Asteroid 3 to the right and slightly below center of the screen  
  {(screenWidth/2)+10, (screenHeight/2)+5},
  {0,0}, {0,0},
  // Color of asteroid set to gray
  COLOR_GRAY,
  &asteroid4,
};

// Playing field as a layer
Layer fieldLayer = {
  (AbShape *) &fieldOutline,
  // Field layer at center of the screen
  {screenWidth/2, screenHeight/2},
  {0,0}, {0,0},				    
  COLOR_RED,
  &asteroid3,
};

Layer asteroid2 = {
  (AbShape *)&circle4,
  // Asteroid 2 at center of the screen
  {screenWidth/2, screenHeight/2},
  {0,0}, {0,0},				   
  COLOR_WHITE,
  &fieldLayer,
};

Layer asteroid1 = {
  (AbShape *)&circle14,
  // Asteroid 1 to the right and slightly below center of the screen
  {(screenWidth/2)+10, (screenHeight/2)+5},
  {0,0}, {0,0},
  COLOR_GRAY,
  &asteroid2,
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

// initial value of {0,0} will be overwritten 
MovLayer ml_shipBody = {&shipBodyLayer, {0,0}, 0};
MovLayer ml_leftWing = {&leftWingLayer, {0,0}, &ml_shipBody};
MovLayer ml_rightWing = {&rightWingLayer, {0,0}, &ml_leftWing};
MovLayer ml_asteroid4 = { &asteroid4, {1,1}, &ml_rightWing};
MovLayer ml_asteroid3 = { &asteroid3, {1,1}, &ml_asteroid4}; 
MovLayer ml_asteroid2 = { &asteroid2, {1,2}, &ml_asteroid3 }; 
MovLayer ml_asteroid1 = { &asteroid1, {2,1}, &ml_asteroid2 }; 

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  // Disable interrupts (GIE off)
  and_sr(~8);
  // For each moving layer
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) {
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  // Disable interrupts (GIE on)
  or_sr(8);

  // For each moving layer
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) {
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
	     // Probe all layers, in order
         probeLayer = probeLayer->next) {
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } // If probe check 
	} // For checking all layers at col, row
	lcd_writeColor(color); 
      } // For col
    } // For row
  } // For moving layer being updated
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
      }	// If outside of fence 
    } // For axis 
    ml->layer->posNext = newPos;
  } // For ml 
}


// Allow player to move the ship 
// Set velocity of ship 
void set_velocity(int axis, int v) {
    ml_shipBody.velocity.axes[axis] = v;
    ml_leftWing.velocity.axes[axis] = v;
    ml_rightWing.velocity.axes[axis] = v;
}

// Detect user input and move the ship depending on the switch being pressed 
void move_ship() {
    // Enable switch detection
    u_int switches = p2sw_read();
   
    // Switches being pressed
    unsigned char s1_pressed = (switches & 1) ? 0 : 1;
    unsigned char s2_pressed = (switches & 2) ? 0 : 1;
    unsigned char s3_pressed = (switches & 4) ? 0 : 1;
    unsigned char s4_pressed = (switches & 8) ? 0 : 1;
    
    // Direction of ship movement
    char left = 0;
    char up= 0;
    char down = 0;
    char right = 0;
    
    // Determine direction of ship movement based on the switch being pressed
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
    
    // Change velocity and direction of ship based on the switch being pressed
    if(left) {
        set_velocity(0,-2);
        advanceState();
    }
    
    else if(up) {
        set_velocity(1,-2);
        advanceState();
    }
    else if(down) {
        set_velocity(1,2);
        advanceState();
    }
    else if(right) {
        set_velocity(0,2);
        advanceState();
    }
    else {
        set_velocity(0,0);
    }
}

// Check the collisions between the ship and the asteroids
int checkCollisions() {   
    int gameOver = 0;
    // Ship hit box
    Region shipHitBox;
    // Bounds of ship hit box
    layerGetBounds(&rightWingLayer, &shipHitBox);
    
    // Asteroid vectors 
    Vec2 asteroid1_vec_1 = {asteroid1.posNext.axes[0], asteroid1.posNext.axes[1]};
    Vec2 asteroid2_vec_2 = {asteroid2.posNext.axes[0], asteroid2.posNext.axes[1]};
    Vec2 asteroid3_vec_3 = {asteroid3.posNext.axes[0], asteroid3.posNext.axes[1]};
    Vec2 asteroid4_vec_4 = {asteroid4.posNext.axes[0], asteroid4.posNext.axes[1]};
    Vec2 asteroids[4] = {asteroid1_vec_1, asteroid2_vec_2, asteroid3_vec_3, asteroid4_vec_4};
    int i; 
    
    // Check the collisions between the ship and the asteroids
    for(i = 0; i < 4; i++) {
        // Stop the movement of the ship and asteroids of the ship collides with the asteroids
        if(asteroids[i].axes[0] - 4 < shipHitBox.botRight.axes[0] &&
            asteroids[i].axes[0] + 4 > shipHitBox.topLeft.axes[0] &&
            asteroids[i].axes[1] - 4 < shipHitBox.botRight.axes[1] &&
            asteroids[i].axes[1] + 4 > shipHitBox.topLeft.axes[1]
            ) {
            // Stop ship
            ml_shipBody.velocity.axes[0] = 0;
            ml_leftWing.velocity.axes[0] = 0;
            ml_rightWing.velocity.axes[0] = 0;
            ml_shipBody.velocity.axes[1] = 0;
            ml_leftWing.velocity.axes[1] = 0;
            ml_rightWing.velocity.axes[1] = 0;
    
            // Stop Asteroids
            ml_asteroid1.velocity.axes[0] = 0;
            ml_asteroid2.velocity.axes[0] = 0;
            ml_asteroid3.velocity.axes[0] = 0;
            ml_asteroid4.velocity.axes[0] = 0;
            ml_asteroid1.velocity.axes[1] = 0;
            ml_asteroid2.velocity.axes[1] = 0;
            ml_asteroid3.velocity.axes[1] = 0;
            ml_asteroid4.velocity.axes[1] = 0;
        
            // display GAME OVER message
            drawString5x7(screenWidth/2-32,screenHeight/2+12, "GAME OVER", COLOR_WHITE, COLOR_BLACK);
            gameOver = 1;
            buzzerSetPeriod(1500);
            return gameOver;
        }
    }
    return gameOver;
}

// Background color
u_int bgColor = COLOR_BLACK;
// Boolean for whether screen needs to be redrawn
int redrawScreen = 1;
// Fence around playing field
Region fieldFence;


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  // Green led on when CPU on
    P1DIR |= GREEN_LED;		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  buzzerInit();
  shapeInit();
  p2sw_init(15);

  shapeInit();

  layerInit(&asteroid1);
  layerDraw(&asteroid1);
  

  layerGetBounds(&fieldLayer, &fieldFence);

  // Enable preiodic interrupt
  enableWDTInterrupts();
  // GIE (enable interrupts
  or_sr(0x8);
  
  for(;;) { 
    // Pause CPU if screen doesn't need updating 
    while (!redrawScreen) { 
        // Green led off witHo CPU
        P1OUT &= ~GREEN_LED;   
        /**< CPU OFF */
        or_sr(0x10);	      
    }
    // Green led on when CPU on 
    P1OUT |= GREEN_LED;       
    redrawScreen = 0;
    movLayerDraw(&ml_asteroid1, &asteroid1);
    move_ship();
    int gameOver = checkCollisions();
    // Display score 
    char score_str[4];
    itoa(score, score_str, 10);
    drawString5x7(screenWidth/2 - 25, 3 , "Score:    ", COLOR_WHITE, COLOR_BLACK);
    drawString5x7(screenWidth/2 + 11, 3 , score_str, COLOR_WHITE, COLOR_BLACK);
   
  }
}

// Watchdog timer interrupt handler. 15 interrupts/sec 
void wdt_c_handler()
{
  static short count = 0;
  // Green LED on when cpu on
  P1OUT |= GREEN_LED;
  count ++;
  // Increment score
  score_count++;
  if(score_count == 500 && checkCollisions() == 0) {
      score++;
      score_count = 0;
  }
  
  if (count == 20) {
    mlAdvance(&ml_asteroid1, &fieldFence);
    if (p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  // Green LED off when cpu off 
  P1OUT &= ~GREEN_LED;
}

