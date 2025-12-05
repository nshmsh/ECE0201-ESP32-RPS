#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Bounce2.h>

#include "esp_system.h" // library for ESP random number generator



// declaration for display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// declaration for buttons
#define BUTTON_READY 13
#define BUTTON_1_R 14
#define BUTTON_2_P 25
#define BUTTON_3_S 26

// declaration for OLED display connected using software SPI
#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     16
#define OLED_CS     22
#define OLED_RESET   4

// declaration for LEDS
#define LED_POWER 32 // game on/off
#define LED_PLAYER 33 // player wins
#define LED_COMPUTER 27 // computer wins

// declaration for button bounces
Bounce buttonReady = Bounce();
Bounce button1R = Bounce();
Bounce button2P = Bounce();
Bounce button3S = Bounce();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// declaration for enum states for the game
enum GameState {
  WELCOME,
  RULES,
  INPUT_ROUNDS,
  READY,
  COUNTDOWN,
  PLAYING_ROUND,
  SHOW_RESULT,
  WAIT_FOR_NEXT,
  GAME_OVER
};

// game will start at welcome state
GameState currentState = WELCOME;
// previous state will then be welcome
GameState previousState = GAME_OVER;


// game tracking variables (must be global b/c we need them throughout different stages!)
int totalRounds = 0;
int currentRound = 0;
int playerScore = 0;
int computerScore = 0;
int playerChoice = -1;
int computerChoice = -1;
String roundResult = "";

// time tracking for error checking
unsigned long inputStartTime = 0;
const unsigned long inputTimeout = 10000; // 10 seconds

void setup() {
  Serial.begin(115200);

  // seed random number for  the generator
  randomSeed(esp_random());

  // attach buttons to pin with pullup
  buttonReady.attach(BUTTON_READY, INPUT_PULLUP);
  buttonReady.interval(50);  // 50ms is the debounce time

  button1R.attach(BUTTON_1_R, INPUT_PULLUP);
  button1R.interval(50);

  button2P.attach(BUTTON_2_P, INPUT_PULLUP);
  button2P.interval(50);

  button3S.attach(BUTTON_3_S, INPUT_PULLUP);
  button3S.interval(50);

  // initialize the OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // clear the buffer
  display.clearDisplay();

  // setup LEDs
  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_PLAYER, OUTPUT);
  pinMode(LED_COMPUTER, OUTPUT);

  // turn on power LED
  digitalWrite(LED_POWER, HIGH);
}

