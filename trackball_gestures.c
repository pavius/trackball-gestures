/**************************************************************************
 *
 * Trackball gesture project, 27/12/2009
 *
 *==========================================================================
 * 
 * Written by Eran "Pavius" Duchan. www.pavius.net.
 * Free to use and distribute
 *
 ***************************************************************************/

#include "sys.h"

// trackball states
typedef enum
{
    TB_STATE_IDLE = 0,
    TB_STATE_DEBOUCING,
    TB_STATE_FAST_PULSE_IN_PROGRESS,
    TB_STATE_SLOW_PULSE_IN_PROGRESS,

} TRACKBALL_STATE;

// trackball events
typedef enum
{
    TB_EVENT_NONE = 0,
    TB_SLOW_PULSE,
    TB_FAST_PULSE

} TRACKBALL_EVENT;

// trackball states
TRACKBALL_STATE trackballState = TB_STATE_IDLE;
int_8 trackballDirection = 0;
uint_8 xCountLog[500];
uint_8 xCountLogIdx = 0;
uint_8 xCounts;
uint_8 trackballMediumConsecutiveHits = 0;

// trackball constants
#define TRACKBALL_HIGH_PULSE_COUNT_THRESHOLD (80)
#define TRACKBALL_HIGH_PULSE_MEDIUM_COUNT_THRESHOLD (50)

// SSEG definitions
#define SSEG_SEGMENT_A (1 << 6)
#define SSEG_SEGMENT_B (1 << 0)
#define SSEG_SEGMENT_C (1 << 1)
#define SSEG_SEGMENT_D (1 << 2)
#define SSEG_SEGMENT_E (1 << 3)
#define SSEG_SEGMENT_F (1 << 5)
#define SSEG_SEGMENT_G (1 << 4)

// digit value array
uint_8 sseg_digitValues[4];

// current digit
uint_8 sseg_currentDigit = 0;

// digit values
const uint_8 sseg_digitLedMap[] = 
{
    /* 0 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_B | SSEG_SEGMENT_C | SSEG_SEGMENT_D | SSEG_SEGMENT_E | SSEG_SEGMENT_F),
    /* 1 */ ~(SSEG_SEGMENT_B | SSEG_SEGMENT_C),
    /* 2 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_B | SSEG_SEGMENT_D | SSEG_SEGMENT_E | SSEG_SEGMENT_G),
    /* 3 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_B | SSEG_SEGMENT_C | SSEG_SEGMENT_D | SSEG_SEGMENT_G),
    /* 4 */ ~(SSEG_SEGMENT_B | SSEG_SEGMENT_C | SSEG_SEGMENT_F | SSEG_SEGMENT_G),
    /* 5 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_C | SSEG_SEGMENT_D | SSEG_SEGMENT_F | SSEG_SEGMENT_G),
    /* 6 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_C | SSEG_SEGMENT_D | SSEG_SEGMENT_E | SSEG_SEGMENT_F | SSEG_SEGMENT_G),
    /* 7 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_B | SSEG_SEGMENT_C),
    /* 8 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_B | SSEG_SEGMENT_C | SSEG_SEGMENT_D | SSEG_SEGMENT_E | SSEG_SEGMENT_F | SSEG_SEGMENT_G),
    /* 9 */ ~(SSEG_SEGMENT_A | SSEG_SEGMENT_B | SSEG_SEGMENT_C | SSEG_SEGMENT_D | SSEG_SEGMENT_F | SSEG_SEGMENT_G),
};

// init seven segment
void sseg_init()
{
    // 1:16 and 1:16 scalars
    T2CON = 0b01111011;
    
    // set period
    PR2 = 2;
    
    // enable interrupts
    TMR2IE = 1; 
}    

// start displaying
void sseg_startDisplay()
{
    // start timer2
    TMR2ON = 1;
}    

// quad encoder counters
uint_8 encoderStatus = 0;
uint_8 encoderLastXStatus = 0, encoderCurrXStatus;
uint_8 xCountsP, xCountsN;
uint_8 segmentIntCounter = 40;

