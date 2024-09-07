#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

// The game state can be used to detect what happens on the playfield
#define GAMEOVER   0
#define ACTIVE     (1 << 0)
#define ROW_CLEAR  (1 << 1)
#define TILE_ADDED (1 << 2)


#define MAX_NAME_LEN 256
#define MATRIX_DIM1 8
#define MATRIX_DIM2 8
#define MATRIX_LENGHT (MATRIX_DIM1 * MATRIX_DIM2* sizeof(u_int16_t))

#define RED 0xf000
#define GREY 0x3182
#define BROWN 0x8800
#define YELLOW 0xffe0
#define ORANGE 0xfb40
#define LIGHT_GREEN 0xbee0
#define PURPLE 0xb81f
#define VIOLET 0xb80c
#define DARK_GREEN 0x034c
#define BLUE 0x0356
#define MINT 0xF7FE
#define PINK 0xe974


//Used to retrieve unchangeable information about the frame buffer device 
struct fb_fix_screeninfo screeninfo;
int n_devices = 10;
//Pointer to the mapped memory of the frame buffer
u_int16_t *fb_pointer;

struct input_event event;
//Frame buffer file descriptor
int fbd;
//Joystick file descriptor
int joystick;
//Global variable used in case "chooseNextColor" instead of "chooseColor"
int currentColor = 0;

u_int16_t colors[] = {RED, GREY, BROWN, YELLOW, ORANGE, LIGHT_GREEN, PURPLE, VIOLET, DARK_GREEN, BLUE,  PINK};

int saved_output;

typedef struct {
  bool occupied;
  u_int16_t color;
} tile;

typedef struct {
  unsigned int x;
  unsigned int y;
} coord;

typedef struct {
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile;                       // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;



gameConfig game = {
                   .grid = {8, 8},
                   .uSecTickTime = 10000,
                   .rowsPerLevel = 2,
                   .initNextGameTick = 50,
};


bool mapFrameBuffer(){
  //Maps the frame buffer pointer fb_pointer to the frame buffer of the LED matrix
  fb_pointer= mmap(NULL, MATRIX_LENGHT, PROT_WRITE, MAP_SHARED, fbd, 0);
  
  if (fb_pointer == MAP_FAILED){
    printf("Error in mapping the frame buffer");
    return false;
  }else{
    printf("Frame buffer mapped correcly");
    return true;
  }
}

//Selects a color randomly among the available ones in "colors"
u_int16_t chooseColor(){
  int lenght= sizeof(colors) / sizeof(colors[0]);
  //Generates a random number from 0 to lenght - 1, and uses it as an index
  int randomNumber = rand() % (lenght);
  return colors[randomNumber];
}

//Otherwise it is possible to use this function to select the colors in a iterative way using the global variable
u_int16_t chooseNextColor(){
  int lenght= sizeof(colors) / sizeof(colors[0]);
  currentColor++;
  currentColor = currentColor % lenght;
  return colors[currentColor];
}

bool identifyFrameBuffer(){
    for (int i=0; i < n_devices; i++) {
    //this allows us to identify the framebuffer device
        char buffer [20];
        snprintf(buffer, sizeof(buffer), "/dev/fb%d", i);
        fbd = open(buffer, O_RDWR);
        if (fbd == -1){ continue;//continue in the loop checking the next device
            }else{
              //We use FBIOGET_FSCREENINFO to retrieve information about the frame buffer device
              //In this case the output is a negative value, therefore an error occured
                if (ioctl(fbd, FBIOGET_FSCREENINFO, &screeninfo) < 0){
                    return false;
            }else{
        //Checks the device name
                if (strcmp(screeninfo.id, "RPi-Sense FB") == 0) {
                    printf("The framebuffer device is 'RPi-Sense FB'\n");
                    return true;
                    break;
                }else{
                    return false;
                }
            }
        }
    }  
   //If it has terminated the loop it means it hasn't found the device
   return false;
}


bool identifyJoystick() {
    int n_events = 10;
    char device[20];
    char name[MAX_NAME_LEN];
   
    for (int i = 0; i < n_events; i++) {
      //Checks if the joystick is among the 10 events 
        snprintf(device, sizeof(device), "/dev/input/event%d", i);
        joystick = open(device, O_RDONLY);  
        
        if (joystick == -1) {
            continue; //continue in the loop checking the next event
        } else {
            //Gets the device name, return a negative values if there is an error
            if (ioctl(joystick, EVIOCGNAME(MAX_NAME_LEN), name) < 0) {
                printf("Error in reading the device name");
                continue;
            } else {
              //Verifies the device name is "Raspberry Pi Sense HAT Joystick"
                printf("Device name: %s\n", name); 
                if (strcmp(name, "Raspberry Pi Sense HAT Joystick") == 0) {
                    printf("The input device is 'RPi-Sense FB'\n");                   
                    return true;
                    break;
                } else {
                  //Continues checking the next device if present
                    continue;
                }
            }
        }
    }
    //If it has terminated the loop it means it hasn't found the device
    return false;
}

// This function is called on the start of your application

bool initializeSenseHat() {
  //Tries to open the framebuffer device
  //First it initializes the frame buffer
  bool bufferIdentified = identifyFrameBuffer();
  printf("%d\n", bufferIdentified);
  if(!bufferIdentified){
    printf("\n%d", bufferIdentified);
    return false;
  }else{
    //Now identifies the Raspbery Sense Hats
    bool joystickIdentified = identifyJoystick();
    printf("\n%d", joystickIdentified);

    if(joystickIdentified){
      //if it has been identified correctly it attemps to map the frame buffer
      bool isMapped = mapFrameBuffer();
      return isMapped;
      }else{
        printf("\nJoystick can't be identified");
        return false;
      }
    }     
  }


// Reset matrix to make it empty
static void resetMatrix(){
  memset(fb_pointer, 0, MATRIX_LENGHT);
}

//Unmap the frame buffer
static void unmapFb(){
  munmap(fb_pointer, MATRIX_LENGHT);
}

// This function is called when the application exits
void freeSenseHat() {
  //reset matrix
  resetMatrix();
  //unmap the frame buffer
  unmapFb();
  //close the frame buffer descriptor
  close(fbd);
  //close the Sense Hat descriptor
  close(joystick);
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed

int readSenseHatJoystick() {
  int ready;
  ssize_t s;
  struct input_event newEvent;
  struct pollfd pfds = {
       pfds.fd = joystick,
       pfds.events = POLLIN
  };

  int lkey = 0;
  ready = poll(&pfds, 1, 0);
  if (ready == -1) {
    printf("poll");
  }  
  
  if (pfds.revents & POLLIN) {
   
    s = read(pfds.fd, &newEvent, sizeof(newEvent));
    if (s == -1){
      printf("    read %zd bytes: %.*s\n", s, (int) s, newEvent);
    }
  }
  
  //The event type should be EV_KEY for keypress, but don't count twice a press (value=1) and a release (value=0)
  if(newEvent.type==EV_KEY && (newEvent.value==1 || newEvent.value==2)){
   //Each case corresponds to a different event code 
    switch (newEvent.code) {
      case 28: return KEY_ENTER;
      
      case 105: return KEY_LEFT;
      
      case 106: return KEY_RIGHT;
    
      case 108: return KEY_DOWN;

      case 103: return KEY_UP;     
    }   
  }
  return 0;
}

//For now assign colors randomly
u_int16_t setColor(coord newTile){
  //Chooses a color randomly and sets the tile with it
  u_int16_t color = chooseColor();
  game.playfield[newTile.y][newTile.x].color=color;
  
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}




// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfieldrenderSenseHatMatrix
void renderSenseHatMatrix(bool const playfieldChanged) {
  //Checks if the playfield has changed. If it hasn't there is nothing to display
  if (!playfieldChanged)
    return;
  
  //Color all the matrix in MINT (background color)
  for (unsigned int x = 0; x < game.grid.x; x ++) {
    for (unsigned int y = 0; y < game.grid.y; y++) {
      //Consider each possible tile
      coord const checkTile = {x, y};
      if (!tileOccupied(checkTile)) {
      //If the tile is not occupied it means it belongs to the background, therefore colors it in MINT
       game.playfield[checkTile.y][checkTile.x].color=MINT;
      } 
      //Assigns to the frame buffer led the color of the corresponding tile 
      fb_pointer[x + y * MATRIX_DIM2] = game.playfield[checkTile.y][checkTile.x].color;
    }   
  }
}

 





static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
}

static inline void copyTile(coord const to, coord const from) {
  
  memcpy((void *) &game.playfield[to.y][to.x], (void *) &game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  if(to==from){
    return;
  }
  memcpy((void *) &game.playfield[to][0], (void *) &game.playfield[from][0], sizeof(tile) * game.grid.x);
}

static inline void resetTile(coord const target) {
  memset((void *) &game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *) &game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}


static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}


static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}



bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    for (unsigned int y = game.grid.y - 1; y > 0; y--) {
      copyRow(y, y - 1);
    }
    resetRow(0);
    return true;
  }
  return false;
}

void advanceLevel() {
  game.level++;
  switch(game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}


bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch(key) {
      case KEY_LEFT:
        moveLeft();
        break;
      case KEY_RIGHT:
        moveRight();
        break;
      case KEY_DOWN:
        while (moveDown()) {renderSenseHatMatrix(playfieldChanged);};
        game.tick = 0;
        break;
      case KEY_UP:
        break;

      default:
        playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
          setColor(game.activeTile);
        } else {
          gameOver();
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    setColor(game.activeTile);
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
       .fd = STDIN_FILENO,
       .events = POLLIN
  };
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
 exit:
    switch (lkey) {
      case 10: return KEY_ENTER;
      case 65: return KEY_UP;
      case 66: return KEY_DOWN;
      case 67: return KEY_RIGHT;
      case 68: return KEY_LEFT;
    }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x ++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
      case 0:
        fprintf(stdout, "| Tiles: %10u\n", game.tiles);
        break;
      case 1:
        fprintf(stdout, "| Rows:  %10u\n", game.rows);
        break;
      case 2:
        fprintf(stdout, "| Score: %10u\n", game.score);
        break;
      case 4:
        fprintf(stdout, "| Level: %10u\n", game.level);
        break;
      case 7:
        fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
        break;
    default:
        fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}


inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  }

  saved_output = dup(1);
  // Allocate the playing field structure
  game.rawPlayfield = (tile *) malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile**) malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  
  // Start with gameOver
  gameOver();
  initializeSenseHat();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  }

  //test
  

  // Clear console, render first time
  fprintf(stdout, "\033[H\033[J");
  
  renderConsole(true);
  renderSenseHatMatrix(true);
  
  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick();
    if (!key)
      key = readKeyboard();
    if (key == KEY_ENTER)
      break;
    
    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    //Updates the Sense Hat matrix
    renderSenseHatMatrix(playfieldChanged);

    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }
 
  //Free all the allocated memory
  freeSenseHat();
  fflush(NULL);
  free(game.playfield);
  free(game.rawPlayfield);

  //Added reset command to restore shell configuration
  system("reset");
  return 0;
}