void loop() {
  buttonReady.update();
  button1R.update();
  button2P.update();
  button3S.update();

  // check if state just changed
  if (currentState != previousState) {
    // state just changed! handle entry actions
    switch(currentState) {
      case WELCOME:
        displayWelcome();
        break;

      case RULES:
        displayRules();
        break;

      case INPUT_ROUNDS:
        promptInput();
        // start the timer for user input
        inputStartTime = millis();
        break;

      case READY:
        displayReadyMessage(totalRounds);
        break;

      case COUNTDOWN:
        // increment current round
        currentRound++;

        // call function to display the countdown
        displayRPSCountdown();
        break;
      
      case PLAYING_ROUND:
        // get the computer's choice 
        computerChoice = getComputerChoice();

        // start the timer for user input
        inputStartTime = millis();
        break;
      
      case SHOW_RESULT:
        // call the display result function to display winner of the round
        displayResult();

        // start a timer for display
        inputStartTime = millis();

        // light up winner LED
        if (roundResult == "Player") {
          digitalWrite(LED_PLAYER, HIGH);
        }
        else if (roundResult == "Computer") {
          digitalWrite(LED_COMPUTER, HIGH);
        }
        // for tie, no LED
        break;

      case WAIT_FOR_NEXT:
        // display press 1 when ready
        readyForNext();
        break;
      
      case GAME_OVER:
        // display final scores
        endingGame(playerScore, computerScore);
    }

    // update previous
    previousState = currentState;
  }
  
  // handle ongoing state logic
  switch(currentState) {
    case WELCOME:
      currentState = RULES;
      break;

    case RULES:
      // wait for button press before going to next state
      if (buttonReady.fell()) { 
        currentState = INPUT_ROUNDS;
      }
      break;

    case INPUT_ROUNDS:
      // gets number of rounds
      totalRounds = getNumRounds();

      // error check the number of rounds
      if (totalRounds > 0) {
          currentState = READY;
      }
      // if 0 for more than 10 seconds, reprompt the player
      else if (millis() - inputStartTime >= inputTimeout) {
      // shows prompt again
      promptInput();
      // restart the timer
      inputStartTime = millis();
      }
      break;

    case READY:
      // if the ready button was pressed, go to countdown state
      if (buttonReady.fell()) {
          currentState = COUNTDOWN;
      }
      break;

    case COUNTDOWN:
      currentState = PLAYING_ROUND;
      break;
    
    case PLAYING_ROUND:
      playerChoice = getPlayerChoice();

      // for valid choice
      if (playerChoice > 0) {
        // determine winner
        roundResult = determineWinner(playerChoice, computerChoice);
      
        // update scores
        if (roundResult == "Player") {
          playerScore++;
        }
        else if (roundResult == "Computer") {
          computerScore++;
        }

        // force early win if someone wins 2 from best out of 3
        if (totalRounds == 3 && (playerScore >= 2 || computerScore >= 2)) {
          currentRound = totalRounds;
        }
        // force early win if someone wins 3 from best out of 5
        if (totalRounds == 5 && (playerScore >= 3 || computerScore >= 3)) {
          currentRound = totalRounds;  // Force game to end after showing result
        }
        // update state 
        currentState = SHOW_RESULT;
      }

    break;

    case SHOW_RESULT:
      // delays transition to next state by 3 seconds
      if (millis() - inputStartTime >= 3000) {
        // turn off both winner LEDs
        digitalWrite(LED_PLAYER, LOW);
        digitalWrite(LED_COMPUTER, LOW);
        
        // check if game is over
        if (currentRound >= totalRounds) {
          currentState = GAME_OVER;
        }
        else {
          currentState = WAIT_FOR_NEXT;
        }
      }
      break;

    case WAIT_FOR_NEXT:
      if (buttonReady.fell()) {
      // check if all rounds are done
        if (currentRound >= totalRounds) {
          currentState = GAME_OVER;
        }
        // start next round if not
        else {
          currentState = COUNTDOWN; 
        }
      }
      break;

    case GAME_OVER:
      if (buttonReady.fell()) {
      // reset game variables
      totalRounds = 0;
      currentRound = 0;
      playerScore = 0;
      computerScore = 0;
      playerChoice = -1;
      computerChoice = -1;
      roundResult = "";
      
      // go straight to round selection
      currentState = INPUT_ROUNDS;
      }
      break;

  }
}


// ------------------------ FUNCTIONS!!! ------------------------ //

void displayWelcome() {
  // show "Hello!" first
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(34, 0);
  display.println("Hello!");
  display.display();   
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();

  // show welcome to..
  display.clearDisplay();
  display.setCursor(0,20);
  display.println("Welcome");
  display.println("to...");
  display.display();
  delay(2000); 
  
  // then show rock paper scissors
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Rock,");
  display.println("Paper,");
  display.println("Scissors!");
  display.display();
  delay(2000);
}

void displayRules() {
  // show rules:
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("RULES:");
  display.display();
  delay(2000);

  // display the actual rules, one by one
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Rock beats scissors");
  display.println("Paper beats rock");
  display.println("Scissors beats paper");

  // tell player to press button1 to continue
  display.println();
  display.println("Press #1");
  display.println("to continue!");
  display.display();

}

void promptInput() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20); 
  display.println("Press #2 for 1 round");
  display.println("Press #3 for 3 rounds");
  display.println("Press #4 for 5 rounds");
  display.display();
}

// displays the number of rounds and ready message
void displayReadyMessage(int rounds) {
  if (rounds == 1) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("You chose:");
    display.print(rounds);
    display.print(" round");
    display.display();
    delay(1500);
  }
  else {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("You chose:");
    display.print(rounds);
    display.print(" rounds");
    display.display();
    delay(1500);
  }

  // tell player to press button1 to continue
  display.setTextSize(1);
  display.println();
  display.println();
  display.println();
  display.println("Press #1 to ");
  display.println("start round!");
  display.display();
}