// interrupt
void interrupt intr_highIsr()
{
    // check tmr2
    if (TMR2IF)
    {
        //
        // Check quad encoder state
        //

        // read current
        encoderStatus = PORTB;

        // get current X
        encoderCurrXStatus = encoderStatus & 0x3;
        
        // check if anything has changed since last time on X axis
        if (encoderCurrXStatus != encoderLastXStatus)
        {
            // check direction
            if ((encoderCurrXStatus >> 1) != (encoderLastXStatus & 0x1)) xCountsP++;
            else xCountsN++;

            // save last status
            encoderLastXStatus = encoderCurrXStatus;
        }

        //
        // Light up the digits every 160 interrupts
        //
        
        // check counter
        if (segmentIntCounter == 0)
        {
            // select current digit
            PORTA = (1 << sseg_currentDigit);
    
            // load current digit
            PORTC = sseg_digitValues[sseg_currentDigit];
    
            // next digit
            sseg_currentDigit++;
    
            // and wrap
            if (sseg_currentDigit >= 4) sseg_currentDigit = 0;
            
            // reset segment counter
            segmentIntCounter = 40;
        }
        else
        {
            // decrement seg counter
            segmentIntCounter--;    
        }    
        
        // clear the interrupt
        TMR2IF = 0;
    }
}

// set a number to the seven segment
void sseg_setDisplay(uint_16 numberToDisplay)
{
    // init values
    uint_8 thousands, hundreds, tens, units;
    thousands = hundreds = tens = units = 0;

    // split number
    while (numberToDisplay >= 1000){thousands++; numberToDisplay -= 1000;}
    while (numberToDisplay >= 100){hundreds++; numberToDisplay -= 100;}
    while (numberToDisplay >= 10){tens++; numberToDisplay -= 10;}
    units = numberToDisplay;

    // set to display array
    sseg_digitValues[0] = sseg_digitLedMap[units];
    sseg_digitValues[1] = sseg_digitLedMap[tens];
    sseg_digitValues[2] = sseg_digitLedMap[hundreds];
    sseg_digitValues[3] = sseg_digitLedMap[thousands];
}    

// process events
BOOL trackball_processEvents(uint_8 xCountsPositive, uint_8 xCountsNegative, TRACKBALL_EVENT *xEvent)
{
    // whtehr event has occured
    BOOL eventOccured = FALSE;

    // zero out event and direction
    *xEvent = TB_EVENT_NONE;

    // get direction and absolute counts
    if (xCountsPositive || xCountsNegative)
    {
        if (xCountsPositive > xCountsNegative)
        {
            trackballDirection++;
            xCounts = xCountsPositive - xCountsNegative;
        }
        else
        {
            trackballDirection--;
            xCounts = xCountsNegative - xCountsPositive;
        }
    }
    else
    {
        // no counts detected
        xCounts = 0;    
    }    

    // debug log
    xCountLog[xCountLogIdx++] = xCounts;
    if (xCountLogIdx >= 500) xCountLogIdx = 0;

    // 
    // Idle state
    // 
    if (trackballState == TB_STATE_IDLE)
    {
        // check if value indicates fast pulse
        if (xCounts > TRACKBALL_HIGH_PULSE_COUNT_THRESHOLD)
        {
            // enter fast pulse state
            trackballState = TB_STATE_FAST_PULSE_IN_PROGRESS;
        }
        // is it smaller than fast pulse but greater than 0?
        else if (xCounts > 0)
        {
            // check if need to add to medium counter
            if (xCounts > TRACKBALL_HIGH_PULSE_MEDIUM_COUNT_THRESHOLD) trackballMediumConsecutiveHits++;

            // start debouncing
            trackballState = TB_STATE_DEBOUCING;
        }
    }
    // 
    // Debouncing state
    // 
    else if (trackballState == TB_STATE_DEBOUCING)
    {
        // check if value now indicates fast pulse
        if (xCounts > TRACKBALL_HIGH_PULSE_COUNT_THRESHOLD)
        {
            // enter fast pulse state
            trackballState = TB_STATE_FAST_PULSE_IN_PROGRESS;
        }
        // is it smaller than fast pulse but greater than 0?
        else if (xCounts > 0)
        {
            // check if need to add to medium counter
            if (xCounts > TRACKBALL_HIGH_PULSE_MEDIUM_COUNT_THRESHOLD)
            {
                // increment medium hit count
                trackballMediumConsecutiveHits++;

                // was previous count above medium as well?
                if (trackballMediumConsecutiveHits >= 2)
                {
                    // we have 2 Consecutive above medium hits. this is a fast pulse
                    trackballState = TB_STATE_FAST_PULSE_IN_PROGRESS;
                }
                else
                {
                    // first count was below medium, second is above. If third will
                    // be above as well, this is a fast pulse
                    trackballState = TB_STATE_SLOW_PULSE_IN_PROGRESS;
                }
            }
            else
            {
                // if trackballMediumConsecutiveHits is 1 then the first measurement was above medium
                // however, teh second one isn't so we must zero it out
                trackballMediumConsecutiveHits = 0;

                // 2 Consecutive slow pulse counts - enter slow pulse
                trackballState = TB_STATE_SLOW_PULSE_IN_PROGRESS;
            }
        }
        else
        {
            // noise in previous measurement, go back to idle
            trackballState = TB_STATE_IDLE;
        }
    }
    // 
    // Slow pulse state
    // 
    else if (trackballState == TB_STATE_SLOW_PULSE_IN_PROGRESS)
    {
        // check if value indicates fast pulse (could start slow
        // and then pulse fast)
        if (xCounts > TRACKBALL_HIGH_PULSE_COUNT_THRESHOLD)
        {
            // enter fast pulse state
            trackballState = TB_STATE_FAST_PULSE_IN_PROGRESS;
        }
        // check if count exceeds medium threshold
        else if (xCounts > TRACKBALL_HIGH_PULSE_MEDIUM_COUNT_THRESHOLD)
        {
            // increment Consecutive counter
            trackballMediumConsecutiveHits++;
            
            // check if we have 2 Consecutive counts > medium
            if (trackballMediumConsecutiveHits >= 2)
            {
                // too many medium hits - switch to high
                trackballState = TB_STATE_FAST_PULSE_IN_PROGRESS;
            }
        }
        // any counts?
        else if (xCounts > 0)
        {
            // must zero out Consecutive hits counter
            trackballMediumConsecutiveHits = 0;
        }
        // check if value went back to zero
        else if (xCounts == 0)
        {
            // declare slow pulse event
            *xEvent = TB_SLOW_PULSE;            

            // go back to idle state and 
            trackballState = TB_STATE_IDLE;
            
            // event happend
            eventOccured = TRUE;
        }
    }
    // 
    // Fast pulse state
    // 
    else if (trackballState == TB_STATE_FAST_PULSE_IN_PROGRESS)
    {
        // check if value went back to zero
        if (xCounts == 0)
        {
            // declare slow pulse event
            *xEvent = TB_FAST_PULSE;            

            // go back to idle state and 
            trackballState = TB_STATE_IDLE;

            // event happend
            eventOccured = TRUE;
        }
    }
    
    // return if event has occured
    return eventOccured;
}