// displays ROCK PAPER SCISSORS SHOOT! after the player is ready
void displayRPSCountdown() {

  // ROCK
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.println("ROCK");
  display.display();
  delay(1000);

  // PAPER
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.println("PAPER");
  display.display();
  delay(1000);

  // SCISSORS
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.println("SCISSORS");
  display.display();
  delay(1000);

  // SHOOT!
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(10, 20);
  display.println("SHOOT!");
  display.display();

  // tell player to press their choice now
  display.println();
  display.setTextSize(1);
  display.setCursor(10, 45);
  display.println("Choose now!");
  display.display();
  delay(1000);
}

// function for translating choice from int to string
String choiceToString(int choice) {
  // if choice is 1, return "Rock"
  if (choice == 1) return "Rock";
  // if choice is 2, return "Paper"
  if (choice == 2) return "Paper";
  // if choice is 3, return "Scissors"
  if (choice == 3) return "Scissors";
  return "Unknown Error...";
}

// function for getting number of rounds
int getNumRounds() {
  // button 1 means 1 round
  if (button1R.fell()) return 1;  
  // button 2 means 3 rounds
  if (button2P.fell()) return 3;
  // button 3 means 5 rounds
  if (button3S.fell()) return 5;
  // nothing inputted
  return 0;
}

// function for getting user's choice
int getPlayerChoice() {

  if (button1R.fell()) {
    return 1;  // rock
  }
  if (button2P.fell()) {
    return 2;  // paper
  }
  if (button3S.fell()) {
    return 3;  // scissors
  }
  
  return 0;  // no button pressed this loop
}

// function for getting computer's choice
int getComputerChoice() {
  // returns the random number
  return random(1, 4);
}

// function for determining winner
String determineWinner(int player, int computer) {
  // first check for tie
  if (player == computer) {
    return "Tie";
  }
  
  // check when PLAYER wins
  // rock beats scissors
  if (player == 1 && computer == 3) {
    return "Player";
  }
  // paper beats rock
  else if (player == 2 && computer == 1) {
    return "Player";
  }
  // scissors beats paper
  else if (player == 3 && computer == 2) {
    return "Player";
  }
  // if none of those cases, computer must've won
  else {
    return "Computer";
  }
}

// display result function
void displayResult() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  
  display.print("You chose: "); 
  display.println(choiceToString(playerChoice));
  
  display.println("Computer chose: ");
  display.println(choiceToString(computerChoice));
  
  if (roundResult == "Tie") {
    display.println();
    display.println("Tie!");
  }
  else {
    display.println();
    display.print(roundResult);
    display.println(" wins!");
  }
  
  display.print("Score: ");
  display.print(playerScore);
  display.print("-");
  display.println(computerScore);
  display.display();
  delay(2000);

}

// waiting for next confirmation display
void readyForNext() {
  // tells the user to press 1 when ready for the next round
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("Press #1 when ready");
  display.println("for next round!");
  display.display();
}

// flashes winner led, displays final score and game over
void endingGame(int playerScore, int computerScore) {
    // flash winner LED 3 times
    int winnerLED;
    if (playerScore > computerScore) {
      winnerLED = LED_PLAYER;
    }
    else if (computerScore > playerScore) {
      winnerLED = LED_COMPUTER;
    }
    else {
      winnerLED = -1;  // tie - no LED
    }
    
    // flash 3 times if there's a winner
    if (winnerLED != -1) {
      for (int i = 0; i < 3; i++) {
        digitalWrite(winnerLED, HIGH);
        delay(500);
        digitalWrite(winnerLED, LOW);
        delay(500);
      }
    }

    // display the final score
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 10);
    display.println("FINAL SCORE:");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Player: ");
    display.println(playerScore);
    display.print("Computer: ");
    display.println(computerScore);
    display.display();
    delay(5000);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);

    if (computerScore > playerScore) {
      display.println("The winneris...");
      display.println();
      display.println("Computer:(");
      display.display();
    }
    else if (computerScore < playerScore) {
      display.println("The winneris...");
      display.println();
      display.println("Player!!");
      display.display();
    }
    else {
      display.println("It was a..");
      display.println();
      display.println("TIE!!");
      display.display();
    }
    delay(8000);

    // thank user for playing
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,10);
    display.println("Thank you for");
    display.println("playing!");
    display.display();
    delay(5000);

    // ask if they want to play again
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Press #1"); 
    display.println("to play");
    display.println("again!");
    display.display();
}