// entry
void main()
{
    int_8 value = 0;
    uint_8 xp, xn, processCounter = 5;
    TRACKBALL_EVENT tbXEvent;
    int_8 direction;
    
    // initialize pic env
    sys_init();

    // init seven seg
    sseg_init();

    // clear all digits
    PORTC = 0xFF;

    // output on display port
    TRISC = 0x00;

    // initialize transistor values
    PORTA = 0x00;

    // output on transistor control
    TRISA = 0x00;

    // initialize input on quad enc
    TRISB = 0x0F;

    // set numbers
    sseg_digitValues[0] = sseg_digitLedMap[8];
    sseg_digitValues[1] = sseg_digitLedMap[9];
    sseg_digitValues[2] = sseg_digitLedMap[6];
    sseg_digitValues[3] = sseg_digitLedMap[7];

    // start displaying
    sseg_startDisplay();

    // forever
    while (1)
    {
        // copy value
        xp += xCountsP;
        xn += xCountsN;
        xCountsP = 0;
        xCountsN = 0;

        // every 100ms
        if (processCounter == 0)
        {
            // get event
            if (trackball_processEvents(xp, xn, &tbXEvent) == TRUE)
            {
                int_8 change;

                // get counts
                if (tbXEvent == TB_SLOW_PULSE) change = 1;
                else if (tbXEvent == TB_FAST_PULSE) change = 15;
    
                // check direction hits
                if (trackballDirection > 0) value += change;
                else                        value -= change; 

                // wrap
                if (value < 0) value = (value + 60);
                else if (value >= 60) value = (value - 60);

                // display
                sseg_setDisplay(value);

                // zero out direction
                trackballDirection = 0;
                trackballMediumConsecutiveHits = 0;
            }

            // reset process counter
            processCounter = 5;

            // zero accumulators
            xp = xn = 0;
        }    
        else
        {
            processCounter--;
        }    

        // wait a bit
        time_delayMs(10);
    }    
}